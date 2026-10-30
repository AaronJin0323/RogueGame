#include "Director/DTDirectorCore.h"
#include "Core/DTConstantsGen.h"

namespace DTDirectorCoreLocal
{
	constexpr int32 NumRoles = static_cast<int32>(EDTEnemyRole::Count);
	/** Regular-mix roles in timeline column order (`w_swarm w_tank w_rusher w_turret`). */
	constexpr EDTEnemyRole MixRoles[4] = { EDTEnemyRole::Swarm, EDTEnemyRole::Tank, EDTEnemyRole::Rusher, EDTEnemyRole::Turret };

	static float RoleRadius(EDTEnemyRole Role)
	{
		return FDTEnemyRoleDef::Get(Role).Radius;
	}

	static FVector2f Dir(float Angle)
	{
		return FVector2f(FMath::Cos(Angle), FMath::Sin(Angle));
	}

	/** Spacing of event enemies along an arc: body to body plus the separation the role machine keeps (enemies.md). */
	static float ArcSpacing(EDTEnemyRole Role)
	{
		return 2.0f * RoleRadius(Role) + static_cast<float>(DTConst::ENM_SEPARATION_RADIUS_UU);
	}
}

FDTDirectorCore::FDTDirectorCore()
{
	BossSeconds = static_cast<float>(DTConst::RUN_LENGTH_TO_BOSS);
}

const TCHAR* FDTDirectorCore::BlockName(EDTDirBlock InBlock)
{
	switch (InBlock)
	{
	case EDTDirBlock::Harvest: return TEXT("Harvest");
	case EDTDirBlock::Threat: return TEXT("Threat");
	case EDTDirBlock::Breath: return TEXT("Breath");
	case EDTDirBlock::Boss: return TEXT("Boss");
	default: return TEXT("Idle");
	}
}

float FDTDirectorCore::HpScaleAt(double RunSeconds)
{
	return 1.0f + DTConst::SCALE_HP_PER_MIN * static_cast<float>(RunSeconds / 60.0);
}

float FDTDirectorCore::DmgScaleAt(double RunSeconds)
{
	return 1.0f + DTConst::SCALE_DMG_PER_MIN * static_cast<float>(RunSeconds / 60.0);
}

int32 FDTDirectorCore::GemValue(EDTEnemyRole Role, bool bElite)
{
	int32 Value = 0;
	switch (Role)
	{
	case EDTEnemyRole::Swarm: Value = DTConst::DROP_XP_SWARM; break;
	case EDTEnemyRole::Tank: Value = DTConst::DROP_XP_TANK; break;
	case EDTEnemyRole::Rusher: Value = DTConst::DROP_XP_RUSHER; break;
	case EDTEnemyRole::Turret: Value = DTConst::DROP_XP_TURRET; break;
	default: break; // the boss drops nothing here (boss.md)
	}
	return bElite ? Value * DTConst::DROP_XP_ELITE_MULT : Value;
}

float FDTDirectorCore::ShareCap(EDTEnemyRole Role)
{
	switch (Role)
	{
	case EDTEnemyRole::Tank: return DTConst::DIR_SHARE_MAX_TANK;
	case EDTEnemyRole::Rusher: return DTConst::DIR_SHARE_MAX_RUSHER;
	case EDTEnemyRole::Turret: return DTConst::DIR_SHARE_MAX_TURRET;
	default: return 1.0f; // Swarm has no share cap
	}
}

bool FDTDirectorCore::IsAtShareCap(EDTEnemyRole Role, int32 RoleAlive, int32 TotalAlive)
{
	const float Cap = ShareCap(Role);
	if (Cap >= 1.0f || TotalAlive <= DTConst::DIR_SHARE_FLOOR_ALIVE)
	{
		return false;
	}
	return static_cast<float>(RoleAlive) >= Cap * static_cast<float>(TotalAlive);
}

FVector2f FDTDirectorCore::ClampToArena(const FVector2f& Position, float Radius)
{
	const float Limit = FMath::Max(0.0f, static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU) - Radius);
	return FVector2f(FMath::Clamp(Position.X, -Limit, Limit), FMath::Clamp(Position.Y, -Limit, Limit));
}

void FDTDirectorCore::StartRun()
{
	StartRun(TConstArrayView<DTCat::FTimelineRow>(DTCat::TimelineArena1, DTCat::NumTimelineArena1));
}

