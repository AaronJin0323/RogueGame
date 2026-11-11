#include "Presentation/DTImpactArt.h"
#include "Core/DTConstantsGen.h"

namespace DTImpactArtLocal
{
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);

	static float Life01(float Age, float Life)
	{
		return Life > 0.0f ? FMath::Clamp(Age / Life, 0.0f, 1.0f) : 1.0f;
	}

	/** Seconds from step Born to the start of step Step, less the step that is drawing: what Expire measures an effect against. */
	static float AgeAtStart(int64 Born, int64 Step)
	{
		return static_cast<float>(Step - 1 - Born) / Hz;
	}
}

// ---- the tier -----------------------------------------------------------------------------------------------------------------

EDTDensityTier DTImpactArt::NextTier(EDTDensityTier Tier, float KillsPerSecond)
{
	const float CrowdOn = static_cast<float>(DTConst::PRE_DENSITY_LOD_KILLS_PER_S);
	const float CrowdOff = static_cast<float>(DTConst::IMP_DENSITY_CROWD_OFF_KILLS_PER_S);
	const float FloodOn = static_cast<float>(DTConst::IMP_DENSITY_FLOOD_KILLS_PER_S);
	const float FloodOff = static_cast<float>(DTConst::IMP_DENSITY_FLOOD_OFF_KILLS_PER_S);
	// Two passes: a rate that crosses both thresholds moves two tiers in one step. The bands make a second pass stop where it should
	// (Crowd entered from below is not left again, Crowd entered from above is not re-entered), so the result never oscillates.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		switch (Tier)
		{
		case EDTDensityTier::PerKill:
			if (KillsPerSecond > CrowdOn)
			{
				Tier = EDTDensityTier::Crowd;
				continue;
			}
			return Tier;
		case EDTDensityTier::Crowd:
			if (KillsPerSecond > FloodOn)
			{
				Tier = EDTDensityTier::Flood;
				continue;
			}
			if (KillsPerSecond < CrowdOff)
			{
				Tier = EDTDensityTier::PerKill;
				continue;
			}
			return Tier;
		case EDTDensityTier::Flood:
		default:
			if (KillsPerSecond < FloodOff)
			{
				Tier = EDTDensityTier::Crowd;
				continue;
			}
			return EDTDensityTier::Flood;
		}
	}
	return Tier;
}

const TCHAR* DTImpactArt::TierName(EDTDensityTier Tier)
{
	switch (Tier)
	{
	case EDTDensityTier::PerKill: return TEXT("PerKill");
	case EDTDensityTier::Crowd: return TEXT("Crowd");
	case EDTDensityTier::Flood: return TEXT("Flood");
	default: return TEXT("?");
	}
}

// ---- variation ----------------------------------------------------------------------------------------------------------------

uint32 DTImpactArt::Hash(uint32 Serial, int64 Step, int32 Index)
{
	// A 64-bit finaliser over the three words (splitmix64's): every bit of every input reaches every bit of the output.
	uint64 X = static_cast<uint64>(Serial) * 0x9E3779B97F4A7C15ull;
	X ^= static_cast<uint64>(Step) + 0x632BE59BD9B4E019ull + (X << 6) + (X >> 2);
	X ^= static_cast<uint64>(static_cast<uint32>(Index)) * 0xC2B2AE3D27D4EB4Full;
	X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
	X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
	X ^= X >> 31;
	return static_cast<uint32>(X ^ (X >> 32));
}

float DTImpactArt::Hash01(uint32 Serial, int64 Step, int32 Index)
{
	return static_cast<float>(Hash(Serial, Step, Index) >> 8) / static_cast<float>(1u << 24);
}

double DTImpactArt::NowSeconds(int64 StepsRun, float Alpha)
{
	return (static_cast<double>(StepsRun - 1) + static_cast<double>(Alpha)) / static_cast<double>(DTConst::SIM_FIXED_HZ);
}

float DTImpactArt::AgeSeconds(int64 Born, double Now)
{
	return FMath::Max(0.0f, static_cast<float>(Now - static_cast<double>(Born) / static_cast<double>(DTConst::SIM_FIXED_HZ)));
}

// ---- shapes -------------------------------------------------------------------------------------------------------------------

FVector2f DTImpactArt::SparkDirection(const FVector2f& Contact, const FVector2f& HullCentre, const FVector2f* OwnerHero)
{
	const FVector2f Out = Contact - HullCentre;
	if (Out.SizeSquared() > UE_KINDA_SMALL_NUMBER)
	{
		return Out.GetSafeNormal();
	}
	if (OwnerHero)
	{
		const FVector2f Away = Contact - *OwnerHero;
		if (Away.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			return Away.GetSafeNormal();
		}
	}
	return FVector2f(1.0f, 0.0f);
}

