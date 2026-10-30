#include "Bot/DTBotBrain.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Offers/DTOfferGenerator.h"

// The bot's own heuristics. They are tool parameters of the measuring instrument, not game tuning: no gameplay value is
// read from here and changing them never changes the game (record a change in Docs/Validation/S1_bot_baseline.md).
namespace DTBotTuning
{
	constexpr int32 NumDirs = 16;
	/** Look-ahead for moving threats (rounds, bodies). */
	constexpr float HorizonS = 0.6f;
	/** A threat this close in time asks for a dash. */
	constexpr float ImminentS = 0.22f;
	/** Clearance added to every collision radius. */
	constexpr float MarginUU = 18.0f;
	constexpr float RoundWeight = 1000.0f;
	constexpr float BodyWeight = 400.0f;
	constexpr float TelegraphWeight = 700.0f;
	constexpr float HazardWeight = 900.0f;
	/** Bodies inside this radius push the bot away (crowd pressure). */
	constexpr float CrowdRadiusUU = 650.0f;
	constexpr float CrowdWeight = 14.0f;
	/** Kiting: away from the centroid of enemies within this radius. */
	constexpr float KiteRadiusUU = 1300.0f;
	constexpr float KiteWeight = 6.0f;
	constexpr float WallBandUU = 900.0f;
	constexpr float WallWeight = 120.0f;
	constexpr float GemRadiusUU = 4000.0f;
	/** Gems nearer than this pull equally hard. */
	constexpr float GemNearUU = 200.0f;
	constexpr float GemWeight = 40.0f;
	constexpr float ChestWeight = 25.0f;
	/** A move toward pickups is rewarded only while its danger is below this (bodies and telegraphs come first). */
	constexpr float GemSafeDanger = 60.0f;
	constexpr float InertiaWeight = 2.0f;
	/** Only threats within this distance are considered. */
	constexpr float ThreatRangeUU = 1400.0f;
	/** Intercession when the meter is full and this many enemies are near. */
	constexpr int32 IntercessionCrowd = 12;
	constexpr float IntercessionRadiusUU = 700.0f;
	/** Skill 0: a decision every this many seconds of reaction delay; movement and aim noise in degrees. */
	constexpr float MaxReactionS = 0.25f;
	constexpr float MaxMoveNoiseDeg = 35.0f;
	constexpr float MaxAimNoiseDeg = 20.0f;
	constexpr int32 SeedSalt = 0x0B07;

	static FVector2f DirAt(int32 Index)
	{
		const float A = 2.0f * UE_PI * static_cast<float>(Index) / static_cast<float>(NumDirs);
		return FVector2f(FMath::Cos(A), FMath::Sin(A));
	}