void FDTDirectorCore::StartRun(TConstArrayView<DTCat::FTimelineRow> InRows)
{
	Rows = InRows;
	Time = 0.0;
	BossSeconds = static_cast<float>(DTConst::RUN_LENGTH_TO_BOSS);
	for (const DTCat::FTimelineRow& Row : Rows)
	{
		if (Row.Phase == DTCat::ETimelinePhase::Boss)
		{
			BossSeconds = Row.Start_s; // R-DIR-13: the Boss row; equal to C.RUN.LENGTH_TO_BOSS on the shipped data (tested)
			break;
		}
	}
	NextRow = 0;
	HarvestRow = INDEX_NONE;
	OpenRow = INDEX_NONE;
	PendingThreats.Reset();
	StartedEarly.Reset();
	BeatRow = INDEX_NONE;
	BeatId = INDEX_NONE;
	BeatEnd = 0.0;
	BreathEnd = 0.0;
	EventAlive = 0;
	EventQueued = 0;
	LastEventDeath = FVector2f::ZeroVector;
	Credit = 0.0f;
	DropIns.Reset();
	DropInProgress = 0.0f;
	bDropInDue = false;
	LastLandTime.Reset();
	Forced.Reset();
	Totals = FDTDirectorTotals();
	OpenBlock(EDTDirBlock::Harvest, 0.0f);
}

void FDTDirectorCore::Stop()
{
	Block = EDTDirBlock::Idle;
	DropIns.Reset();
	PendingThreats.Reset();
}

void FDTDirectorCore::Skip(float Seconds)
{
	Time += FMath::Max(0.0f, Seconds);
}

void FDTDirectorCore::ForceBlock(EDTDirBlock Wanted)
{
	Forced = Wanted;
}

EDTDirBanner FDTDirectorCore::GetBanner() const
{
	if (Time - Open.StartSeconds >= DTConst::HUD_BANNER_S)
	{
		return EDTDirBanner::None;
	}
	return Block == EDTDirBlock::Threat ? EDTDirBanner::Threat : Block == EDTDirBlock::Boss ? EDTDirBanner::BossIncoming : EDTDirBanner::None;
}

void FDTDirectorCore::OpenBlock(EDTDirBlock State, float RowStart)
{
	Block = State;
	Open = FDTTelDirBlock();
	Open.Phase = BlockName(State);
	Open.RowStartSeconds = RowStart;
	Open.StartSeconds = static_cast<float>(Time);
	Open.AlivePeak = AliveTotal;
	KillDistances.Reset();
	OpenRow = State == EDTDirBlock::Harvest ? HarvestRow : INDEX_NONE;
}

void FDTDirectorCore::CloseBlock(const TCHAR* EndedBy, IDTDirectorSink& Sink)
{
	Open.Block = Totals.Blocks++;
	Open.EndSeconds = static_cast<float>(Time);
	Open.EndedBy = EndedBy;
	Open.AlivePeak = FMath::Max(Open.AlivePeak, AliveTotal);
	if (KillDistances.Num() > 0)
	{
		// Median; the upper middle for an even count. Once per block, so a full sort is cheap enough.
		KillDistances.Sort();
		Open.KillHorizonP50UU = KillDistances[KillDistances.Num() / 2];
	}
	Sink.BlockEnded(Open);
}

bool FDTDirectorCore::HasLivingHero(const FDTDirectorStepIn& In) const
{
	const int32 Num = In.View ? In.View->HeroPositions.Num() : 0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if (!In.HeroAlive.IsValidIndex(Index) || In.HeroAlive[Index])
		{
			return true;
		}
	}
	return false;
}

