#include "Player/DTHeroSim.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"

namespace DTHeroSimLocal
{
	// Numeric tolerance for sim-time countdowns and the meter (float steps of 1/C.SIM.FIXED_HZ never sum exactly).
	// Far below one step (1/60 s) and one meter step (1/3600), so it never moves a threshold by a whole step.
	constexpr float TimeEpsilon = 1e-4f;
	// Below this length a direction is treated as zero (no move input, hero on top of its aim target).
	constexpr float DirEpsilon = 1e-4f;

	static float CountDown(float Left, float Seconds)
	{
		const float Next = Left - Seconds;
		return Next <= TimeEpsilon ? 0.0f : Next;
	}

	static bool HitBeats(const FDTHeroHit& A, const FDTHeroHit& B)
	{
		// R-CMB-09: largest damage; ties: contact before bullet before hazard, then the lower enemy serial.
		if (A.Damage != B.Damage)
		{
			return A.Damage > B.Damage;
		}
		if (A.Kind != B.Kind)
		{
			return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind);
		}
		return A.EnemySerial < B.EnemySerial;
	}
}

// --- latch ------------------------------------------------------------------------------------------------------------

void FDTHeroInputLatch::Latch(int32 OwnerId, const FDTHeroInput& Frame)
{
	if (OwnerId < 0)
	{
		return;
	}
	if (!Pending.IsValidIndex(OwnerId))
	{
		Pending.SetNum(OwnerId + 1);
	}
	Pending[OwnerId].MergeFrame(Frame);
}

FDTHeroInput FDTHeroInputLatch::Consume(int32 OwnerId)
{
	if (!Pending.IsValidIndex(OwnerId))
	{
		return FDTHeroInput();
	}
	FDTHeroInput Out = Pending[OwnerId];
	Pending[OwnerId].bDashPress = false;
	Pending[OwnerId].bIntercessionPress = false;
	return Out;
}

// --- sim ------------------------------------------------------------------------------------------------------------

float FDTHeroSim::ComputeMaxHp(const FDTHeroStats& Stats)
{
	return static_cast<float>(DTConst::HERO_MAX_HP) + Stats.MaxHpBonus;
}

int32 FDTHeroSim::ComputeMaxCharges(const FDTHeroStats& Stats)
{
	return FMath::Max(0, DTConst::DASH_CHARGES + Stats.DashChargesBonus);
}

FVector2f FDTHeroSim::ShapeStick(const FVector2f& Raw)
{
	FVector2f V = Raw;
	const float Len = V.Size();
	if (Len > 1.0f)
	{
		V /= Len;
	}
	const float Clamped = FMath::Min(Len, 1.0f);
	const float DeadZone = DTConst::HERO_STICK_DEADZONE;
	if (Clamped <= DeadZone || Clamped <= DTHeroSimLocal::DirEpsilon)
	{
		return FVector2f::ZeroVector;
	}
	// Rescale (DeadZone, 1] to (0, 1] along the same direction: no jump at the edge, full speed at full tilt.
	const float Scaled = (Clamped - DeadZone) / (1.0f - DeadZone);
	return V / Clamped * Scaled;
}

int32 FDTHeroSim::FindNearest(const FVector2f& From, TConstArrayView<FVector2f> Positions, const FDTSpatialHash& Hash)
{
	if (Positions.Num() == 0)
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	auto Consider = [&](int32 Index)
	{
		const float DistSq = FVector2f::DistSquared(From, Positions[Index]);
		if (DistSq < BestDistSq || (DistSq == BestDistSq && Index < Best))
		{
			BestDistSq = DistSq;
			Best = Index;
		}
	};
	if (Hash.Num() != Positions.Num())
	{
		// The hash was not built for these positions (tests, stress maps): scan.
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Consider(Index);
		}
		return Best;
	}
	// Grow a query circle until it holds a point inside its radius: any closer point lies in the circle too.
	TArray<int32> Candidates;
	const float MaxRadius = 4.0f * static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU) + FVector2f::Distance(From, FVector2f::ZeroVector);
	for (float Radius = Hash.GetCellSize(); ; Radius *= 2.0f)
	{
		Candidates.Reset();
		Hash.QueryCircle(From, Radius, Candidates);
		const float RadiusSq = Radius * Radius;
		for (const int32 Index : Candidates)
		{
			if (Positions.IsValidIndex(Index) && FVector2f::DistSquared(From, Positions[Index]) <= RadiusSq)
			{
				Consider(Index);
			}
		}
		if (Best != INDEX_NONE || Radius >= MaxRadius)
		{
			return Best;
		}
	}
}