	static FVector2f Rotate(const FVector2f& V, float Deg)
	{
		const float R = FMath::DegreesToRadians(Deg);
		const float C = FMath::Cos(R);
		const float S = FMath::Sin(R);
		return FVector2f(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	static float Limit() { return static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU - DTConst::HERO_RADIUS_UU); }

	static FVector2f Clamp(FVector2f P)
	{
		P.X = FMath::Clamp(P.X, -Limit(), Limit());
		P.Y = FMath::Clamp(P.Y, -Limit(), Limit());
		return P;
	}

	/** Closest approach of a point moving at RelVel from Rel (relative to the hero) within [0, Horizon]: time and distance. */
	static float ClosestApproach(const FVector2f& Rel, const FVector2f& RelVel, float Horizon, float& OutT)
	{
		const float VV = RelVel.SizeSquared();
		OutT = VV > UE_SMALL_NUMBER ? FMath::Clamp(-FVector2f::DotProduct(Rel, RelVel) / VV, 0.0f, Horizon) : 0.0f;
		return (Rel + RelVel * OutT).Size();
	}
}

// ---- config -----------------------------------------------------------------------------------------------------------

const TCHAR* FDTBotConfig::PolicyName(EDTBotPolicy Policy)
{
	switch (Policy)
	{
	case EDTBotPolicy::None: return TEXT("None");
	case EDTBotPolicy::Random: return TEXT("Random");
	case EDTBotPolicy::Steered: return TEXT("Steered");
	default: return TEXT("Off");
	}
}

EDTBotPolicy FDTBotConfig::ParsePolicy(const FString& Text)
{
	if (Text.Equals(TEXT("None"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("NoPick"), ESearchCase::IgnoreCase))
	{
		return EDTBotPolicy::None;
	}
	if (Text.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
	{
		return EDTBotPolicy::Random;
	}
	if (Text.Equals(TEXT("Steered"), ESearchCase::IgnoreCase))
	{
		return EDTBotPolicy::Steered;
	}
	return EDTBotPolicy::Off;
}

EDTPath FDTBotConfig::ParsePath(const FString& Text)
{
	static const TCHAR* Names[] = { TEXT("Storm"), TEXT("Forge"), TEXT("Tide"), TEXT("Shade"), TEXT("Radiance"), TEXT("Quicksilver") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		if (Text.Equals(Names[Index], ESearchCase::IgnoreCase))
		{
			return static_cast<EDTPath>(Index);
		}
	}
	return EDTPath::None;
}

EDTPath FDTBotConfig::DefaultSecond(EDTPath Primary)
{
	switch (Primary)
	{
	case EDTPath::Storm: return EDTPath::Tide;
	case EDTPath::Forge: return EDTPath::Storm;
	case EDTPath::Tide: return EDTPath::Forge;
	default: return EDTPath::Storm;
	}
}

FString FDTBotConfig::Describe() const
{
	static const TCHAR* Names[] = { TEXT("Storm"), TEXT("Forge"), TEXT("Tide"), TEXT("Shade"), TEXT("Radiance"), TEXT("Quicksilver") };
	const int32 P = static_cast<int32>(Path);
	const int32 S = static_cast<int32>(SecondPath == EDTPath::None ? DefaultSecond(Path) : SecondPath);
	return FString::Printf(TEXT("policy=%s path=%s archetype=%s second=%s skill=%.2f seed=%d"), PolicyName(Policy),
		P < UE_ARRAY_COUNT(Names) ? Names[P] : TEXT("-"), *Archetype.ToString(), S < UE_ARRAY_COUNT(Names) ? Names[S] : TEXT("-"), Skill, Seed);
}

// ---- brain ------------------------------------------------------------------------------------------------------------

void FDTBotBrain::Reset(const FDTBotConfig& InConfig)
{
	Config = InConfig;
	Config.Skill = FMath::Clamp(Config.Skill, 0.0f, 1.0f);
	MoveRng.Initialize(Config.Seed ^ DTBotTuning::SeedSalt);
	CardRng.Initialize((Config.Seed * 31) ^ (DTBotTuning::SeedSalt << 4));
	LastDir = FVector2f::ZeroVector;
	Held = FDTHeroInput();
	NextDecideStep = 0;
	ImminentSteps = 0;
	Stats = FDTBotStats();
}

int32 FDTBotBrain::ReactionSteps() const
{
	const float Delay = (1.0f - Config.Skill) * DTBotTuning::MaxReactionS;
	return FMath::RoundToInt32(Delay * static_cast<float>(DTConst::SIM_FIXED_HZ));
}

bool FDTBotBrain::InTelegraph(const FDTTelegraph& Tele, const FVector2f& P, float Margin)
{
	const FVector2f Local = P - Tele.Origin;
	const float Dist = Local.Size();
	const FVector2f Dir = Tele.Dir.GetSafeNormal();
	switch (Tele.Shape)
	{
	case EDTTelegraphShape::Band:
	case EDTTelegraphShape::AimLine:
	{
		const float Along = FVector2f::DotProduct(Local, Dir);
		const float Across = FMath::Abs(Local.X * Dir.Y - Local.Y * Dir.X);
		return Along >= -Margin && Along <= Tele.Length + Margin && Across <= Tele.Width * 0.5f + Margin;
	}
	case EDTTelegraphShape::Ring:
		return Tele.Width <= 0.0f ? Dist <= Tele.Length + Margin : (Dist <= Tele.Length + Margin && Dist >= Tele.Length - Tele.Width - Margin);
	case EDTTelegraphShape::Arc:
	{
		if (Dist > Tele.Length + Margin)
		{
			return false;
		}
		const float Cos = Dist > UE_SMALL_NUMBER ? FVector2f::DotProduct(Local / Dist, Dir) : 1.0f;
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.0f, 1.0f))) <= Tele.Width * 0.5f;
	}
	case EDTTelegraphShape::RingGap:
	case EDTTelegraphShape::SpiralLane:
	{
		// Everything out to Length is hit except the lane (a sector of Width degrees around Dir); the lane is shrunk by
		// the margin's angle at this distance so the hero body fits inside it.
		if (Dist > Tele.Length + Margin)
		{
			return false;
		}
		const float Cos = Dist > UE_SMALL_NUMBER ? FVector2f::DotProduct(Local / Dist, Dir) : 1.0f;
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.0f, 1.0f)));
		const float MarginDeg = Dist > Margin ? FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Margin / Dist, 0.0f, 1.0f))) : 90.0f;
		return Off > Tele.Width * 0.5f - MarginDeg;
	}
	case EDTTelegraphShape::HazardRing:
		return Dist >= Tele.Length - Margin;
	default:
		return false;
	}
}