int32 FDTDirectorCore::PickHero(const FDTDirectorStepIn& In) const
{
	const int32 Num = In.View ? In.View->HeroPositions.Num() : 0;
	int32 Living = 0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Living += (!In.HeroAlive.IsValidIndex(Index) || In.HeroAlive[Index]) ? 1 : 0;
	}
	if (Living == 0)
	{
		return INDEX_NONE;
	}
	int32 Pick = In.Rng->RandHelper(Living); // R-DIR-05: a living hero chosen uniformly
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if (!In.HeroAlive.IsValidIndex(Index) || In.HeroAlive[Index])
		{
			if (Pick-- == 0)
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

FVector2f FDTDirectorCore::HeroAt(const FDTDirectorStepIn& In, int32 Hero) const
{
	return In.View && In.View->HeroPositions.IsValidIndex(Hero) ? In.View->HeroPositions[Hero] : FVector2f::ZeroVector;
}

FVector2f FDTDirectorCore::OnRing(const FVector2f& Center, float MinR, float MaxR, FRandomStream& Rng) const
{
	const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
	const float Radius = Rng.FRandRange(MinR, MaxR);
	return Center + DTDirectorCoreLocal::Dir(Angle) * Radius;
}

bool FDTDirectorCore::DrawRole(const float* Weights, bool bShareCaps, FRandomStream& Rng, EDTEnemyRole& OutRole) const
{
	using namespace DTDirectorCoreLocal;
	float W[4];
	float Sum = 0.0f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const EDTEnemyRole Role = MixRoles[Index];
		const bool bExcluded = bShareCaps && IsAtShareCap(Role, Alive[static_cast<int32>(Role)], AliveTotal);
		W[Index] = bExcluded ? 0.0f : FMath::Max(0.0f, Weights[Index]);
		Sum += W[Index];
	}
	if (Sum <= 0.0f)
	{
		return false;
	}
	float X = Rng.GetFraction() * Sum;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (W[Index] <= 0.0f)
		{
			continue;
		}
		OutRole = MixRoles[Index];
		if (X < W[Index])
		{
			return true;
		}
		X -= W[Index];
	}
	return true; // rounding: the last role with weight
}

FDTEnemySpawn FDTDirectorCore::MakeSpawn(EDTEnemyRole Role, const FVector2f& Position) const
{
	FDTEnemySpawn Spawn;
	Spawn.Role = Role;
	Spawn.Position = ClampToArena(Position, DTDirectorCoreLocal::RoleRadius(Role));
	Spawn.HpScale = HpScaleAt(Time);   // R-DIR-10: stamped once, at the spawn's run time
	Spawn.DmgScale = DmgScaleAt(Time);
	return Spawn;
}

void FDTDirectorCore::Emit(FDTEnemySpawn Spawn, IDTDirectorSink& Sink)
{
	const int32 Role = static_cast<int32>(Spawn.Role);
	++Alive[Role];
	++AliveTotal;
	++Open.SpawnedByRole[Role];
	++Totals.Spawned[Role];
	Open.AlivePeak = FMath::Max(Open.AlivePeak, AliveTotal);
	Sink.QueueSpawn(Spawn);
}

int32 FDTDirectorCore::TakeNextThreatRow()
{
	if (PendingThreats.Num() > 0)
	{
		const int32 Row = PendingThreats[0];
		PendingThreats.RemoveAt(0);
		return Row;
	}
	for (int32 Row = NextRow; Row < Rows.Num(); ++Row)
	{
		if (Rows[Row].Phase == DTCat::ETimelinePhase::Threat && !StartedEarly.Contains(Row))
		{
			StartedEarly.Add(Row);
			return Row;
		}
	}
	return INDEX_NONE;
}

void FDTDirectorCore::StartBeat(int32 Row, const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	const DTCat::FTimelineRow& R = Rows[Row];
	OpenBlock(EDTDirBlock::Threat, R.Start_s);
	BeatRow = Row;
	BeatId = Totals.Beats++;
	BeatEnd = Time + R.Duration_s;
	EventAlive = 0;
	EventQueued = 0;
	LastEventDeath = HeroAt(In, 0);
	SpawnEvent(R, In, Sink);
}

