#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTTelemetryCoreLocal
{
	/** Nearest-rank percentile of Values (sorted in place); P in [0, 1]. */
	static float Percentile(TArray<float>& Values, float P)
	{
		check(Values.Num() > 0);
		Values.Sort();
		const int32 Rank = FMath::CeilToInt32(FMath::Clamp(P, 0.0f, 1.0f) * static_cast<float>(Values.Num()));
		return Values[FMath::Clamp(Rank - 1, 0, Values.Num() - 1)];
	}

	/** A CSV cell from free text: separators and quotes become '_'. */
	static FString CsvText(const FString& In)
	{
		FString Out = In;
		for (TCHAR& Ch : Out)
		{
			if (Ch == TEXT(',') || Ch == TEXT('"') || Ch == TEXT('\n') || Ch == TEXT('\r'))
			{
				Ch = TEXT('_');
			}
		}
		return Out;
	}

	/** A JSON string literal (quoted, escaped). */
	static FString JsonText(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 2);
		Out += TEXT('"');
		for (const TCHAR Ch : In)
		{
			if (Ch == TEXT('"') || Ch == TEXT('\\'))
			{
				Out += TEXT('\\');
				Out += Ch;
			}
			else if (Ch < TEXT(' '))
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Ch));
			}
			else
			{
				Out += Ch;
			}
		}
		Out += TEXT('"');
		return Out;
	}

	/** {"Name":value,...} in source-id order, so the output is deterministic. */
	template <typename TGet>
	static FString JsonBySource(const TMap<uint16, FDTTelemetrySourceTotals>& Map, TGet Get)
	{
		TArray<uint16> Ids;
		Map.GetKeys(Ids);
		Ids.Sort();
		FString Out = TEXT("{");
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			Out += FString::Printf(TEXT("%s%s:%lld"), Index ? TEXT(",") : TEXT(""), *JsonText(FDTTelemetrySources::GetName(Ids[Index])),
				static_cast<long long>(Get(Map[Ids[Index]])));
		}
		return Out + TEXT("}");
	}

	static FString Rate(double Count, double Elapsed)
	{
		return FString::Printf(TEXT("%.3f"), Elapsed > 0.0 ? Count * DTConst::TEL_RATE_UNIT_S / Elapsed : 0.0);
	}
}

const FString& FDTTelemetryCore::GetCsvHeader()
{
	static const FString Header = TEXT("interval,run_time_s,player,kills_per_min,swarm_ttk_s,damage_taken_per_min,dashes_per_min,level,"
		"live_bullets,live_enemies,recycled_bullets,refused_spawns,threat_actions_per_min,nearest_enemy_uu,hits_per_min,"
		"damage_dealt_per_min,seed,build,culture,input_device");
	return Header;
}

void FDTTelemetryCore::Start(const FDTTelemetryConfig& InConfig)
{
	using namespace DTTelemetryCoreLocal;
	Config = InConfig;
	Config.NumPlayers = FMath::Max(1, Config.NumPlayers);
	Config.StepSeconds = Config.StepSeconds > 0.0f ? Config.StepSeconds : 1.0f / DTConst::SIM_FIXED_HZ;
	Players.Reset();
	Players.SetNum(Config.NumPlayers);
	StepsDone = 0;
	CurrentStep = 0;
	IntervalStartStep = 0;
	IntervalSteps = FMath::Max<int64>(1, FMath::RoundToInt64(static_cast<double>(Config.IntervalSeconds) / Config.StepSeconds));
	IntervalIndex = 0;
	LastLiveBullets = LastLiveEnemies = 0;
	LastRecycledTotal = LastRefusedTotal = RecycledAtIntervalStart = RefusedAtIntervalStart = 0;
	bHaveCounterBase = false;
	MirrorSpawnStep.Reset();
	MirrorDead.Reset();
	ResolveHits.Reset();
	Pending.Reset();
	bInResolve = false;
	MirrorDesyncs = 0;
	UnownedEvents = 0;
	CsvOut.Reset();
	JsonlOut.Reset();
	bHeaderWritten = false;
	bActive = true;

	JsonlOut += JsonHead(TEXT("run_start"), INDEX_NONE) + FString::Printf(
		TEXT(",\"seed\":%d,\"players\":%d,\"culture\":%s,\"build\":%s,\"input_device\":%s,\"interval_s\":%.3f,\"step_s\":%.6f,\"wall\":%s}\n"),
		Config.Seed, Config.NumPlayers, *JsonText(Config.Culture), *JsonText(Config.Build), *JsonText(Config.InputDevice),
		Config.IntervalSeconds, Config.StepSeconds, *JsonText(Config.WallStart));
}