void FDTHeroSim::Reset(int32 NumPlayers)
{
	const int32 Count = FMath::Max(1, NumPlayers);
	States.Reset();
	States.SetNum(Count);
	StepInputs.Reset();
	StepInputs.SetNum(Count);
	const int32 OldModes = AimModes.Num();
	AimModes.SetNum(Count);
	for (int32 Index = OldModes; Index < Count; ++Index)
	{
		AimModes[Index] = EDTAimMode::Auto;
	}
	for (FDTHeroState& S : States)
	{
		S.MaxHp = ComputeMaxHp(S.Stats);
		S.Hp = S.MaxHp;
		S.DashCharges = ComputeMaxCharges(S.Stats);
		S.DashChargesMax = S.DashCharges;
	}
	StepEvents.Reset();
	bRunOver = false;
	RunEndStep = INDEX_NONE;
}

void FDTHeroSim::SetStepInput(int32 OwnerId, const FDTHeroInput& Input)
{
	if (StepInputs.IsValidIndex(OwnerId))
	{
		StepInputs[OwnerId] = Input;
	}
}

EDTAimMode FDTHeroSim::GetAimMode(int32 OwnerId) const
{
	return AimModes.IsValidIndex(OwnerId) ? AimModes[OwnerId] : EDTAimMode::Auto;
}

void FDTHeroSim::SetAimMode(int32 OwnerId, EDTAimMode Mode)
{
	if (OwnerId < 0)
	{
		return;
	}
	if (!AimModes.IsValidIndex(OwnerId))
	{
		const int32 Old = AimModes.Num();
		AimModes.SetNum(OwnerId + 1);
		for (int32 Index = Old; Index < AimModes.Num(); ++Index)
		{
			AimModes[Index] = EDTAimMode::Auto;
		}
	}
	AimModes[OwnerId] = Mode;
}

void FDTHeroSim::SetStats(int32 OwnerId, const FDTHeroStats& Stats)
{
	if (States.IsValidIndex(OwnerId))
	{
		States[OwnerId].Stats = Stats;
	}
}

void FDTHeroSim::Emit(FDTHeroEvent Event, int32 OwnerId, int64 StepIndex)
{
	Event.OwnerId = OwnerId;
	Event.StepIndex = StepIndex;
	StepEvents.Add(Event);
}

void FDTHeroSim::Step(const FDTSimContext& Context)
{
	StepEvents.Reset();
	if (!Context.View || States.Num() == 0)
	{
		return;
	}
	FDTWorldView& View = *Context.View;

	// R-CMB-08: damage comes only from what the previous step published. Pick each player's candidate (R-CMB-09).
	HitScratch.Init(INDEX_NONE, States.Num());
	for (int32 HitIndex = 0; HitIndex < View.HeroHits.Num(); ++HitIndex)
	{
		const FDTHeroHit& Hit = View.HeroHits[HitIndex];
		if (!States.IsValidIndex(Hit.PlayerId) || !(Hit.Damage > 0.0f))
		{
			continue;
		}
		int32& Best = HitScratch[Hit.PlayerId];
		if (Best == INDEX_NONE || DTHeroSimLocal::HitBeats(Hit, View.HeroHits[Best]))
		{
			Best = HitIndex;
		}
	}

	for (int32 OwnerId = 0; OwnerId < States.Num(); ++OwnerId)
	{
		const int32 HitIndex = HitScratch[OwnerId];
		StepPlayer(OwnerId, Context, HitIndex != INDEX_NONE ? &View.HeroHits[HitIndex] : nullptr);
	}
	View.HeroHits.Reset();

	// Publish (07_Technical §4: overwritten each step).
	View.HeroPositions.SetNum(States.Num());
	View.HeroAimDirs.SetNum(States.Num());
	View.HeroIntangible.SetNum(States.Num());
	bool bAllDown = true;
	for (int32 OwnerId = 0; OwnerId < States.Num(); ++OwnerId)
	{
		const FDTHeroState& S = States[OwnerId];
		View.HeroPositions[OwnerId] = S.Pos;
		View.HeroAimDirs[OwnerId] = S.AimDir;
		View.HeroIntangible[OwnerId] = S.bIntangible;
		bAllDown &= S.bDown;
	}
	if (bAllDown && !bRunOver)
	{
		bRunOver = true;
		RunEndStep = Context.StepIndex;
	}
}