void FDTDirectorCore::SpawnEvent(const DTCat::FTimelineRow& Row, const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	using namespace DTDirectorCoreLocal;
	const int32 Hero = PickHero(In);
	if (Hero == INDEX_NONE || Row.EventCount <= 0)
	{
		return;
	}
	// Event enemies count toward C.ENEMIES.CAP: only as many as fit are queued, so the beat's clear is exact.
	const int32 Room = FMath::Max(0, DTConst::ENEMIES_CAP - AliveTotal);
	const int32 Count = FMath::Min(Row.EventCount, Room);
	Open.SkippedAtCap += Row.EventCount - Count;
	Totals.SkippedAtCap += Row.EventCount - Count;

	FRandomStream& Rng = *In.Rng;
	const FVector2f Center = HeroAt(In, Hero);
	const float RingMin = static_cast<float>(DTConst::DIR_THREAT_RING_MIN_UU);
	const float RingMax = static_cast<float>(DTConst::DIR_THREAT_RING_MAX_UU);

	auto Queue = [this, &Sink](FDTEnemySpawn Spawn)
	{
		Spawn.BeatId = BeatId;
		++EventAlive;
		++EventQueued;
		++Totals.EventSpawned;
		Emit(Spawn, Sink);
	};
	// One arc of the threat ring, centred on a Director-stream angle, Count bodies spaced along it.
	auto Arc = [&](EDTEnemyRole Role, bool bStaggerOut)
	{
		const float Mid = Rng.FRandRange(0.0f, 2.0f * PI);
		const float Step = ArcSpacing(Role) / RingMin;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = Mid + (static_cast<float>(Index) - 0.5f * static_cast<float>(Count - 1)) * Step;
			// Stampede: radii step out across the band so the wind-ups trigger one after another (staggered);
			// Nest: alternate inner and outer edge, a cluster rather than a line.
			const float T = Count > 1 ? static_cast<float>(Index) / static_cast<float>(Count - 1) : 0.0f;
			const float Radius = bStaggerOut ? FMath::Lerp(RingMin, RingMax, T) : ((Index & 1) ? RingMax : RingMin);
			Queue(MakeSpawn(Role, Center + Dir(Angle) * Radius));
		}
	};

	switch (Row.Event)
	{
	case DTCat::ETimelineEvent::EliteWave:
		for (int32 Index = 0; Index < Count; ++Index)
		{
			EDTEnemyRole Role = EDTEnemyRole::Swarm;
			DrawRole(Row.Weights, false, Rng, Role);
			FDTEnemySpawn Spawn = MakeSpawn(Role, OnRing(Center, RingMin, RingMax, Rng));
			Spawn.bElite = true;
			Queue(Spawn);
		}
		break;
	case DTCat::ETimelineEvent::SwarmRing:
	{
		const float Phase = Rng.FRandRange(0.0f, 2.0f * PI);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = Phase + 2.0f * PI * static_cast<float>(Index) / static_cast<float>(Count);
			Queue(MakeSpawn(EDTEnemyRole::Swarm, Center + Dir(Angle) * RingMin));
		}
		break;
	}
	case DTCat::ETimelineEvent::RusherStampede:
		Arc(EDTEnemyRole::Rusher, true);
		break;
	case DTCat::ETimelineEvent::TurretNest:
		Arc(EDTEnemyRole::Turret, false);
		break;
	default:
		break;
	}
}

void FDTDirectorCore::EndBeat(bool bCleared, const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	const DTCat::FTimelineRow& R = Rows[BeatRow];
	CloseBlock(bCleared ? TEXT("cleared") : TEXT("duration"), Sink);
	Totals.BeatsCleared += bCleared ? 1 : 0;

	// R-DIR-16: exactly one chest per beat.
	FVector2f ChestAt;
	if (bCleared && R.Event == DTCat::ETimelineEvent::EliteWave)
	{
		ChestAt = LastEventDeath;
	}
	else
	{
		const int32 Hero = PickHero(In);
		const float Angle = In.Rng->FRandRange(0.0f, 2.0f * PI);
		ChestAt = HeroAt(In, Hero) + DTDirectorCoreLocal::Dir(Angle) * static_cast<float>(DTConst::DIR_CHEST_DROP_UU);
	}
	Sink.SpawnChest(ClampToArena(ChestAt, 0.0f));
	++Totals.Chests;

	// R-DIR-04, R-DIR-08: Breath follows; no drop-in lands in it.
	CancelDropIns();
	OpenBlock(EDTDirBlock::Breath, R.Start_s);
	BreathEnd = Time + DTConst::DIR_BREATH_S;
}

void FDTDirectorCore::CancelDropIns()
{
	Totals.DropInsCancelled += DropIns.Num();
	DropIns.Reset();
	bDropInDue = false;
}