void FDTTelemetryCore::End(const FString& Reason)
{
	using namespace DTTelemetryCoreLocal;
	if (!bActive)
	{
		return;
	}
	WriteStepHits();
	if (StepsDone > IntervalStartStep)
	{
		WriteInterval();
		ResetInterval();
	}
	JsonlOut += JsonHead(TEXT("run_end"), INDEX_NONE) + FString::Printf(
		TEXT(",\"reason\":%s,\"steps\":%lld,\"unowned_events\":%lld,\"mirror_desyncs\":%lld}\n"),
		*JsonText(Reason), static_cast<long long>(StepsDone), static_cast<long long>(UnownedEvents), static_cast<long long>(MirrorDesyncs));
	bActive = false;
}

double FDTTelemetryCore::TimeOfCurrentStep() const
{
	return static_cast<double>(StepsDone) * Config.StepSeconds;
}

FString FDTTelemetryCore::JsonHead(const TCHAR* Kind, int32 Owner) const
{
	return FString::Printf(TEXT("{\"t\":%.4f,\"step\":%lld,\"kind\":\"%s\",\"owner\":%d"), TimeOfCurrentStep(),
		static_cast<long long>(CurrentStep), Kind, Owner);
}

void FDTTelemetryCore::OnRunEvent(const FDTGameEvent& Event, bool bRouted)
{
	if (!bActive)
	{
		return;
	}
	if (bInResolve)
	{
		Pending.Add({ Event, bRouted });
		return;
	}
	// Outside a bracketed resolve (other producers, tests): no hit list to read a source or a slot from.
	if (Event.Kind == EDTGameEventKind::Hit)
	{
		ApplyHit(Event.OwnerId, bRouted, FDTTelemetrySources::Unknown, Event.Amount);
	}
	else if (Event.Kind == EDTGameEventKind::Kill)
	{
		ApplyKill(Event.OwnerId, bRouted, FDTTelemetrySources::Unknown, -1.0f);
	}
}

void FDTTelemetryCore::ApplyHit(int32 Owner, bool bRouted, uint16 Source, int32 Amount)
{
	if (!bRouted || !Players.IsValidIndex(Owner))
	{
		++UnownedEvents;
		return;
	}
	FPlayerInterval& P = Players[Owner];
	++P.Hits;
	P.DamageDealt += Amount;
	++P.StepHits;
	P.StepDamage += Amount;
	P.StepDamageBySource.FindOrAdd(Source) += Amount;
	for (TMap<uint16, FDTTelemetrySourceTotals>* Map : { &P.BySource, &P.RunBySource })
	{
		FDTTelemetrySourceTotals& T = Map->FindOrAdd(Source);
		T.Damage += Amount;
		++T.Hits;
	}
}

void FDTTelemetryCore::ApplyKill(int32 Owner, bool bRouted, uint16 Source, float TtkSeconds)
{
	using namespace DTTelemetryCoreLocal;
	if (!bRouted || !Players.IsValidIndex(Owner))
	{
		++UnownedEvents;
		return;
	}
	FPlayerInterval& P = Players[Owner];
	++P.Kills;
	++P.BySource.FindOrAdd(Source).Kills;
	++P.RunBySource.FindOrAdd(Source).Kills;
	FString Line = JsonHead(TEXT("kill"), Owner) + TEXT(",\"source\":") + JsonText(FDTTelemetrySources::GetName(Source));
	if (TtkSeconds >= 0.0f)
	{
		P.TtkSamples.Add(TtkSeconds);
		Line += FString::Printf(TEXT(",\"ttk_s\":%.4f"), TtkSeconds);
	}
	JsonlOut += Line + TEXT("}\n");
}