DTImpactArt::FDTSparkShape DTImpactArt::SparkAt(float Age)
{
	FDTSparkShape Out;
	const float Life = static_cast<float>(DTConst::IMP_SPARK_S);
	Out.bAlive = Age >= 0.0f && Age < Life;
	const float T = DTImpactArtLocal::Life01(Age, Life);
	const float Size = FMath::Lerp(static_cast<float>(DTConst::IMP_SPARK_START_RATIO), 1.0f, T);
	Out.Length = static_cast<float>(DTConst::IMP_SPARK_LENGTH_UU) * Size;
	Out.Width = static_cast<float>(DTConst::IMP_SPARK_WIDTH_UU) * Size;
	Out.Emissive = Out.bAlive ? static_cast<float>(DTConst::IMP_SPARK_EMISSIVE) * (1.0f - T) : 0.0f;
	return Out;
}

float DTImpactArt::DrawnRadius(float HitRadius, bool bElite)
{
	return HitRadius * (bElite ? static_cast<float>(DTConst::ELITE_DRAW_SCALE) : 1.0f);
}

DTImpactArt::FDTFlashShape DTImpactArt::FlashAt(float Drawn, float Age, bool bAllowFlash)
{
	FDTFlashShape Out;
	const float Life = static_cast<float>(DTConst::IMP_KILL_FLASH_S);
	Out.bAlive = Age >= 0.0f && Age < Life;
	const float T = DTImpactArtLocal::Life01(Age, Life);
	const float Widest = static_cast<float>(DTConst::IMP_FLASH_RADIUS_MAX_UU);
	const float Start = static_cast<float>(DTConst::IMP_KILL_FLASH_START_RATIO);
	const float End = static_cast<float>(DTConst::IMP_KILL_FLASH_END_RATIO);
	if (bAllowFlash)
	{
		Out.Radius = FMath::Min(Widest, Drawn * FMath::Lerp(Start, End, T));
		Out.Emissive = Out.bAlive ? static_cast<float>(DTConst::IMP_KILL_FLASH_EMISSIVE) * (1.0f - T) : 0.0f;
	}
	else
	{
		// R-IMP-09: a steady mark at its final size and the reduced strength, for its whole life.
		Out.Radius = FMath::Min(Widest, Drawn * End);
		Out.Emissive = Out.bAlive ? static_cast<float>(DTConst::IMP_KILL_FLASH_EMISSIVE * DTConst::IMP_REDUCED_EMISSIVE_RATIO) : 0.0f;
	}
	return Out;
}

int32 DTImpactArt::DebrisPieces(float Drawn)
{
	const int32 Pieces = FMath::RoundToInt32(Drawn / static_cast<float>(DTConst::IMP_DEBRIS_RADIUS_PER_PIECE_UU));
	return FMath::Clamp(Pieces, static_cast<int32>(DTConst::IMP_DEBRIS_PIECES_MIN), static_cast<int32>(DTConst::IMP_DEBRIS_PIECES_MAX));
}

FVector2f DTImpactArt::BlowDirection(const FVector2f& Contact, const FVector2f& HullCentre, uint32 Serial, int64 Step)
{
	const FVector2f Through = HullCentre - Contact;
	if (Through.SizeSquared() > UE_KINDA_SMALL_NUMBER)
	{
		return Through.GetSafeNormal();
	}
	const float Angle = UE_TWO_PI * Hash01(Serial, Step, -1);
	return FVector2f(FMath::Cos(Angle), FMath::Sin(Angle));
}

DTImpactArt::FDTPiece DTImpactArt::MakePiece(uint32 Serial, int64 Step, int32 Index, const FVector2f& Origin, const FVector2f& Blow)
{
	// Four independent draws per piece, each its own hash index, so no draw correlates with another.
	const int32 Base = 4 * Index;
	FDTPiece Piece;
	Piece.Origin = Origin;
	const float Spread = FMath::DegreesToRadians(static_cast<float>(DTConst::IMP_DEBRIS_SPREAD_DEG));
	const float Turn = Spread * (2.0f * Hash01(Serial, Step, Base) - 1.0f);
	const float C = FMath::Cos(Turn);
	const float S = FMath::Sin(Turn);
	Piece.Dir = FVector2f(Blow.X * C - Blow.Y * S, Blow.X * S + Blow.Y * C).GetSafeNormal();
	const float Variation = static_cast<float>(DTConst::IMP_DEBRIS_SPEED_VARIATION);
	Piece.Speed = static_cast<float>(DTConst::IMP_DEBRIS_SPEED_UU_S) * (1.0f + Variation * (2.0f * Hash01(Serial, Step, Base + 1) - 1.0f));
	Piece.Yaw = UE_TWO_PI * Hash01(Serial, Step, Base + 2);
	const uint32 Last = Hash(Serial, Step, Base + 3);
	Piece.Spin = (Last & 1u) != 0 ? 1.0f : -1.0f;
	Piece.Shard = static_cast<int32>((Last >> 1) % static_cast<uint32>(NumShards));
	return Piece;
}