void FDTDirectorCore::RegularSpawn(const DTCat::FTimelineRow& Row, const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	const int32 Cap = FMath::Min(Row.Cap, DTConst::ENEMIES_CAP);
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	if (AliveTotal >= Cap || !DrawRole(Row.Weights, true, *In.Rng, Role))
	{
		// R-DIR-05: the credit is spent, nothing is banked.
		++Open.SkippedAtCap;
		++Totals.SkippedAtCap;
		return;
	}
	const int32 Hero = PickHero(In);
	if (Hero == INDEX_NONE)
	{
		return;
	}
	// R-DIR-06: threat roles use the threat ring in a beat; turrets always do.
	const bool bThreatRing = Role == EDTEnemyRole::Turret || (Block == EDTDirBlock::Threat && Role == EDTEnemyRole::Rusher);
	const float MinR = static_cast<float>(bThreatRing ? DTConst::DIR_THREAT_RING_MIN_UU : DTConst::DIR_SPAWN_RING_MIN_UU);
	const float MaxR = static_cast<float>(bThreatRing ? DTConst::DIR_THREAT_RING_MAX_UU : DTConst::DIR_SPAWN_RING_MAX_UU);
	Emit(MakeSpawn(Role, OnRing(HeroAt(In, Hero), MinR, MaxR, *In.Rng)), Sink);
}