float FDTBotBrain::ThreatAlong(const FDTBotWorld& World, const FVector2f& Vel, float Horizon, float* OutSoonest) const
{
	using namespace DTBotTuning;
	const FVector2f P = World.Hero->Pos;
	const float HeroR = static_cast<float>(DTConst::HERO_RADIUS_UU);
	float Score = 0.0f;
	float Soonest = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < World.RoundPos.Num(); ++Index)
	{
		const FVector2f Rel = World.RoundPos[Index] - P;
		if (Rel.SizeSquared() > ThreatRangeUU * ThreatRangeUU)
		{
			continue;
		}
		const FVector2f RVel = World.RoundVel.IsValidIndex(Index) ? World.RoundVel[Index] : FVector2f::ZeroVector;
		float T = 0.0f;
		const float D = ClosestApproach(Rel, RVel - Vel, Horizon, T);
		const float R = HeroR + (World.RoundRadius.IsValidIndex(Index) ? World.RoundRadius[Index] : 0.0f) + MarginUU;
		if (D < R)
		{
			Score += RoundWeight * (1.0f - T / (Horizon + 0.1f)) * (1.0f + (R - D) / R);
			Soonest = FMath::Min(Soonest, T);
		}
	}
	for (int32 Index = 0; Index < World.EnemyPos.Num(); ++Index)
	{
		const FVector2f Rel = World.EnemyPos[Index] - P;
		if (Rel.SizeSquared() > ThreatRangeUU * ThreatRangeUU)
		{
			continue;
		}
		const FVector2f EVel = World.EnemyVel.IsValidIndex(Index) ? World.EnemyVel[Index] : FVector2f::ZeroVector;
		float T = 0.0f;
		const float D = ClosestApproach(Rel, EVel - Vel, Horizon, T);
		const float R = HeroR + (World.EnemyRadius.IsValidIndex(Index) ? World.EnemyRadius[Index] : 0.0f) + MarginUU;
		if (D < R)
		{
			Score += BodyWeight * (1.0f - T / (Horizon + 0.1f)) * (1.0f + (R - D) / R);
			Soonest = FMath::Min(Soonest, T);
		}
	}
	if (OutSoonest)
	{
		*OutSoonest = Soonest;
	}
	return Score;
}

