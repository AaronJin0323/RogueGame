#include "Bot/DTBotBrain.h"
#include "Keepers/DTReactions.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Misc/Crc.h"
#include "Offers/DTOfferGenerator.h"

// The bot's own heuristics. They are tool parameters of the measuring instrument, not game tuning: no gameplay value is
// read from here and changing them never changes the game (record a change in Docs/Validation/S1_bot_baseline.md).
namespace DTBotTuning
{
	constexpr int32 NumDirs = 16;
	constexpr float RoundWeight = 1000.0f;
	constexpr float BodyWeight = 400.0f;
	constexpr float TelegraphWeight = 700.0f;
	constexpr float HazardWeight = 900.0f;
	/** Outside a live hazard ring, the danger gains one HazardWeight step per this many uu. */
	constexpr float RingGradeUU = 60.0f;
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
	/** Intercession counts the enemies this near (how many: FDTBotSkillProfile::CastCrowd). */
	constexpr float IntercessionRadiusUU = 700.0f;
	/** Bot v1, skill 0: a decision every this many seconds of reaction delay; movement and aim noise in degrees. */
	constexpr float MaxReactionS = 0.25f;
	/** A duo card (reactions.md): above an off-archetype blessing, under the arm's own cards. */
	constexpr float DuoScore = 66.0f;
	constexpr float MaxMoveNoiseDeg = 35.0f;
	constexpr float MaxAimNoiseDeg = 20.0f;
	constexpr int32 SeedSalt = 0x0B07;
	/** Bot v2 (bot.md): how fast the stick error drifts; the range samples; what an unusable weapon loses; the second Keeper's scores. */
	constexpr float StickTauS = 0.5f;
	/** R-BOT-14: how far ahead a move is checked against the wall; about one held input. */
	constexpr float WallProbeUU = 10.0f;
	constexpr float ReachSampleS = 0.25f;
	constexpr float ReachWindowS = 60.0f;
	constexpr int32 ReachMinSamples = 20;
	/** The widest range the bot kites at (S1: nearest enemy 214-407 uu): reach over this stands in until samples exist. */
	constexpr float ReachPriorUU = 400.0f;
	constexpr float ReachPenalty = 60.0f;
	constexpr float ModReachShare = 0.5f;
	/** A second weapon still beats an upgrade whatever its reach (a hair over WeaponUpgrade's 60): reach chooses between weapons. */
	constexpr float SecondWeaponFloor = 61.0f;
	constexpr float SecondFirstScore = 94.0f;
	constexpr float SecondNextScore = 65.0f;
	/** The probe route: stand for ProbeLegS, walk a circle of ProbeOrbitUU for ProbeLegS, a dash every ProbeDashS. */
	constexpr int32 ProbeLegS = 1;
	constexpr int32 ProbeDashS = 6;
	constexpr float ProbeOrbitUU = 400.0f;
	/** With no enemy this close a walking leg goes to the nearest one: a player walks to a crowd a cast moved. The Arc Blade's reach at its top levels. */
	constexpr float ProbeReachUU = 300.0f;

	/** The wall band's share of FDTBotBrain::DangerAt. */
	static float WallDanger(const FVector2f& P)
	{
		const float Edge = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU - DTConst::HERO_RADIUS_UU) - FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
		if (Edge >= WallBandUU)
		{
			return 0.0f;
		}
		const float W = (WallBandUU - Edge) / WallBandUU;
		return WallWeight * W * W;
	}

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

	/**
	 * One object's share of ThreatAlong: D and T are its closest approach on this path. The time weight reads an approach at
	 * T == 0 at full strength and one still coming discounted, so read plainly a path that walks off a body inside the clearance
	 * scores worse than standing to wait for it. R-BOT-14 turns that off below skill 1.0 (Leave > 0): a path only leaving the
	 * object counts nothing, and a path that passes without touching counts its depth in the clearance, not a hit.
	 */
	static float PathThreat(float Weight, float T, float Horizon, float D, float Contact, float R, float Leave, float& Soonest)
	{
		if (D >= R || (T <= 0.0f && D >= R - Leave))
		{
			return 0.0f;
		}
		const bool bBrush = Leave > 0.0f && D >= Contact;
		if (!bBrush)
		{
			Soonest = FMath::Min(Soonest, T);
		}
		return Weight * (1.0f - T / (Horizon + 0.1f)) * ((bBrush ? 0.0f : 1.0f) + (R - D) / R);
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
	return FString::Printf(TEXT("policy=%s path=%s archetype=%s second=%s skill=%.2f seed=%d version=%d"), PolicyName(Policy),
		P < UE_ARRAY_COUNT(Names) ? Names[P] : TEXT("-"), *Archetype.ToString(), S < UE_ARRAY_COUNT(Names) ? Names[S] : TEXT("-"), Skill, Seed, Version);
}