void FDTTelemetryCore::OnEnemiesSpawned(int64 StepIndex, bool bCleared, int32 PoolNum)
{
	if (!bActive)
	{
		return;
	}
	CurrentStep = StepIndex;
	if (bCleared)
	{
		MirrorSpawnStep.Reset();
	}
	ResyncMirror(PoolNum);
}

void FDTTelemetryCore::ResyncMirror(int32 PoolNum)
{
	if (PoolNum < MirrorSpawnStep.Num())
	{
		++MirrorDesyncs; // the pool shrank outside ResolveHits: identities of the remaining slots are unknown
		MirrorSpawnStep.SetNum(PoolNum);
	}
	while (MirrorSpawnStep.Num() < PoolNum)
	{
		MirrorSpawnStep.Add(CurrentStep);
	}
	MirrorDead.Init(0, MirrorSpawnStep.Num());
}

void FDTTelemetryCore::BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits)
{
	if (!bActive)
	{
		return;
	}
	CurrentStep = StepIndex;
	ResolveHits = Hits;
	Pending.Reset();
	MirrorDead.Init(0, MirrorSpawnStep.Num());
	bInResolve = true;
}

void FDTTelemetryCore::EndResolve(int32 PoolNum)
{
	if (!bActive || !bInResolve)
	{
		return;
	}
	bInResolve = false;

	// Walk the hits as the pool did: a hit on a live slot emitted one Hit event, then one Kill if it killed (R-ENM-05/06).
	int32 Next = 0;
	bool bDesync = false;
	for (const FDTHit& Hit : ResolveHits)
	{
		const int32 Slot = Hit.EnemyIndex;
		if (!MirrorSpawnStep.IsValidIndex(Slot) || MirrorDead[Slot])
		{
			continue;
		}
		if (!Pending.IsValidIndex(Next) || Pending[Next].Event.Kind != EDTGameEventKind::Hit)
		{
			bDesync = true;
			break;
		}
		const FPendingEvent& HitEvent = Pending[Next++];
		ApplyHit(HitEvent.Event.OwnerId, HitEvent.bRouted, Hit.SourceId, HitEvent.Event.Amount);
		if (Pending.IsValidIndex(Next) && Pending[Next].Event.Kind == EDTGameEventKind::Kill)
		{
			const FPendingEvent& KillEvent = Pending[Next++];
			MirrorDead[Slot] = 1;
			const float Ttk = static_cast<float>(static_cast<double>(CurrentStep - MirrorSpawnStep[Slot]) * Config.StepSeconds);
			ApplyKill(KillEvent.Event.OwnerId, KillEvent.bRouted, Hit.SourceId, Ttk);
		}
	}
	// Anything not matched (desync) still counts, without a source or TTK.
	for (; Next < Pending.Num(); ++Next)
	{
		bDesync = true;
		const FPendingEvent& Left = Pending[Next];
		if (Left.Event.Kind == EDTGameEventKind::Hit)
		{
			ApplyHit(Left.Event.OwnerId, Left.bRouted, FDTTelemetrySources::Unknown, Left.Event.Amount);
		}
		else if (Left.Event.Kind == EDTGameEventKind::Kill)
		{
			ApplyKill(Left.Event.OwnerId, Left.bRouted, FDTTelemetrySources::Unknown, -1.0f);
		}
	}
	Pending.Reset();
	ResolveHits.Reset();
	if (bDesync)
	{
		++MirrorDesyncs;
	}

	// Compact like FDTEnemyPool::Compact: back to front, swap-remove (R-ENM-07).
	for (int32 Slot = MirrorSpawnStep.Num() - 1; Slot >= 0; --Slot)
	{
		if (MirrorDead[Slot])
		{
			MirrorSpawnStep.RemoveAtSwap(Slot, 1, EAllowShrinking::No);
			MirrorDead.RemoveAtSwap(Slot, 1, EAllowShrinking::No);
		}
	}
	if (MirrorSpawnStep.Num() != PoolNum)
	{
		++MirrorDesyncs;
	}
	ResyncMirror(PoolNum);
}