float FDTBotBrain::DangerAt(const FDTBotWorld& World, const FVector2f& P, float T) const
{
	using namespace DTBotTuning;
	const float HeroR = static_cast<float>(DTConst::HERO_RADIUS_UU);
	float Score = 0.0f;
	for (const FDTTelegraph& Tele : World.Telegraphs)
	{
		if (InTelegraph(Tele, P, HeroR + MarginUU))
		{
			if (Tele.Shape == EDTTelegraphShape::HazardRing)
			{
				// Graded by the distance outside: from past the look-ahead every move lands in the hazard, and a flat score would
				// leave only the kite, which points away from the boss, so outward (S1_boss_after_fields.md).
				const float Outside = FMath::Max(0.0f, FVector2f::Distance(P, Tele.Origin) - Tele.Length);
				Score += HazardWeight * (0.3f + Tele.Progress) * (1.0f + Outside / FMath::Max(Tele.Length, 1.0f));
			}
			else
			{
				Score += TelegraphWeight * (0.3f + Tele.Progress);
			}
		}
	}
	for (int32 Index = 0; Index < World.EnemyPos.Num(); ++Index)
	{
		const FVector2f Ahead = World.EnemyPos[Index] + (World.EnemyVel.IsValidIndex(Index) ? World.EnemyVel[Index] * T : FVector2f::ZeroVector);
		const float D = FVector2f::Distance(Ahead, P);
		if (D < CrowdRadiusUU)
		{
			const float W = (CrowdRadiusUU - D) / CrowdRadiusUU;
			Score += CrowdWeight * W * W;
		}
	}
	const float Edge = Limit() - FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
	if (Edge < WallBandUU)
	{
		const float W = (WallBandUU - Edge) / WallBandUU;
		Score += WallWeight * W * W;
	}
	return Score;
}

float FDTBotBrain::ScoreMove(const FDTBotWorld& World, const FVector2f& Dir, float Speed) const
{
	using namespace DTBotTuning;
	const FVector2f Vel = Dir * Speed;
	float Score = ThreatAlong(World, Vel, HorizonS, nullptr);
	// Static danger at two look-ahead points (telegraphs land, the crowd closes in).
	for (const float T : { HorizonS * 0.5f, HorizonS })
	{
		Score += DangerAt(World, Clamp(World.Hero->Pos + Vel * T), T);
	}
	return Score;
}

FVector2f FDTBotBrain::ChooseDash(const FDTBotWorld& World) const
{
	using namespace DTBotTuning;
	const float DashT = DTConst::DASH_DURATION_S;
	const float Dist = static_cast<float>(DTConst::DASH_DISTANCE_UU);
	FVector2f Best = World.Hero->LastMoveDir;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < NumDirs; ++Index)
	{
		const FVector2f Dir = DirAt(Index);
		const FVector2f End = Clamp(World.Hero->Pos + Dir * Dist);
		// Intangible during the dash: only the landing matters, and what is heading there right after it.
		float Score = DangerAt(World, End, DashT);
		FDTHeroState Probe = *World.Hero;
		Probe.Pos = End;
		FDTBotWorld After = World;
		After.Hero = &Probe;
		Score += ThreatAlong(After, FVector2f::ZeroVector, ImminentS, nullptr) * 0.5f;
		if (FVector2f::Distance(End, World.Hero->Pos) < Dist * 0.5f)
		{
			Score += WallWeight * 4.0f; // a wall eats most of the dash
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Dir;
		}
	}
	return Best;
}