DTImpactArt::FDTPieceAt DTImpactArt::PieceAt(const FDTPiece& Piece, float Age)
{
	FDTPieceAt Out;
	const float Life = static_cast<float>(DTConst::IMP_DEBRIS_S);
	const float Drag = static_cast<float>(DTConst::IMP_DEBRIS_DRAG_S);
	const float Clamped = FMath::Clamp(Age, 0.0f, Life);
	Out.bAlive = Age >= 0.0f && Age < Life;
	const float Travel = Piece.Speed * Drag * (1.0f - FMath::Exp(-Clamped / FMath::Max(Drag, UE_KINDA_SMALL_NUMBER)));
	Out.Position = Piece.Origin + Piece.Dir * Travel;
	// It turns through the angle it would roll through on its own half size: no rate of its own to tune.
	const float Half = 0.5f * static_cast<float>(DTConst::IMP_DEBRIS_SIZE_UU);
	Out.Yaw = Piece.Yaw + Piece.Spin * Travel / FMath::Max(Half, UE_KINDA_SMALL_NUMBER);
	const float Share = static_cast<float>(DTConst::IMP_DEBRIS_SHRINK_SHARE);
	const float ShrinkFrom = Life * (1.0f - Share);
	Out.Scale = Out.bAlive ? (Clamped <= ShrinkFrom ? 1.0f : FMath::Clamp((Life - Clamped) / FMath::Max(Life * Share, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f)) : 0.0f;
	return Out;
}

DTImpactArt::FDTRingShape DTImpactArt::RingAt(float Age, bool bAllowFlash)
{
	FDTRingShape Out;
	const float Life = static_cast<float>(DTConst::IMP_HERO_FLASH_S);
	Out.bAlive = Age >= 0.0f && Age < Life;
	const float T = DTImpactArtLocal::Life01(Age, Life);
	const float Hero = static_cast<float>(DTConst::HERO_RADIUS_UU);
	const float End = static_cast<float>(DTConst::IMP_HERO_FLASH_END_RATIO);
	Out.Thick = static_cast<float>(DTConst::IMP_HERO_FLASH_THICK_UU);
	if (bAllowFlash)
	{
		Out.Outer = Hero * FMath::Lerp(static_cast<float>(DTConst::IMP_HERO_FLASH_START_RATIO), End, T);
		Out.Emissive = Out.bAlive ? static_cast<float>(DTConst::IMP_HERO_FLASH_EMISSIVE) * (1.0f - T) : 0.0f;
	}
	else
	{
		Out.Outer = Hero * End;
		Out.Emissive = Out.bAlive ? static_cast<float>(DTConst::IMP_HERO_FLASH_EMISSIVE * DTConst::IMP_REDUCED_EMISSIVE_RATIO) : 0.0f;
	}
	return Out;
}

// ---- aggregation --------------------------------------------------------------------------------------------------------------

void DTImpactArt::Aggregate(TConstArrayView<FVector2f> Positions, TConstArrayView<float> Drawn, float CellUU, int32 Cap, TArray<FDTAggregate>& Out)
{
	Out.Reset();
	const float Inv = 1.0f / FMath::Max(CellUU, 1.0f);
	const int32 Count = FMath::Min(Positions.Num(), Drawn.Num());
	for (int32 Kill = 0; Kill < Count; ++Kill)
	{
		const FIntPoint Cell(FMath::FloorToInt32(Positions[Kill].X * Inv), FMath::FloorToInt32(Positions[Kill].Y * Inv));
		FDTAggregate* Found = Out.FindByPredicate([&Cell](const FDTAggregate& A) { return A.Cell == Cell; });
		if (!Found)
		{
			Found = &Out.AddDefaulted_GetRef();
			Found->Cell = Cell;
			Found->First = Kill;
		}
		// Centre and squared radius summed here, divided and rooted below.
		Found->Centre += Positions[Kill];
		Found->Drawn += Drawn[Kill] * Drawn[Kill];
		++Found->Count;
	}
	for (FDTAggregate& A : Out)
	{
		A.Centre /= static_cast<float>(FMath::Max(1, A.Count));
		A.Drawn = FMath::Sqrt(A.Drawn);
	}
	Out.StableSort([](const FDTAggregate& A, const FDTAggregate& B) { return A.Count != B.Count ? A.Count > B.Count : A.First < B.First; });
	if (Out.Num() > Cap)
	{
		Out.SetNum(FMath::Max(0, Cap), EAllowShrinking::No);
	}
}

bool DTImpactArt::IsCountedHit(const FDTImpact& Impact)
{
	return (Impact.Flags & (DTImpactFlags::FieldTick | DTImpactFlags::StatusTick | DTImpactFlags::Kill)) == 0;
}

bool DTImpactArt::IsKill(const FDTImpact& Impact)
{
	return (Impact.Flags & DTImpactFlags::Kill) != 0;
}

// ---- the live effects ---------------------------------------------------------------------------------------------------------

FDTImpactEffects::FDTImpactEffects()
{
	Reset();
}

void FDTImpactEffects::Reset()
{
	Sparks.Clear();
	Flashes.Clear();
	Pieces.Clear();
	for (int32 Player = 0; Player < RingLive.Num(); ++Player)
	{
		if (RingLive[Player])
		{
			RingLive[Player] = 0;
			RingsDied.Add(Player);
		}
	}
	// The slots stay with their instances (a renderer drains the deaths above); the caps are the live caps.
	Sparks.Cap = DTConst::IMP_SPARK_LIVE_MAX;
	Flashes.Cap = DTConst::IMP_FLASH_LIVE_MAX;
	Pieces.Cap = DTConst::IMP_DEBRIS_LIVE_MAX;
	RefusedSparks = 0;
	RefusedFlashes = 0;
	RefusedPieces = 0;
}

void FDTImpactEffects::ClearAll()
{
	const int64 Sp = RefusedSparks;
	const int64 Fl = RefusedFlashes;
	const int64 Pi = RefusedPieces;
	Reset();
	RefusedSparks = Sp;
	RefusedFlashes = Fl;
	RefusedPieces = Pi;
}

int32 FDTImpactEffects::NumRingsLive() const
{
	int32 Count = 0;
	for (const uint8 Live : RingLive)
	{
		Count += Live ? 1 : 0;
	}
	return Count;
}

void FDTImpactEffects::Expire(int64 Step)
{
	using namespace DTImpactArtLocal;
	const float SparkLife = static_cast<float>(DTConst::IMP_SPARK_S);
	for (int32 Slot = 0; Slot < Sparks.NumSlots(); ++Slot)
	{
		if (Sparks.IsLive(Slot) && AgeAtStart(Sparks.Items[Slot].Born, Step) >= SparkLife)
		{
			Sparks.Remove(Slot);
		}
	}
	const float FlashLife = static_cast<float>(DTConst::IMP_KILL_FLASH_S);
	for (int32 Slot = 0; Slot < Flashes.NumSlots(); ++Slot)
	{
		if (Flashes.IsLive(Slot) && AgeAtStart(Flashes.Items[Slot].Born, Step) >= FlashLife)
		{
			Flashes.Remove(Slot);
		}
	}
	const float PieceLife = static_cast<float>(DTConst::IMP_DEBRIS_S);
	for (int32 Slot = 0; Slot < Pieces.NumSlots(); ++Slot)
	{
		if (Pieces.IsLive(Slot) && AgeAtStart(Pieces.Items[Slot].Born, Step) >= PieceLife)
		{
			Pieces.Remove(Slot);
		}
	}
	const float RingLife = static_cast<float>(DTConst::IMP_HERO_FLASH_S);
	for (int32 Player = 0; Player < RingLive.Num(); ++Player)
	{
		if (RingLive[Player] && AgeAtStart(Rings[Player].Born, Step) >= RingLife)
		{
			RingLive[Player] = 0;
			RingsDied.Add(Player);
		}
	}
}

void FDTImpactEffects::RaiseFlash(const FVector2f& Centre, float Drawn, bool bAllowFlash, int64 Step, FDTImpactStepCounts& Counts)
{
	FDTFxFlash Flash;
	Flash.Centre = Centre;
	Flash.Drawn = Drawn;
	Flash.bAllowFlash = bAllowFlash;
	Flash.Born = Step;
	if (Flashes.Add(Flash) == INDEX_NONE)
	{
		++RefusedFlashes;
		return;
	}
	++Counts.Flashes;
}

FDTImpactStepCounts FDTImpactEffects::Feed(const FDTImpactStepInput& Input)
{
	using namespace DTImpactArt;
	Expire(Input.Step);
	FDTImpactStepCounts Counts;
	const EDTDensityTier Tier = Input.Tier;
	const int32 SparkCap = Tier == EDTDensityTier::PerKill ? DTConst::IMP_SPARKS_PER_STEP_MAX
		: (Tier == EDTDensityTier::Crowd ? DTConst::IMP_SPARKS_DENSE_PER_STEP_MAX : 0);
	SparkSerials.Reset();
	KillPositions.Reset();
	KillDrawn.Reset();

	for (const FDTImpact& Impact : Input.Impacts)
	{
		const bool bElite = (Impact.Flags & DTImpactFlags::Elite) != 0;
		if (IsKill(Impact))
		{
			++Counts.Kills;
			const float Drawn = DrawnRadius(Impact.EnemyRadius, bElite);
			if (Tier != EDTDensityTier::PerKill)
			{
				KillPositions.Add(Impact.EnemyPosition);
				KillDrawn.Add(Drawn);
				continue;
			}
			RaiseFlash(Impact.EnemyPosition, Drawn, Input.bAllowFlash, Input.Step, Counts);
			const FVector2f Blow = BlowDirection(Impact.Position, Impact.EnemyPosition, Impact.EnemySerial, Input.Step);
			const int32 Wanted = DebrisPieces(Drawn);
			for (int32 Index = 0; Index < Wanted; ++Index)
			{
				FDTFxPiece Piece;
				Piece.Piece = MakePiece(Impact.EnemySerial, Input.Step, Index, Impact.EnemyPosition, Blow);
				Piece.Born = Input.Step;
				if (Pieces.Add(Piece) == INDEX_NONE)
				{
					RefusedPieces += Wanted - Index;
					break;
				}
				++Counts.Pieces;
			}
			continue;
		}
		if (!IsCountedHit(Impact))
		{
			continue; // a tick is damage, not a blow (R-IMP-03)
		}
		++Counts.CountedHits;
		if (Counts.Sparks >= SparkCap)
		{
			continue;
		}
		if (Tier == EDTDensityTier::Crowd)
		{
			if (SparkSerials.Contains(Impact.EnemySerial))
			{
				continue; // one spark per enemy per step in a crowd
			}
			SparkSerials.Add(Impact.EnemySerial);
		}
		FDTFxSpark Spark;
		Spark.Contact = Impact.Position;
		const FVector2f* Owner = Input.HeroPositions.IsValidIndex(Impact.OwnerId) ? &Input.HeroPositions[Impact.OwnerId] : nullptr;
		Spark.Dir = SparkDirection(Impact.Position, Impact.EnemyPosition, Owner);
		Spark.Born = Input.Step;
		if (Sparks.Add(Spark) == INDEX_NONE)
		{
			++RefusedSparks;
			continue;
		}
		++Counts.Sparks;
	}

	if (Tier != EDTDensityTier::PerKill && KillPositions.Num() > 0)
	{
		const bool bFlood = Tier == EDTDensityTier::Flood;
		const float Cell = static_cast<float>(bFlood ? DTConst::IMP_AGG_FLOOD_CELL_UU : DTConst::IMP_AGG_CELL_UU);
		const int32 Cap = bFlood ? DTConst::IMP_AGG_FLOOD_PER_STEP_MAX : DTConst::IMP_AGG_PER_STEP_MAX;
		Aggregate(KillPositions, KillDrawn, Cell, Cap, Cells);
		for (const FDTAggregate& A : Cells)
		{
			RaiseFlash(A.Centre, A.Drawn, Input.bAllowFlash, Input.Step, Counts);
		}
	}

	// Hero rings (R-IMP-09): one per hurt hero, restarted by a second hit while it shows.
	const int32 Players = Input.HeroHurt.Num();
	if (Rings.Num() < Players)
	{
		Rings.SetNum(Players);
		RingLive.SetNumZeroed(Players);
	}
	for (int32 Player = 0; Player < Players; ++Player)
	{
		if (!Input.HeroHurt[Player])
		{
			continue;
		}
		Rings[Player].Born = Input.Step;
		Rings[Player].bAllowFlash = Input.bAllowFlash;
		RingLive[Player] = 1;
		RingsBorn.Add(Player);
		++Counts.Rings;
	}
	return Counts;
}