void FDTTelemetryCore::EndStep(int64 StepIndex, const FDTTelemetrySample& Sample)
{
	if (!bActive)
	{
		return;
	}
	CurrentStep = StepIndex;
	if (!bHaveCounterBase)
	{
		RecycledAtIntervalStart = Sample.RecycledBulletsTotal;
		RefusedAtIntervalStart = Sample.RefusedSpawnsTotal;
		bHaveCounterBase = true;
	}
	LastLiveBullets = Sample.LiveBullets;
	LastLiveEnemies = Sample.LiveEnemies;
	LastRecycledTotal = Sample.RecycledBulletsTotal;
	LastRefusedTotal = Sample.RefusedSpawnsTotal;

	for (int32 Player = 0; Player < Players.Num(); ++Player)
	{
		FPlayerInterval& P = Players[Player];
		P.Level = Sample.Levels.IsValidIndex(Player) ? Sample.Levels[Player] : 1;
		if (Sample.Heroes.IsValidIndex(Player) && Sample.Enemies.Num() > 0)
		{
			const FVector2f Hero = Sample.Heroes[Player];
			float Best = TNumericLimits<float>::Max();
			for (const FVector2f& Enemy : Sample.Enemies)
			{
				Best = FMath::Min(Best, FVector2f::DistSquared(Hero, Enemy));
			}
			P.NearestSquaredSamples.Add(Best);
		}
	}

	WriteStepHits();
	++StepsDone;
	CurrentStep = StepIndex + 1;
	if (StepsDone - IntervalStartStep >= IntervalSteps)
	{
		WriteInterval();
		ResetInterval();
	}
}

void FDTTelemetryCore::WriteStepHits()
{
	using namespace DTTelemetryCoreLocal;
	for (int32 Player = 0; Player < Players.Num(); ++Player)
	{
		FPlayerInterval& P = Players[Player];
		if (P.StepHits == 0)
		{
			continue;
		}
		TArray<uint16> Ids;
		P.StepDamageBySource.GetKeys(Ids);
		Ids.Sort();
		FString BySource = TEXT("{");
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			BySource += FString::Printf(TEXT("%s%s:%lld"), Index ? TEXT(",") : TEXT(""), *JsonText(FDTTelemetrySources::GetName(Ids[Index])),
				static_cast<long long>(P.StepDamageBySource[Ids[Index]]));
		}
		JsonlOut += JsonHead(TEXT("hit"), Player) + FString::Printf(TEXT(",\"hits\":%d,\"damage\":%lld,\"damage_by_source\":%s}}\n"),
			P.StepHits, static_cast<long long>(P.StepDamage), *BySource);
		P.StepHits = 0;
		P.StepDamage = 0;
		P.StepDamageBySource.Reset();
	}
}

FDTTelemetryRow FDTTelemetryCore::ComputeRow(int32 Player) const
{
	using namespace DTTelemetryCoreLocal;
	FDTTelemetryRow Row;
	if (!Players.IsValidIndex(Player))
	{
		return Row;
	}
	const FPlayerInterval& P = Players[Player];
	Row.Interval = IntervalIndex;
	Row.RunTimeSeconds = static_cast<double>(StepsDone) * Config.StepSeconds;
	Row.ElapsedSeconds = static_cast<double>(StepsDone - IntervalStartStep) * Config.StepSeconds;
	Row.Player = Player;
	Row.Kills = P.Kills;
	Row.Hits = P.Hits;
	Row.DamageDealt = P.DamageDealt;
	Row.DamageTaken = P.DamageTaken;
	Row.Dashes = P.Dashes;
	Row.ThreatActions = P.ThreatActions;
	Row.Level = P.Level;
	Row.LiveBullets = LastLiveBullets;
	Row.LiveEnemies = LastLiveEnemies;
	Row.RecycledBullets = LastRecycledTotal - RecycledAtIntervalStart;
	Row.RefusedSpawns = LastRefusedTotal - RefusedAtIntervalStart;
	if (P.TtkSamples.Num() > 0)
	{
		TArray<float> Copy = P.TtkSamples;
		Row.bHasTtk = true;
		Row.SwarmTtkSeconds = Percentile(Copy, DTConst::TEL_TTK_PERCENTILE);
	}
	if (P.NearestSquaredSamples.Num() > 0)
	{
		TArray<float> Copy = P.NearestSquaredSamples;
		Row.bHasNearest = true;
		Row.NearestEnemyUU = FMath::Sqrt(Percentile(Copy, DTConst::TEL_NEAREST_PERCENTILE));
	}
	return Row;
}