FDTHeroInput FDTBotBrain::Decide(const FDTBotWorld& World)
{
	using namespace DTBotTuning;
	check(World.Hero);
	const FDTHeroState& S = *World.Hero;
	if (S.bDown)
	{
		return FDTHeroInput();
	}
	FDTHeroInput In = Held;
	In.bDashPress = false;
	In.bIntercessionPress = false;

	const int32 Reaction = ReactionSteps();
	if (World.StepIndex >= NextDecideStep)
	{
		NextDecideStep = World.StepIndex + 1 + Reaction;
		++Stats.Decisions;
		const float Speed = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) * S.Stats.MoveSpeedMul;

		// Kite direction: away from the centroid of nearby enemies.
		FVector2f Centroid = FVector2f::ZeroVector;
		int32 Near = 0;
		int32 Crowd = 0;
		for (const FVector2f& E : World.EnemyPos)
		{
			const float D2 = FVector2f::DistSquared(E, S.Pos);
			if (D2 < KiteRadiusUU * KiteRadiusUU)
			{
				Centroid += E;
				++Near;
			}
			Crowd += D2 < IntercessionRadiusUU * IntercessionRadiusUU ? 1 : 0;
		}
		const FVector2f Away = Near > 0 ? (S.Pos - Centroid / static_cast<float>(Near)).GetSafeNormal() : FVector2f::ZeroVector;

		// Nearest pickup: a chest first, else a gem.
		FVector2f ToPickup = FVector2f::ZeroVector;
		float PickupWeight = 0.0f;
		float BestD2 = GemRadiusUU * GemRadiusUU;
		for (const FVector2f& C : World.Chests)
		{
			const float D2 = FVector2f::DistSquared(C, S.Pos);
			if (D2 < BestD2)
			{
				BestD2 = D2;
				ToPickup = (C - S.Pos).GetSafeNormal();
				PickupWeight = ChestWeight;
			}
		}
		if (PickupWeight == 0.0f)
		{
			// Gems: the pull of every gem in range, nearer ones weighing more (sweeps clusters, not one gem at a time).
			FVector2f Pull = FVector2f::ZeroVector;
			for (const FVector2f& G : World.Gems)
			{
				const FVector2f To = G - S.Pos;
				const float D = To.Size();
				if (D < GemRadiusUU && D > UE_KINDA_SMALL_NUMBER)
				{
					Pull += To / (D * FMath::Max(D, GemNearUU));
				}
			}
			if (!Pull.IsNearlyZero())
			{
				ToPickup = Pull.GetSafeNormal();
				PickupWeight = GemWeight;
			}
		}

		// Candidates: 16 directions and standing still; the lowest score wins (ties: the lower index).
		FVector2f BestDir = FVector2f::ZeroVector;
		float BestScore = ScoreMove(World, FVector2f::ZeroVector, Speed);
		for (int32 Index = 0; Index < NumDirs; ++Index)
		{
			const FVector2f Dir = DirAt(Index);
			const float Danger = ScoreMove(World, Dir, Speed);
			float Score = Danger - KiteWeight * FVector2f::DotProduct(Dir, Away) - InertiaWeight * FVector2f::DotProduct(Dir, LastDir);
			if (PickupWeight > 0.0f && Danger < GemSafeDanger)
			{
				Score -= PickupWeight * FVector2f::DotProduct(Dir, ToPickup);
			}
			if (Score < BestScore)
			{
				BestScore = Score;
				BestDir = Dir;
			}
		}
		if (Config.Skill < 1.0f && !BestDir.IsNearlyZero())
		{
			BestDir = Rotate(BestDir, MoveRng.FRandRange(-1.0f, 1.0f) * (1.0f - Config.Skill) * MaxMoveNoiseDeg);
		}
		LastDir = BestDir;
		In.Move = BestDir;

		// Aim: Auto at full skill; below it, manual aim at the nearest enemy with noise.
		In.AimStick = FVector2f::ZeroVector;
		if (Config.Skill < 1.0f)
		{
			float NearD2 = TNumericLimits<float>::Max();
			FVector2f Aim = FVector2f::ZeroVector;
			for (const FVector2f& E : World.EnemyPos)
			{
				const float D2 = FVector2f::DistSquared(E, S.Pos);
				if (D2 < NearD2)
				{
					NearD2 = D2;
					Aim = (E - S.Pos).GetSafeNormal();
				}
			}
			if (!Aim.IsNearlyZero())
			{
				In.AimStick = Rotate(Aim, MoveRng.FRandRange(-1.0f, 1.0f) * (1.0f - Config.Skill) * MaxAimNoiseDeg);
			}
		}

		// Dash when a hit is imminent on the chosen path (after the reaction delay), a charge is ready and no dash runs.
		float Soonest = TNumericLimits<float>::Max();
		ThreatAlong(World, BestDir * Speed, ImminentS, &Soonest);
		bool bImminent = Soonest <= ImminentS;
		for (const FDTTelegraph& Tele : World.Telegraphs)
		{
			bImminent |= Tele.Progress >= 0.75f && InTelegraph(Tele, Clamp(S.Pos + BestDir * Speed * ImminentS), static_cast<float>(DTConst::HERO_RADIUS_UU));
		}
		ImminentSteps = bImminent ? ImminentSteps + 1 + Reaction : 0;
		if (bImminent && ImminentSteps > Reaction && S.DashCharges > 0 && S.DashLeft <= 0.0f && S.HurtIFramesLeft < ImminentS)
		{
			In.Move = ChooseDash(World);
			In.bDashPress = true;
			++Stats.DashPresses;
			ImminentSteps = 0;
		}

		if (S.Intercession01 >= 1.0f && Crowd >= IntercessionCrowd)
		{
			In.bIntercessionPress = true;
			++Stats.IntercessionPresses;
		}
	}
	Held = In;
	Held.bDashPress = false;
	Held.bIntercessionPress = false;
	return In;
}