void FDTHeroSim::StepPlayer(int32 OwnerId, const FDTSimContext& Context, const FDTHeroHit* Hit)
{
	using namespace DTHeroSimLocal;
	FDTHeroState& S = States[OwnerId];
	const FDTHeroInput& In = StepInputs[OwnerId];
	const float Dt = Context.StepSeconds;
	S.PrevPos = S.Pos;

	if (S.bDown)
	{
		// R-CMB-10: a downed hero stops; its input is ignored.
		S.Vel = FVector2f::ZeroVector;
		S.bIntangible = false;
		return;
	}

	// --- damage intake (R-CMB-08, R-CMB-09) ---
	S.HurtIFramesLeft = CountDown(S.HurtIFramesLeft, Dt);
	const bool bProtected = bNoDamage || S.bIntangible || S.DashLeft > 0.0f || S.HurtIFramesLeft > 0.0f;
	if (Hit && !bProtected)
	{
		const float Amount = Hit->Damage * S.Stats.DamageTakenMul;
		S.Hp -= Amount;
		S.HurtIFramesLeft = DTConst::HERO_HURT_IFRAMES_S;
		++S.HitsTaken;
		FDTHeroEvent Event;
		Event.Kind = EDTHeroEventKind::HeroDamaged;
		Event.Amount = Amount;
		Event.HpAfter = S.Hp;
		Event.MaxHp = S.MaxHp;
		Event.HitKind = Hit->Kind;
		Event.SourceRole = Hit->SourceRole;
		Event.AttackId = Hit->AttackId;
		Emit(Event, OwnerId, Context.StepIndex);
		if (S.Hp <= 0.0f)
		{
			S.bDown = true;
			S.Vel = FVector2f::ZeroVector;
			S.DashLeft = 0.0f;
			S.bIntangible = false;
			S.DashPressAge = -1.0f;
			S.IntercessionPressAge = -1.0f;
			FDTHeroEvent Down;
			Down.Kind = EDTHeroEventKind::HeroDown;
			Emit(Down, OwnerId, Context.StepIndex);
			return;
		}
	}

	// --- stats that follow the stat set ---
	const float NewMaxHp = ComputeMaxHp(S.Stats);
	if (NewMaxHp != S.MaxHp)
	{
		S.Hp = FMath::Min(S.Hp + FMath::Max(0.0f, NewMaxHp - S.MaxHp), NewMaxHp);
		S.MaxHp = NewMaxHp;
	}
	const int32 MaxCharges = ComputeMaxCharges(S.Stats);
	S.DashCharges = FMath::Clamp(S.DashCharges + FMath::Max(0, MaxCharges - S.DashChargesMax), 0, MaxCharges);
	S.DashChargesMax = MaxCharges;
	const float Cooldown = DTConst::DASH_COOLDOWN_S * S.Stats.DashCooldownMul;

	// --- dash refill: one charge per cooldown, one at a time (R-CMB-06) ---
	if (S.DashCharges < MaxCharges)
	{
		if (S.DashRefillLeft <= 0.0f)
		{
			S.DashRefillLeft = Cooldown;
		}
		S.DashRefillLeft = CountDown(S.DashRefillLeft, Dt);
		if (S.DashRefillLeft <= 0.0f)
		{
			++S.DashCharges;
			S.DashRefillLeft = S.DashCharges < MaxCharges ? Cooldown : 0.0f;
		}
	}
	else
	{
		S.DashRefillLeft = 0.0f;
	}

	// --- move input (R-CMB-02, R-CMB-03) ---
	const FVector2f Move = ShapeStick(In.Move);
	const bool bHasMove = Move.SizeSquared() > DirEpsilon * DirEpsilon;
	if (bHasMove)
	{
		S.LastMoveDir = Move.GetSafeNormal();
	}

	// --- dash press, buffered for C.HERO.INPUT_BUFFER_S (R-CMB-02, R-CMB-06) ---
	if (In.bDashPress)
	{
		S.DashPressAge = 0.0f;
	}
	if (S.DashPressAge >= 0.0f)
	{
		if (S.DashLeft <= 0.0f && S.DashCharges > 0)
		{
			--S.DashCharges;
			if (S.DashRefillLeft <= 0.0f)
			{
				S.DashRefillLeft = Cooldown;
			}
			S.DashLeft = DTConst::DASH_DURATION_S;
			S.DashDir = bHasMove ? Move.GetSafeNormal() : S.LastMoveDir;
			S.DashPressAge = -1.0f;
			++S.Dashes;
			FDTHeroEvent Event;
			Event.Kind = EDTHeroEventKind::Dash;
			Event.Dir = S.DashDir;
			Event.ChargesAfter = S.DashCharges;
			Emit(Event, OwnerId, Context.StepIndex);
		}
		else
		{
			S.DashPressAge += Dt;
			if (S.DashPressAge > DTConst::HERO_INPUT_BUFFER_S + TimeEpsilon)
			{
				S.DashPressAge = -1.0f;
			}
		}
	}

	// --- movement on the sim plane, clamped to the arena (R-CMB-03, R-CMB-06) ---
	if (S.DashLeft > 0.0f)
	{
		const float DashSpeed = static_cast<float>(DTConst::DASH_DISTANCE_UU) / DTConst::DASH_DURATION_S;
		const float Slice = FMath::Min(Dt, S.DashLeft);
		S.Vel = S.DashDir * DashSpeed;
		S.Pos += S.DashDir * (DashSpeed * Slice);
		S.DashLeft = CountDown(S.DashLeft, Slice);
		S.bIntangible = true;
	}
	else
	{
		S.Vel = Move * (static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) * S.Stats.MoveSpeedMul);
		S.Pos += S.Vel * Dt;
		S.bIntangible = false;
	}
	const float Limit = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU - DTConst::HERO_RADIUS_UU);
	S.Pos.X = FMath::Clamp(S.Pos.X, -Limit, Limit);
	S.Pos.Y = FMath::Clamp(S.Pos.Y, -Limit, Limit);

	// --- aim (R-CMB-04) ---
	if (GetAimMode(OwnerId) == EDTAimMode::Auto)
	{
		const FDTWorldView& View = *Context.View;
		const int32 Nearest = FindNearest(S.Pos, View.EnemyPositions, View.EnemyHash);
		if (Nearest != INDEX_NONE)
		{
			const FVector2f To = View.EnemyPositions[Nearest] - S.Pos;
			if (To.SizeSquared() > DirEpsilon * DirEpsilon)
			{
				S.AimDir = To.GetSafeNormal();
			}
		}
	}
	else
	{
		const FVector2f Stick = ShapeStick(In.AimStick);
		if (Stick.SizeSquared() > DirEpsilon * DirEpsilon)
		{
			S.AimDir = Stick.GetSafeNormal();
		}
		else if (In.bAimCursorValid)
		{
			const FVector2f To = In.AimCursorWorld - S.Pos;
			if (To.SizeSquared() > DirEpsilon * DirEpsilon)
			{
				S.AimDir = To.GetSafeNormal();
			}
		}
	}

	// --- Intercession meter and press (R-CMB-11) ---
	const float ChargeSeconds = FMath::Max(DTConst::HERO_INTERCESSION_CHARGE_S * S.Stats.IntercessionCooldownMul, TimeEpsilon);
	S.Intercession01 = FMath::Min(1.0f, S.Intercession01 + Dt / ChargeSeconds);
	if (S.Intercession01 >= 1.0f - TimeEpsilon)
	{
		S.Intercession01 = 1.0f;
	}
	if (In.bIntercessionPress)
	{
		S.IntercessionPressAge = 0.0f;
	}
	if (S.IntercessionPressAge >= 0.0f)
	{
		if (S.Intercession01 >= 1.0f)
		{
			S.Intercession01 = 0.0f;
			S.IntercessionPressAge = -1.0f;
			++S.Intercessions;
			FDTHeroEvent Event;
			Event.Kind = EDTHeroEventKind::Intercession;
			if (Context.Run && Context.Run->IsValidPlayer(OwnerId))
			{
				const FDTPlayerRunState& Player = Context.Run->GetPlayer(OwnerId);
				Event.Path = Player.Keepers.Primary != EDTPath::None ? Player.Keepers.Primary : Player.Offer.PatronPath;
			}
			Emit(Event, OwnerId, Context.StepIndex);
		}
		else
		{
			// Deny now when the meter cannot fill inside the buffer window; otherwise keep it pending.
			const float FillLeft = (1.0f - S.Intercession01) * ChargeSeconds;
			const float BufferLeft = DTConst::HERO_INPUT_BUFFER_S - S.IntercessionPressAge;
			S.IntercessionPressAge += Dt;
			if (FillLeft > BufferLeft + TimeEpsilon)
			{
				S.IntercessionPressAge = -1.0f;
				FDTHeroEvent Event;
				Event.Kind = EDTHeroEventKind::IntercessionDenied;
				Emit(Event, OwnerId, Context.StepIndex);
			}
		}
	}
}

// --- phases ----------------------------------------------------------------------------------------------------------

void FDTHeroInputPhase::SimStep(const FDTSimContext& Context)
{
	if (!Hero || !Latch)
	{
		return;
	}
	const int32 Players = Context.Run ? Context.Run->GetNumPlayers() : Hero->Num();
	if (Context.StepIndex == 0 || Hero->Num() != Players)
	{
		Hero->Reset(Players);
	}
	for (int32 OwnerId = 0; OwnerId < Hero->Num(); ++OwnerId)
	{
		Hero->SetStepInput(OwnerId, Latch->Consume(OwnerId));
	}
}

void FDTHeroStepPhase::SimStep(const FDTSimContext& Context)
{
	if (Hero)
	{
		Hero->Step(Context);
	}
}