void FDTTelemetryCore::WriteInterval()
{
	using namespace DTTelemetryCoreLocal;
	if (!bHeaderWritten)
	{
		CsvOut += GetCsvHeader() + TEXT("\n");
		bHeaderWritten = true;
	}
	for (int32 Player = 0; Player < Players.Num(); ++Player)
	{
		const FDTTelemetryRow R = ComputeRow(Player);
		const double E = R.ElapsedSeconds;
		CsvOut += FString::Printf(TEXT("%d,%.3f,%d,%s,%s,%s,%s,%d,%d,%d,%lld,%lld,%s,%s,%s,%s,%d,%s,%s,%s\n"),
			R.Interval, R.RunTimeSeconds, R.Player, *Rate(R.Kills, E),
			R.bHasTtk ? *FString::Printf(TEXT("%.4f"), R.SwarmTtkSeconds) : TEXT(""),
			*Rate(static_cast<double>(R.DamageTaken), E), *Rate(R.Dashes, E), R.Level, R.LiveBullets, R.LiveEnemies,
			static_cast<long long>(R.RecycledBullets), static_cast<long long>(R.RefusedSpawns), *Rate(R.ThreatActions, E),
			R.bHasNearest ? *FString::Printf(TEXT("%.1f"), R.NearestEnemyUU) : TEXT(""),
			*Rate(R.Hits, E), *Rate(static_cast<double>(R.DamageDealt), E), Config.Seed, *CsvText(Config.Build),
			*CsvText(Config.Culture), *CsvText(Config.InputDevice));

		const FPlayerInterval& P = Players[Player];
		if (P.BySource.Num() > 0)
		{
			JsonlOut += JsonHead(TEXT("sources"), Player) + FString::Printf(TEXT(",\"interval\":%d,\"damage\":%s,\"kills\":%s,\"hits\":%s}\n"),
				IntervalIndex,
				*JsonBySource(P.BySource, [](const FDTTelemetrySourceTotals& T) { return T.Damage; }),
				*JsonBySource(P.BySource, [](const FDTTelemetrySourceTotals& T) { return static_cast<int64>(T.Kills); }),
				*JsonBySource(P.BySource, [](const FDTTelemetrySourceTotals& T) { return static_cast<int64>(T.Hits); }));
		}
	}
}

void FDTTelemetryCore::ResetInterval()
{
	for (FPlayerInterval& P : Players)
	{
		P.Kills = 0;
		P.Hits = 0;
		P.DamageDealt = 0;
		P.DamageTaken = 0;
		P.Dashes = 0;
		P.ThreatActions = 0;
		P.TtkSamples.Reset();
		P.NearestSquaredSamples.Reset();
		P.BySource.Reset();
	}
	IntervalStartStep = StepsDone;
	++IntervalIndex;
	RecycledAtIntervalStart = LastRecycledTotal;
	RefusedAtIntervalStart = LastRefusedTotal;
}

void FDTTelemetryCore::TakeOutput(FString& OutCsv, FString& OutJsonl)
{
	OutCsv = MoveTemp(CsvOut);
	OutJsonl = MoveTemp(JsonlOut);
	CsvOut.Reset();
	JsonlOut.Reset();
}