// ---- cards ------------------------------------------------------------------------------------------------------------

float FDTBotBrain::CardScore(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const
{
	const EDTPath Second = Config.SecondPath == EDTPath::None ? FDTBotConfig::DefaultSecond(Config.Path) : Config.SecondPath;
	const FString Id = Card.ContentId.ToString();
	switch (Card.Kind)
	{
	case EDTCardKind::Keeper:
	{
		const EDTPath Path = FDTBotConfig::ParsePath(Id.RightChop(Id.Find(TEXT(".")) + 1));
		const EDTPath Want = Hand.HandKind == EDTHandKind::Patron ? Config.Path : Second;
		return Path == Want ? 200.0f : 10.0f;
	}
	case EDTCardKind::Blessing:
	case EDTCardKind::Reforging:
	{
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
		if (!Info.IsValid())
		{
			return 20.0f;
		}
		const FString Arch = FString(DTCat::Archetypes[Info.Archetype].Id);
		const bool bMine = Info.Path == Config.Path && !Config.Archetype.IsNone() && Arch.EndsWith(FString(TEXT(".")) + Config.Archetype.ToString());
		if (bMine)
		{
			return 100.0f - static_cast<float>(Info.Column) * 4.0f;
		}
		return Info.Path == Config.Path ? 40.0f : (Info.Path == Second ? 50.0f - static_cast<float>(Info.Column) : 30.0f);
	}
	case EDTCardKind::Weapon:
	{
		float Score = Player.Weapons.Num() < 3 ? 70.0f : 30.0f;
		for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
		{
			if (Card.ContentId == FName(DTCat::Weapons[Row].Id) && Config.Path != EDTPath::None
				&& (DTCat::Weapons[Row].Fits & (1u << static_cast<uint32>(Config.Path))) != 0)
			{
				Score += 10.0f;
			}
		}
		return Score;
	}
	case EDTCardKind::WeaponUpgrade: return 60.0f;
	case EDTCardKind::Mod: return 35.0f;
	case EDTCardKind::Reserve: return 5.0f;
	default: return 1.0f;
	}
}

int32 FDTBotBrain::ChooseCard(const FDTOfferHand& Hand, const FDTPlayerRunState& Player)
{
	if (Hand.Cards.Num() == 0)
	{
		return INDEX_NONE;
	}
	int32 Slot = 0;
	switch (Config.Policy)
	{
	case EDTBotPolicy::None:
		Slot = Hand.HandKind == EDTHandKind::Patron ? 0 : INDEX_NONE;
		break;
	case EDTBotPolicy::Random:
		Slot = CardRng.RandRange(0, Hand.Cards.Num() - 1);
		break;
	case EDTBotPolicy::Steered:
	{
		float Best = -TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Hand.Cards.Num(); ++Index)
		{
			const float Score = CardScore(Hand.Cards[Index], Hand, Player);
			if (Score > Best)
			{
				Best = Score;
				Slot = Index;
			}
		}
		break;
	}
	default:
		break;
	}
	if (Slot == INDEX_NONE)
	{
		++Stats.Declines;
	}
	else
	{
		++Stats.Picks;
	}
	return Slot;
}