int32 FDTBotConfig::KnownVersion(int32 Version)
{
	return Version == 2 ? 2 : 1;
}

// ---- brain ------------------------------------------------------------------------------------------------------------

void FDTBotBrain::Reset(const FDTBotConfig& InConfig)
{
	Config = InConfig;
	Config.Skill = FMath::Clamp(Config.Skill, 0.0f, 1.0f);
	Config.Version = FDTBotConfig::KnownVersion(Config.Version);
	Profile = Config.Version >= 2 ? FDTBotSkillProfile::FromSkill(Config.Skill) : FDTBotSkillProfile();
	Perception.Reset(Profile, Config.Seed);
	StickErr = 0.0f;
	ReachSamples.Reset();
	ReachNext = 0;
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
		if (Rel.SizeSquared() > Profile.ThreatRangeUU * Profile.ThreatRangeUU)
		{
			continue;
		}
		const FVector2f RVel = World.RoundVel.IsValidIndex(Index) ? World.RoundVel[Index] : FVector2f::ZeroVector;
		float T = 0.0f;
		const float D = ClosestApproach(Rel, RVel - Vel, Horizon, T);
		const float Contact = HeroR + (World.RoundRadius.IsValidIndex(Index) ? World.RoundRadius[Index] : 0.0f);
		Score += PathThreat(RoundWeight, T, Horizon, D, Contact, Contact + Profile.MarginUU, Profile.LeaveUU, Soonest);
	}
	for (int32 Index = 0; Index < World.EnemyPos.Num(); ++Index)
	{
		const FVector2f Rel = World.EnemyPos[Index] - P;
		if (Rel.SizeSquared() > Profile.ThreatRangeUU * Profile.ThreatRangeUU)
		{
			continue;
		}
		const FVector2f EVel = World.EnemyVel.IsValidIndex(Index) ? World.EnemyVel[Index] : FVector2f::ZeroVector;
		float T = 0.0f;
		const float D = ClosestApproach(Rel, EVel - Vel, Horizon, T);
		const float Contact = HeroR + (World.EnemyRadius.IsValidIndex(Index) ? World.EnemyRadius[Index] : 0.0f);
		Score += PathThreat(BodyWeight, T, Horizon, D, Contact, Contact + Profile.MarginUU, Profile.LeaveUU, Soonest);
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
		if (InTelegraph(Tele, P, HeroR + Profile.MarginUU))
		{
			if (Tele.Shape == EDTTelegraphShape::HazardRing)
			{
				// Graded by the distance outside: from past the look-ahead every move lands in the hazard, and a flat score would
				// leave only the kite, which points away from the boss, so outward (S1_boss_after_fields.md). The grade is steep: a
				// round in the way costs one hit, the live ring one every interval until it is crossed (S1_probes.md).
				const float Outside = FMath::Max(0.0f, FVector2f::Distance(P, Tele.Origin) - Tele.Length);
				Score += HazardWeight * (0.3f + Tele.Progress) * (1.0f + Outside / RingGradeUU);
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
	return Score + WallDanger(P);
}

FVector2f FDTBotBrain::OpenPart(const FVector2f& Pos, const FVector2f& Dir) const
{
	using namespace DTBotTuning;
	if (Profile.LeaveUU <= 0.0f)
	{
		return Dir;
	}
	return (Clamp(Pos + Dir * WallProbeUU) - Pos) / WallProbeUU;
}

float FDTBotBrain::ScoreMove(const FDTBotWorld& World, const FVector2f& Dir, float Speed) const
{
	using namespace DTBotTuning;
	const FVector2f Vel = Dir * Speed;
	float Score = ThreatAlong(World, Vel, Profile.HorizonS, nullptr);
	// Static danger at two look-ahead points (telegraphs land, the crowd closes in).
	for (const float T : { Profile.HorizonS * 0.5f, Profile.HorizonS })
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
		Score += ThreatAlong(After, FVector2f::ZeroVector, Profile.ImminentS, nullptr) * 0.5f;
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

FDTHeroInput FDTBotBrain::ProbeInput(const FDTBotWorld& World)
{
	using namespace DTBotTuning;
	const FDTHeroState& S = *World.Hero;
	FDTHeroInput In;
	const int64 Leg = World.StepIndex / (ProbeLegS * DTConst::SIM_FIXED_HZ);
	const bool bCasting = Config.CastDashSteps > 0 && Config.CastStep >= 0 && World.StepIndex >= Config.CastStep &&
		World.StepIndex < Config.CastStep + FMath::RoundToInt64(DTConst::INTERCESSION_FORGE_DURATION_S * DTConst::SIM_FIXED_HZ);
	const bool bDash = World.StepIndex % (ProbeDashS * DTConst::SIM_FIXED_HZ) == 0 || (bCasting && World.StepIndex % Config.CastDashSteps == 0);
	if (Leg % 2 == 1 || bDash)
	{
		FVector2f Nearest = S.Pos;
		float BestD2 = TNumericLimits<float>::Max();
		for (const FVector2f& E : World.EnemyPos)
		{
			const float D2 = FVector2f::DistSquared(E, S.Pos);
			if (D2 < BestD2)
			{
				BestD2 = D2;
				Nearest = E;
			}
		}
		const FVector2f Out = S.Pos.GetSafeNormal();
		const float Radial = FMath::Clamp((FVector2f(S.Pos).Size() - ProbeOrbitUU) / ProbeOrbitUU, -1.0f, 1.0f);
		const FVector2f Tangent(-Out.Y, Out.X);
		In.Move = World.EnemyPos.Num() > 0 && BestD2 > ProbeReachUU * ProbeReachUU ? (Nearest - S.Pos).GetSafeNormal()
			: (Out.IsNearlyZero() ? FVector2f(1.0f, 0.0f) : Tangent - Out * Radial).GetSafeNormal();
	}
	In.bDashPress = bDash && S.DashCharges > 0;
	Stats.DashPresses += In.bDashPress ? 1 : 0;
	if (Config.CastStep >= 0 && World.StepIndex >= Config.CastStep && Stats.IntercessionPresses == 0)
	{
		In.bIntercessionPress = true;
		++Stats.IntercessionPresses;
	}
	return In;
}

void FDTBotBrain::ChooseMove(const FDTBotWorld& World, float Speed, FVector2f& OutDir, int32& OutCrowd) const
{
	using namespace DTBotTuning;
	const FDTHeroState& S = *World.Hero;
	OutCrowd = 0;

	// Kite direction: away from the centroid of nearby enemies.
	FVector2f Centroid = FVector2f::ZeroVector;
	int32 Near = 0;
	for (const FVector2f& E : World.EnemyPos)
	{
		const float D2 = FVector2f::DistSquared(E, S.Pos);
		if (D2 < KiteRadiusUU * KiteRadiusUU)
		{
			Centroid += E;
			++Near;
		}
		OutCrowd += D2 < IntercessionRadiusUU * IntercessionRadiusUU ? 1 : 0;
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
	OutDir = FVector2f::ZeroVector;
	float BestScore = ScoreMove(World, FVector2f::ZeroVector, Speed);
	for (int32 Index = 0; Index < NumDirs; ++Index)
	{
		const FVector2f Dir = OpenPart(S.Pos, DirAt(Index));
		float Danger = ScoreMove(World, Dir, Speed);
		if (PickupWeight == ChestWeight)
		{
			// R-DIR-16 can leave a chest against a wall: the wall band gives way for one, nothing else does.
			for (const float T : { Profile.HorizonS * 0.5f, Profile.HorizonS })
			{
				Danger -= WallDanger(Clamp(S.Pos + Dir * Speed * T));
			}
		}
		float Score = Danger - KiteWeight * FVector2f::DotProduct(Dir, Away) - InertiaWeight * FVector2f::DotProduct(Dir, LastDir);
		if (PickupWeight > 0.0f && Danger < GemSafeDanger)
		{
			Score -= PickupWeight * FVector2f::DotProduct(Dir, ToPickup);
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			OutDir = DirAt(Index);
		}
	}
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
	if (Config.bProbeRoute)
	{
		return ProbeInput(World);
	}
	if (Config.Version >= 2)
	{
		return DecideV2(World);
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

		FVector2f BestDir = FVector2f::ZeroVector;
		int32 Crowd = 0;
		ChooseMove(World, Speed, BestDir, Crowd);
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
		ThreatAlong(World, BestDir * Speed, Profile.ImminentS, &Soonest);
		bool bImminent = Soonest <= Profile.ImminentS;
		for (const FDTTelegraph& Tele : World.Telegraphs)
		{
			bImminent |= Tele.Progress >= 0.75f && InTelegraph(Tele, Clamp(S.Pos + BestDir * Speed * Profile.ImminentS), static_cast<float>(DTConst::HERO_RADIUS_UU));
		}
		ImminentSteps = bImminent ? ImminentSteps + 1 + Reaction : 0;
		if (bImminent && ImminentSteps > Reaction && S.DashCharges > 0 && S.DashLeft <= 0.0f && S.HurtIFramesLeft < Profile.ImminentS)
		{
			In.Move = ChooseDash(World);
			In.bDashPress = true;
			++Stats.DashPresses;
			ImminentSteps = 0;
		}

		const bool bCast = Config.CastStep == FDTBotConfig::CastAuto ? S.Intercession01 >= 1.0f && Crowd >= Profile.CastCrowd
			: Config.CastStep >= 0 && World.StepIndex >= Config.CastStep && Stats.IntercessionPresses == 0;
		if (bCast)
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

void FDTBotBrain::SampleReach(const FDTBotWorld& Now)
{
	using namespace DTBotTuning;
	const int32 Every = FMath::Max(1, FMath::RoundToInt32(ReachSampleS * static_cast<float>(DTConst::SIM_FIXED_HZ)));
	if (Now.StepIndex % Every != 0)
	{
		return;
	}
	float Nearest = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Now.EnemyPos.Num(); ++Index)
	{
		const float Rim = FVector2f::Distance(Now.EnemyPos[Index], Now.Hero->Pos) - (Now.EnemyRadius.IsValidIndex(Index) ? Now.EnemyRadius[Index] : 0.0f);
		Nearest = FMath::Min(Nearest, FMath::Max(0.0f, Rim));
	}
	if (Nearest > KiteRadiusUU)
	{
		return; // nothing near: this sample says nothing about the range the bot fights at
	}
	const int32 Capacity = FMath::RoundToInt32(ReachWindowS / ReachSampleS);
	if (ReachSamples.Num() < Capacity)
	{
		ReachSamples.Add(Nearest);
	}
	else
	{
		ReachSamples[ReachNext] = Nearest;
		ReachNext = (ReachNext + 1) % Capacity;
	}
}

float FDTBotBrain::ReachUse(float Reach) const
{
	using namespace DTBotTuning;
	if (ReachSamples.Num() < ReachMinSamples)
	{
		return FMath::Min(1.0f, Reach / ReachPriorUU);
	}
	int32 Covered = 0;
	for (const float Sample : ReachSamples)
	{
		Covered += Sample <= Reach ? 1 : 0;
	}
	return static_cast<float>(Covered) / static_cast<float>(ReachSamples.Num());
}

FDTHeroInput FDTBotBrain::DecideV2(const FDTBotWorld& Now)
{
	using namespace DTBotTuning;
	const FDTHeroState& S = *Now.Hero;
	SampleReach(Now);
	Perception.Push(Now);
	FDTHeroInput In = Held;
	In.bDashPress = false;
	In.bIntercessionPress = false;

	if (Now.StepIndex >= NextDecideStep)
	{
		const int32 Hold = FMath::RoundToInt32(Profile.HoldS * static_cast<float>(DTConst::SIM_FIXED_HZ));
		NextDecideStep = Now.StepIndex + 1 + Hold;
		++Stats.Decisions;
		const FDTBotWorld World = Perception.Perceive(Now);
		const float Speed = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) * S.Stats.MoveSpeedMul;

		FVector2f BestDir = FVector2f::ZeroVector;
		int32 Crowd = 0;
		ChooseMove(World, Speed, BestDir, Crowd);
		if (Profile.StickErrDeg > 0.0f)
		{
			// R-BOT-07: a slow drift of unit spread times StickErrDeg (an Ornstein-Uhlenbeck step), so the stick is off for a while, not jittering.
			const float Rho = FMath::Exp(-static_cast<float>(1 + Hold) / (static_cast<float>(DTConst::SIM_FIXED_HZ) * StickTauS));
			const float Gauss = FMath::Sqrt(-2.0f * FMath::Loge(FMath::Max(MoveRng.FRand(), UE_KINDA_SMALL_NUMBER))) * FMath::Cos(2.0f * UE_PI * MoveRng.FRand());
			StickErr = Rho * StickErr + FMath::Sqrt(1.0f - Rho * Rho) * Profile.StickErrDeg * Gauss;
			if (!BestDir.IsNearlyZero())
			{
				BestDir = Rotate(BestDir, StickErr);
			}
		}
		LastDir = BestDir;
		In.Move = BestDir;
		In.AimStick = FVector2f::ZeroVector; // R-BOT-08: Auto aim at every skill

		float Soonest = TNumericLimits<float>::Max();
		ThreatAlong(World, OpenPart(S.Pos, BestDir) * Speed, Profile.ImminentS, &Soonest);
		bool bImminent = Soonest <= Profile.ImminentS;
		for (const FDTTelegraph& Tele : World.Telegraphs)
		{
			bImminent |= Tele.Progress >= 0.75f && InTelegraph(Tele, Clamp(S.Pos + BestDir * Speed * Profile.ImminentS), static_cast<float>(DTConst::HERO_RADIUS_UU));
		}
		if (bImminent && S.DashCharges > 0 && S.DashLeft <= 0.0f && S.HurtIFramesLeft < Profile.ImminentS)
		{
			// R-BOT-07: without the plan roll the dash goes where the hero was heading.
			if (Profile.DashPlanP >= 1.0f || MoveRng.FRand() < Profile.DashPlanP)
			{
				In.Move = ChooseDash(World);
			}
			In.bDashPress = true;
			++Stats.DashPresses;
		}

		const bool bCast = Config.CastStep == FDTBotConfig::CastAuto ? S.Intercession01 >= 1.0f && Crowd >= Profile.CastCrowd
			: Config.CastStep >= 0 && Now.StepIndex >= Config.CastStep && Stats.IntercessionPresses == 0;
		if (bCast)
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
			return DTReactions::FindDuo(Card.ContentId) != INDEX_NONE ? DTBotTuning::DuoScore : 20.0f;
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

float FDTBotBrain::WeaponReach(FName WeaponId, const FDTPlayerRunState& Player)
{
	for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
	{
		const DTCat::FWeaponRow& Weapon = DTCat::Weapons[Row];
		if (WeaponId != FName(Weapon.Id) || Weapon.NumLevels == 0)
		{
			continue;
		}
		const FDTWeaponInstance* Owned = Player.Weapons.FindByPredicate([WeaponId](const FDTWeaponInstance& W) { return W.Id == WeaponId; });
		const int32 Level = FMath::Clamp(Owned ? Owned->Level : 1, 1, Weapon.NumLevels);
		const DTCat::FWeaponLevelRow& Row2 = DTCat::WeaponLevels[Weapon.FirstLevel + Level - 1];
		return FCString::Strcmp(Weapon.Pattern, TEXT("MeleeArc")) == 0 ? Row2.Radius_uu : Row2.Range_uu;
	}
	return 0.0f;
}

float FDTBotBrain::CardTaste(FName ContentId) const
{
	if (Profile.TasteScore <= 0.0f)
	{
		return 0.0f;
	}
	const uint32 Crc = FCrc::StrCrc32(*ContentId.ToString());
	const FRandomStream Taste(static_cast<int32>(Crc ^ (static_cast<uint32>(Config.Seed) * 2654435761u)));
	return Taste.FRandRange(-1.0f, 1.0f) * Profile.TasteScore;
}

float FDTBotBrain::CardScoreV2(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const
{
	using namespace DTBotTuning;
	float Score = CardScore(Card, Hand, Player);
	const float Aware = Profile.FitAwareness;
	switch (Card.Kind)
	{
	case EDTCardKind::Keeper:
		return Score; // the arm's identity (R-BOT-11)
	case EDTCardKind::Weapon:
		Score -= Aware * ReachPenalty * (1.0f - ReachUse(WeaponReach(Card.ContentId, Player)));
		if (Player.Weapons.Num() < 2)
		{
			Score = FMath::Max(Score, SecondWeaponFloor);
		}
		break;
	case EDTCardKind::WeaponUpgrade:
		Score -= Aware * ReachPenalty * (1.0f - ReachUse(WeaponReach(Card.TargetWeapon.IsNone() ? Card.ContentId : Card.TargetWeapon, Player)));
		break;
	case EDTCardKind::Mod:
		Score -= Aware * ReachPenalty * ModReachShare * (1.0f - ReachUse(WeaponReach(Card.TargetWeapon, Player)));
		break;
	case EDTCardKind::Blessing:
	case EDTCardKind::Reforging:
	{
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
		const EDTPath Second = Player.Keepers.Second != EDTPath::None ? Player.Keepers.Second
			: (Config.SecondPath == EDTPath::None ? FDTBotConfig::DefaultSecond(Config.Path) : Config.SecondPath);
		if (Info.IsValid() && Info.Path == Second && Info.Path != Config.Path)
		{
			const bool bFirst = !Player.Keepers.Owned.ContainsByPredicate([Second](const FName& Id) { return DTOffers::FindBlessing(Id).Path == Second; });
			const float Floor = bFirst ? SecondFirstScore : SecondNextScore - static_cast<float>(Info.Column);
			Score += Aware * FMath::Max(0.0f, Floor - Score);
		}
		break;
	}
	default:
		break;
	}
	return Score + CardTaste(Card.ContentId);
}

float FDTBotBrain::ScoreCard(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const
{
	return Config.Version >= 2 ? CardScoreV2(Card, Hand, Player) : CardScore(Card, Hand, Player);
}

int32 FDTBotBrain::ReforgePick(const FDTOfferHand& Hand, int32 Seed)
{
	// R-BOT-15: Reforging cards hold their slot order (R-RFG-02), so walking the slots from the seed cycles the sources.
	TArray<int32> Slots;
	for (int32 Index = 0; Index < Hand.Cards.Num(); ++Index)
	{
		if (Hand.Cards[Index].Kind == EDTCardKind::Reforging)
		{
			Slots.Add(Index);
		}
	}
	if (Slots.Num() == 0)
	{
		return INDEX_NONE;
	}
	const int32 Cycle = DTConst::OFFER_REFORGING_CARDS;
	const int32 Want = ((Seed % Cycle) + Cycle) % Cycle;
	return Slots[Want % Slots.Num()];
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
		const int32 Reforge = Config.Version >= 2 && Hand.HandKind == EDTHandKind::Reforging ? ReforgePick(Hand, Config.Seed) : INDEX_NONE;
		if (Reforge != INDEX_NONE)
		{
			Slot = Reforge;
			break;
		}
		float Best = -TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Hand.Cards.Num(); ++Index)
		{
			const float Score = ScoreCard(Hand.Cards[Index], Hand, Player);
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