void FDTDirectorCore::StepDropIns(const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	// Land the markers whose lead is over (R-DIR-07): the enemy arrives flagged, its threat action starts at once.
	for (int32 Index = 0; Index < DropIns.Num();)
	{
		if (DropIns[Index].Marker.LandStep <= StepIndex)
		{
			FDTEnemySpawn Spawn = MakeSpawn(DropIns[Index].Spawn.Role, DropIns[Index].Spawn.Position);
			Spawn.bArrival = true;
			Emit(Spawn, Sink);
			++Totals.DropInsLanded;
			DropIns.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}

	if ((Block != EDTDirBlock::Harvest && Block != EDTDirBlock::Threat) || !Rows.IsValidIndex(HarvestRow))
	{
		return;
	}
	const DTCat::FTimelineRow& Row = Rows[HarvestRow];
	if (Row.DropInInterval_s <= 0.0f)
	{
		return;
	}
	const bool bThreat = Block == EDTDirBlock::Threat;
	const float Interval = Row.DropInInterval_s / (bThreat ? static_cast<float>(DTConst::DIR_DROPIN_THREAT_MULT) : 1.0f);
	if (!bDropInDue)
	{
		DropInProgress += StepSeconds / Interval;
		if (DropInProgress >= 1.0f)
		{
			// R-DIR-08: at most one waits; the cadence restarts from now, so nothing is banked while it waits.
			DropInProgress = 0.0f;
			bDropInDue = true;
		}
	}
	if (!bDropInDue)
	{
		return;
	}
	const int32 MaxLive = bThreat ? DTConst::DIR_DROPIN_ACTIVE_MAX_THREAT : DTConst::DIR_DROPIN_ACTIVE_MAX;
	const int32 LeadSteps = FMath::Max(1, FMath::RoundToInt(DTConst::DIR_DROPIN_LEAD_S / StepSeconds));
	const double Land = Time + static_cast<double>(LeadSteps) * StepSeconds;
	const bool bGapOk = !LastLandTime.IsSet() || Land - LastLandTime.GetValue() >= DTConst::DIR_DROPIN_GAP_MIN_S - 0.5 * StepSeconds;
	if (DropIns.Num() >= MaxLive || !bGapOk || !HasLivingHero(In))
	{
		return; // waits
	}
	const float WRusher = FMath::Max(0.0f, Row.DropInWeightRusher);
	const float WTurret = FMath::Max(0.0f, Row.DropInWeightTurret);
	if (WRusher + WTurret <= 0.0f)
	{
		bDropInDue = false;
		return;
	}
	FRandomStream& Rng = *In.Rng;
	const EDTEnemyRole Role = Rng.GetFraction() * (WRusher + WTurret) < WRusher ? EDTEnemyRole::Rusher : EDTEnemyRole::Turret;
	const int32 Hero = PickHero(In);
	const FVector2f At = OnRing(HeroAt(In, Hero), static_cast<float>(DTConst::DIR_THREAT_RING_MIN_UU),
		static_cast<float>(DTConst::DIR_THREAT_RING_MAX_UU), Rng);

	FDropIn& Drop = DropIns.AddDefaulted_GetRef();
	Drop.Spawn = MakeSpawn(Role, At);
	Drop.Marker.Role = Role;
	Drop.Marker.Position = Drop.Spawn.Position;
	Drop.Marker.LandStep = StepIndex + LeadSteps;
	LastLandTime = Land;
	bDropInDue = false;
	++Open.DropIns;
	++Totals.DropInsPublished;
}

void FDTDirectorCore::ApplyForced(EDTDirBlock Wanted, const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	const float HarvestStart = Rows.IsValidIndex(HarvestRow) ? Rows[HarvestRow].Start_s : 0.0f;
	switch (Wanted)
	{
	case EDTDirBlock::Harvest:
		if (Block == EDTDirBlock::Threat || Block == EDTDirBlock::Breath)
		{
			CloseBlock(TEXT("forced"), Sink);
			CancelDropIns();
			OpenBlock(EDTDirBlock::Harvest, HarvestStart);
		}
		break;
	case EDTDirBlock::Threat:
		if (Block == EDTDirBlock::Harvest || Block == EDTDirBlock::Breath)
		{
			const int32 Row = TakeNextThreatRow();
			if (Row != INDEX_NONE)
			{
				CloseBlock(TEXT("forced"), Sink);
				StartBeat(Row, In, Sink);
			}
		}
		break;
	case EDTDirBlock::Breath:
		if (Block == EDTDirBlock::Threat)
		{
			EndBeat(false, In, Sink);
		}
		else if (Block == EDTDirBlock::Harvest)
		{
			CloseBlock(TEXT("forced"), Sink);
			CancelDropIns();
			OpenBlock(EDTDirBlock::Breath, HarvestStart);
			BreathEnd = Time + DTConst::DIR_BREATH_S;
		}
		break;
	default:
		break;
	}
}

void FDTDirectorCore::Step(const FDTDirectorStepIn& In, IDTDirectorSink& Sink)
{
	if (!In.View)
	{
		return;
	}
	FDTWorldView& View = *In.View;
	if (Block == EDTDirBlock::Idle || !In.Rng)
	{
		View.DropInMarkers.Reset();
		View.bThreatBlock = false;
		return;
	}
	StepIndex = In.StepIndex;
	StepSeconds = In.StepSeconds;
	AliveTotal = In.AliveTotal;
	for (int32 Role = 0; Role < DTDirectorCoreLocal::NumRoles; ++Role)
	{
		Alive[Role] = In.AliveByRole[Role];
	}
	const double Now = Time;

	// 1. This step's kills (R-DIR-12 drops, R-DIR-04 event deaths, telemetry).
	for (const FDTKill& Kill : View.Kills)
	{
		++Open.Kills;
		++Totals.Kills;
		if (View.HeroPositions.IsValidIndex(Kill.OwnerId))
		{
			KillDistances.Add(FVector2f::Distance(Kill.Position, View.HeroPositions[Kill.OwnerId]));
		}
		if (!Kill.bDirector)
		{
			continue; // debug and stress spawns drop nothing
		}
		const int32 Value = GemValue(Kill.Role, Kill.bElite);
		if (Value > 0)
		{
			FDTGemSpawn Gem;
			Gem.Position = Kill.Position;
			Gem.Value = Value;
			Gem.OwnerId = Kill.OwnerId;
			Sink.SpawnGem(Gem);
			++Totals.Gems;
		}
		if (Block == EDTDirBlock::Threat && BeatId != INDEX_NONE && Kill.BeatId == BeatId && EventAlive > 0)
		{
			--EventAlive;
			LastEventDeath = Kill.Position;
		}
	}

	// 2. Threat actions within the threat ring of a living hero (R-DIR-09, dir_block threat_actions).
	for (const FVector2f& Origin : In.ThreatOrigins)
	{
		for (int32 Hero = 0; Hero < View.HeroPositions.Num(); ++Hero)
		{
			const bool bAlive = !In.HeroAlive.IsValidIndex(Hero) || In.HeroAlive[Hero];
			if (bAlive && FVector2f::DistSquared(Origin, View.HeroPositions[Hero]) <= FMath::Square(static_cast<float>(DTConst::DIR_THREAT_RING_MAX_UU)))
			{
				++Open.ThreatActions;
				++Totals.ThreatActions;
				break;
			}
		}
	}

	// 3. Boss hand-off (R-DIR-13): everything regular stops, markers cancel, one request.
	if (Block != EDTDirBlock::Boss && Now >= BossSeconds)
	{
		CloseBlock(TEXT("boss"), Sink);
		CancelDropIns();
		PendingThreats.Reset();
		Credit = 0.0f;
		OpenBlock(EDTDirBlock::Boss, BossSeconds);
		CloseBlock(TEXT("request"), Sink);
		Totals.bBossRequested = true;
		Sink.BossTime(static_cast<float>(Now));
	}
	if (Block == EDTDirBlock::Boss)
	{
		View.DropInMarkers.Reset();
		View.bThreatBlock = false;
		Time += StepSeconds;
		return;
	}

	// 4. Debug block requests.
	if (Forced.IsSet())
	{
		const EDTDirBlock Wanted = Forced.GetValue();
		Forced.Reset();
		ApplyForced(Wanted, In, Sink);
	}

	// 5. Beat end: its event enemies all dead, or its duration (R-DIR-04).
	if (Block == EDTDirBlock::Threat)
	{
		if (EventQueued > 0 && EventAlive == 0)
		{
			EndBeat(true, In, Sink);
		}
		else if (Now >= BeatEnd)
		{
			EndBeat(false, In, Sink);
		}
	}

	// 6. Breath end.
	if (Block == EDTDirBlock::Breath && Now >= BreathEnd)
	{
		CloseBlock(TEXT("breath_end"), Sink);
		OpenBlock(EDTDirBlock::Harvest, Rows.IsValidIndex(HarvestRow) ? Rows[HarvestRow].Start_s : 0.0f);
	}

	// 7. Rows reached by the clock.
	bool bHarvestChanged = false;
	while (Rows.IsValidIndex(NextRow) && static_cast<double>(Rows[NextRow].Start_s) <= Now)
	{
		const DTCat::FTimelineRow& Row = Rows[NextRow];
		if (Row.Phase == DTCat::ETimelinePhase::Harvest)
		{
			HarvestRow = NextRow;
			bHarvestChanged = true;
		}
		else if (Row.Phase == DTCat::ETimelinePhase::Threat && !StartedEarly.Contains(NextRow))
		{
			PendingThreats.Add(NextRow);
		}
		++NextRow;
	}

	// 8. A beat starts from Harvest only (never inside a beat or its Breath).
	while (Block == EDTDirBlock::Harvest && PendingThreats.Num() > 0)
	{
		const int32 Row = PendingThreats[0];
		PendingThreats.RemoveAt(0);
		if (static_cast<double>(Rows[Row].Start_s + Rows[Row].Duration_s) <= Now)
		{
			++Totals.BeatsSkipped; // its window passed (a debug clock jump)
			continue;
		}
		CloseBlock(TEXT("beat"), Sink);
		StartBeat(Row, In, Sink);
		bHarvestChanged = false;
	}
	if (bHarvestChanged && Block == EDTDirBlock::Harvest)
	{
		if (OpenRow == INDEX_NONE)
		{
			// The run's first row: the block opened at StartRun is this row's.
			Open.RowStartSeconds = Rows[HarvestRow].Start_s;
			OpenRow = HarvestRow;
		}
		else
		{
			CloseBlock(TEXT("next_row"), Sink);
			OpenBlock(EDTDirBlock::Harvest, Rows[HarvestRow].Start_s);
		}
	}

	// 9. Regular spawns (R-DIR-05): Harvest values, overlaid by the beat's row in a Threat block.
	const DTCat::FTimelineRow* Active = nullptr;
	if (Block == EDTDirBlock::Threat && Rows.IsValidIndex(BeatRow))
	{
		Active = &Rows[BeatRow];
	}
	else if (Block == EDTDirBlock::Harvest && Rows.IsValidIndex(HarvestRow))
	{
		Active = &Rows[HarvestRow];
	}
	if (Active)
	{
		Credit = FMath::Min(1.0f, Credit + Active->SpawnRatePerS * StepSeconds);
		while (Credit >= 1.0f)
		{
			Credit -= 1.0f;
			RegularSpawn(*Active, In, Sink);
		}
	}

	// 10. Drop-ins (R-DIR-07, R-DIR-08).
	StepDropIns(In, Sink);

	// 11. Publish (07_Technical §4).
	View.DropInMarkers.Reset();
	for (const FDropIn& Drop : DropIns)
	{
		View.DropInMarkers.Add(Drop.Marker);
	}
	View.bThreatBlock = Block == EDTDirBlock::Threat;
	Open.AlivePeak = FMath::Max(Open.AlivePeak, AliveTotal);
	Time += StepSeconds;
}
