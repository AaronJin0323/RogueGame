#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySources.h"
#include "Telemetry/DTTelemetryJson.h"

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

	static FString JsonText(const FString& In)
	{
		return FDTTelJson::Quote(In);
	}

	/** Source ids ordered by name, so the output never depends on registration order. */
	static TArray<uint16> IdsByName(TArray<uint16> Ids)
	{
		Ids.Sort([](uint16 A, uint16 B) { return FDTTelemetrySources::GetName(A) < FDTTelemetrySources::GetName(B); });
		return Ids;
	}

	/** {"Name":value,...} in source-name order, so the output is deterministic. */
	template <typename TGet>
	static FString JsonBySource(const TMap<uint16, FDTTelemetrySourceTotals>& Map, TGet Get)
	{
		TArray<uint16> Ids;
		Map.GetKeys(Ids);
		Ids = IdsByName(MoveTemp(Ids));
		FString Out = TEXT("{");
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			Out += FString::Printf(TEXT("%s%s:%lld"), Index ? TEXT(",") : TEXT(""), *JsonText(FDTTelemetrySources::GetName(Ids[Index])),
				static_cast<long long>(Get(Map[Ids[Index]])));
		}
		return Out + TEXT("}");
	}

	/** {"Name":value,...} of a per-step source map, in source-name order. */
	static FString JsonStepBySource(const TMap<uint16, int64>& Map)
	{
		TArray<uint16> Ids;
		Map.GetKeys(Ids);
		Ids = IdsByName(MoveTemp(Ids));
		FString Out = TEXT("{");
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			Out += FString::Printf(TEXT("%s%s:%lld"), Index ? TEXT(",") : TEXT(""), *JsonText(FDTTelemetrySources::GetName(Ids[Index])),
				static_cast<long long>(Map[Ids[Index]]));
		}
		return Out + TEXT("}");
	}

	static FString Rate(double Count, double Elapsed)
	{
		return FString::Printf(TEXT("%.3f"), Elapsed > 0.0 ? Count * DTConst::TEL_RATE_UNIT_S / Elapsed : 0.0);
	}

	/** A rate of shares (damage_taken_per_min): one more digit, since a hit is a few percent of max HP. */
	static FString ShareRate(double Share, double Elapsed)
	{
		return FString::Printf(TEXT("%.4f"), Elapsed > 0.0 ? Share * DTConst::TEL_RATE_UNIT_S / Elapsed : 0.0);
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
	ResolveSerials.Reset();
	StepThreatOrigins.Reset();
	StepGemsMerged = 0;
	GemsSpawned = 0;
	GemsCollected = 0;
	GemsLive = 0;
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
	WriteStepRecords();
	if (StepsDone > IntervalStartStep)
	{
		WriteInterval();
		ResetInterval();
	}
	JsonlOut += JsonHead(TEXT("run_end"), INDEX_NONE) + FString::Printf(
		TEXT(",\"reason\":%s,\"steps\":%lld,\"unowned_events\":%lld,\"mirror_desyncs\":%lld%s}\n"),
		*JsonText(Reason), static_cast<long long>(StepsDone), static_cast<long long>(UnownedEvents), static_cast<long long>(MirrorDesyncs),
		*RunEndExtras());
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
	if (!bRouted || !Players.IsValidIndex(Owner))
	{
		++UnownedEvents;
		return;
	}
	FPlayerInterval& P = Players[Owner];
	++P.Kills;
	++P.BySource.FindOrAdd(Source).Kills;
	++P.RunBySource.FindOrAdd(Source).Kills;
	++P.StepKills;
	++P.StepKillsBySource.FindOrAdd(Source);
	if (TtkSeconds >= 0.0f)
	{
		P.TtkSamples.Add(TtkSeconds); // the CSV percentile keeps every sample (R-TEL-03)
		P.StepTtkMin = P.StepTtkCount ? FMath::Min(P.StepTtkMin, TtkSeconds) : TtkSeconds;
		P.StepTtkMax = P.StepTtkCount ? FMath::Max(P.StepTtkMax, TtkSeconds) : TtkSeconds;
		P.StepTtkSum += TtkSeconds;
		++P.StepTtkCount;
	}
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

void FDTTelemetryCore::BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits, TConstArrayView<uint32> Serials)
{
	if (!bActive)
	{
		return;
	}
	CurrentStep = StepIndex;
	ResolveHits = Hits;
	ResolveSerials = Serials;
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
		if (Hit.EnemySerial != 0 && ResolveSerials.IsValidIndex(Slot) && ResolveSerials[Slot] != Hit.EnemySerial)
		{
			continue; // stale handle: the pool ignores it and emits nothing (R-ENM-23)
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
	ResolveSerials.Reset();
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
		if (Sample.XpTotals.IsValidIndex(Player))
		{
			P.XpTotal = Sample.XpTotals[Player];
		}
		// Threat actions that started this step within the threat ring of this player's hero (director.md, enemies.md).
		if (Sample.Heroes.IsValidIndex(Player))
		{
			const float RingSq = FMath::Square(static_cast<float>(DTConst::DIR_THREAT_RING_MAX_UU));
			for (const FVector2f& Origin : StepThreatOrigins)
			{
				P.ThreatActions += FVector2f::DistSquared(Sample.Heroes[Player], Origin) <= RingSq ? 1 : 0;
			}
		}
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

	StepThreatOrigins.Reset();
	WriteStepRecords();
	++StepsDone;
	CurrentStep = StepIndex + 1;
	if (StepsDone - IntervalStartStep >= IntervalSteps)
	{
		WriteInterval();
		ResetInterval();
	}
}

void FDTTelemetryCore::WriteStepRecords()
{
	using namespace DTTelemetryCoreLocal;
	for (int32 Player = 0; Player < Players.Num(); ++Player)
	{
		FPlayerInterval& P = Players[Player];
		if (P.StepHits > 0)
		{
			JsonlOut += JsonHead(TEXT("hit"), Player) + FString::Printf(TEXT(",\"hits\":%d,\"damage\":%lld,\"damage_by_source\":%s}\n"),
				P.StepHits, static_cast<long long>(P.StepDamage), *JsonStepBySource(P.StepDamageBySource));
			P.StepHits = 0;
			P.StepDamage = 0;
			P.StepDamageBySource.Reset();
		}
		if (P.StepKills > 0)
		{
			// Sums, not means, so a tool recombines exact means across steps; min/max only when a TTK was known.
			FString Line = JsonHead(TEXT("kill"), Player) + FString::Printf(TEXT(",\"kills\":%d,\"kills_by_source\":%s,\"ttk_n\":%d,\"ttk_sum_s\":%.4f"),
				P.StepKills, *JsonStepBySource(P.StepKillsBySource), P.StepTtkCount, P.StepTtkSum);
			if (P.StepTtkCount > 0)
			{
				Line += FString::Printf(TEXT(",\"ttk_min_s\":%.4f,\"ttk_max_s\":%.4f"), P.StepTtkMin, P.StepTtkMax);
			}
			JsonlOut += Line + TEXT("}\n");
			P.StepKills = 0;
			P.StepKillsBySource.Reset();
			P.StepTtkCount = 0;
			P.StepTtkSum = 0.0;
			P.StepTtkMin = 0.0f;
			P.StepTtkMax = 0.0f;
		}
		if (P.StepGems > 0 || P.StepXp > 0)
		{
			JsonlOut += FDTTelJson(JsonHead(TEXT("xp"), Player)).Int(TEXT("gems"), P.StepGems).Int(TEXT("xp"), P.StepXp).Line();
			P.StepGems = 0;
			P.StepXp = 0;
		}
	}
	if (StepGemsMerged > 0)
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("gems_merged"), INDEX_NONE)).Int(TEXT("count"), StepGemsMerged).Line();
		StepGemsMerged = 0;
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
	Row.DamageTakenShare = P.DamageTakenShare;
	Row.InputDevice = P.bHasInputDevice ? P.InputDevice : EDTTelInputDevice::Unknown;
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
			*ShareRate(R.DamageTakenShare, E), *Rate(R.Dashes, E), R.Level, R.LiveBullets, R.LiveEnemies,
			static_cast<long long>(R.RecycledBullets), static_cast<long long>(R.RefusedSpawns), *Rate(R.ThreatActions, E),
			R.bHasNearest ? *FString::Printf(TEXT("%.1f"), R.NearestEnemyUU) : TEXT(""),
			*Rate(R.Hits, E), *Rate(static_cast<double>(R.DamageDealt), E), Config.Seed, *CsvText(Config.Build),
			*CsvText(Config.Culture),
			*CsvText(Players[Player].bHasInputDevice ? FString(DTTelNames::InputDevice(R.InputDevice)) : Config.InputDevice));

		const FPlayerInterval& P = Players[Player];
		if (P.BySource.Num() > 0)
		{
			JsonlOut += JsonHead(TEXT("sources"), Player) + FString::Printf(TEXT(",\"interval\":%d,\"damage\":%s,\"kills\":%s,\"hits\":%s}\n"),
				IntervalIndex,
				*JsonBySource(P.BySource, [](const FDTTelemetrySourceTotals& T) { return T.Damage; }),
				*JsonBySource(P.BySource, [](const FDTTelemetrySourceTotals& T) { return static_cast<int64>(T.Kills); }),
				*JsonBySource(P.BySource, [](const FDTTelemetrySourceTotals& T) { return static_cast<int64>(T.Hits); }));
		}
		WriteIntervalRecords(Player);
	}
}

void FDTTelemetryCore::ResetInterval()
{
	for (FPlayerInterval& P : Players)
	{
		P.Kills = 0;
		P.Hits = 0;
		P.DamageDealt = 0;
		P.DamageTakenShare = 0.0;
		P.Dashes = 0;
		P.ThreatActions = 0;
		P.IntercessionDenied = 0;
		FMemory::Memzero(P.StatusApplied);
		P.Reactions.Reset();
		for (TPair<uint16, FWeaponInterval>& Weapon : P.Weapons)
		{
			Weapon.Value.Spawned = 0;
			Weapon.Value.Throttled = 0;
		}
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
