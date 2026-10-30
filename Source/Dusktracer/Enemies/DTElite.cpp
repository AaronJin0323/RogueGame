#include "Enemies/DTElite.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Enemies/DTEnemyPool.h"

namespace DTEliteLocal
{
	constexpr int32 NumAffixes = static_cast<int32>(EDTAffix::Count);
	constexpr float LightTopDigit = 9.0f;
	// Grey-box layout, in multiples of the part size (elites.md R-ELT-11); the art book replaces it at S4.
	constexpr float VaneYawDeg = 30.0f;
	constexpr float VaneLength = 2.5f;
	constexpr float ThinPart = 0.5f;
	constexpr float ReactorSize = 1.5f;
	constexpr float SpikeLength = 2.5f;
	constexpr float ShieldOrbit = 2.0f;

	static const TCHAR* StringTableName = TEXT("ST_Enemies");
	static const TCHAR* StringTablePrefix = TEXT("ST_Enemies.");

	static FString NameKey(int32 Affix)
	{
		FString Key = DTCat::Affixes[Affix].NameKey;
		Key.RemoveFromStart(StringTablePrefix);
		return Key;
	}
}

uint8 DTElite::Roll(EDTEnemyRole Role, int32 Count, float FavorWeight, FRandomStream& Rng)
{
	using namespace DTEliteLocal;
	uint8 Mask = 0;
	for (int32 Pick = 0; Pick < Count; ++Pick)
	{
		float Weights[NumAffixes];
		float Sum = 0.0f;
		for (int32 Affix = 0; Affix < NumAffixes; ++Affix)
		{
			const DTCat::FAffixRow& Row = DTCat::Affixes[Affix];
			const bool bBarred = (Row.NoRoles & (1u << static_cast<uint32>(Role))) != 0 || Has(Mask, static_cast<EDTAffix>(Affix));
			Weights[Affix] = bBarred ? 0.0f : (Row.bFavored && FavorWeight > 0.0f ? FavorWeight : 1.0f);
			Sum += Weights[Affix];
		}
		if (Sum <= 0.0f)
		{
			break;
		}
		float X = Rng.GetFraction() * Sum;
		for (int32 Affix = 0; Affix < NumAffixes; ++Affix)
		{
			if (Weights[Affix] <= 0.0f)
			{
				continue;
			}
			if (X < Weights[Affix] || Affix == NumAffixes - 1)
			{
				Mask |= Bit(static_cast<EDTAffix>(Affix));
				break;
			}
			X -= Weights[Affix];
		}
	}
	return Mask;
}

EDTAffix DTElite::Lead(uint8 Mask)
{
	EDTAffix Best = EDTAffix::Count;
	int32 BestDanger = TNumericLimits<int32>::Min();
	for (int32 Affix = 0; Affix < DTEliteLocal::NumAffixes; ++Affix)
	{
		if (Has(Mask, static_cast<EDTAffix>(Affix)) && DTCat::Affixes[Affix].Danger > BestDanger)
		{
			BestDanger = DTCat::Affixes[Affix].Danger;
			Best = static_cast<EDTAffix>(Affix);
		}
	}
	return Best;
}

float DTElite::LightLevel(uint8 Mask, float Seconds)
{
	const EDTAffix Affix = Lead(Mask);
	if (Affix == EDTAffix::Count)
	{
		return 1.0f;
	}
	const TCHAR* Pattern = DTCat::Affixes[static_cast<int32>(Affix)].Light;
	const int32 Length = FCString::Strlen(Pattern);
	const int32 Index = FMath::FloorToInt32(FMath::Max(0.0f, Seconds) / DTConst::ELITE_BLINK_STEP_S) % FMath::Max(1, Length);
	return static_cast<float>(Pattern[Index] - TEXT('0')) / DTEliteLocal::LightTopDigit;
}

float DTElite::LightEmissive(uint8 Mask, float Seconds)
{
	return FMath::Lerp(static_cast<float>(DTConst::ENEMIES_OPTIC_EMISSIVE), static_cast<float>(DTConst::ELITE_LIGHT_EMISSIVE), LightLevel(Mask, Seconds));
}

bool DTElite::ParseMask(const FString& Text, uint8& OutMask)
{
	OutMask = 0;
	if (Text.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	TArray<FString> Names;
	Text.ParseIntoArray(Names, TEXT("+"));
	for (FString Name : Names)
	{
		Name.RemoveFromStart(TEXT("Enemy.Affix."), ESearchCase::IgnoreCase);
		bool bFound = false;
		for (int32 Affix = 0; Affix < DTEliteLocal::NumAffixes && !bFound; ++Affix)
		{
			if (Name.Equals(DTCat::Affixes[Affix].Id, ESearchCase::IgnoreCase))
			{
				OutMask |= Bit(static_cast<EDTAffix>(Affix));
				bFound = true;
			}
		}
		if (!bFound)
		{
			return false;
		}
	}
	return Names.Num() > 0;
}

void DTElite::LabelKeys(uint8 Mask, TArray<FString>& OutKeys)
{
	OutKeys.Reset();
	uint8 Left = Mask;
	for (EDTAffix Affix = Lead(Left); Affix != EDTAffix::Count; Affix = Lead(Left))
	{
		OutKeys.Add(DTEliteLocal::NameKey(static_cast<int32>(Affix)));
		Left &= static_cast<uint8>(~Bit(Affix));
	}
}

FText DTElite::LabelText(uint8 Mask)
{
	TArray<FString> Keys;
	LabelKeys(Mask, Keys);
	FText Out;
	for (const FString& Key : Keys)
	{
		const FText Name = FText::FromStringTable(DTEliteLocal::StringTableName, Key);
		Out = Out.IsEmpty() ? Name : FText::Format(FText::FromStringTable(DTEliteLocal::StringTableName, TEXT("Affix.Join")), Out, Name);
	}
	return Out;
}

void DTElite::LabelledElites(const FDTEnemyPool& Pool, const FVector2f& At, TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	const TConstArrayView<uint8> Flags = Pool.GetFlags();
	const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		if ((Flags[Index] & DTEnemyFlags::Elite) != 0 && Pool.GetAux(Index).AffixMask != 0)
		{
			OutIndices.Add(Index);
		}
	}
	OutIndices.Sort([&Pos, &At](int32 L, int32 R) { return FVector2f::DistSquared(Pos[L], At) < FVector2f::DistSquared(Pos[R], At); });
	if (OutIndices.Num() > DTConst::ELITE_LABELS_MAX)
	{
		OutIndices.SetNum(DTConst::ELITE_LABELS_MAX, EAllowShrinking::No);
	}
}

void DTElite::BuildParts(const FDTEnemyPool& Pool, float Alpha, float Seconds, float MeshSizeUU, TArray<FDTElitePart>& Out)
{
	using namespace DTEliteLocal;
	Out.Reset();
	const TConstArrayView<uint8> Flags = Pool.GetFlags();
	const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
	const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
	const TConstArrayView<FVector2f> Facing = Pool.GetFacings();
	const TConstArrayView<float> Radii = Pool.GetRadii();
	const TConstArrayView<EDTEnemyRole> Roles = Pool.GetRoles();
	const FLinearColor Body = DTPal::Linear(DTPal::Enemy_Body);
	const FLinearColor Lit = DTPal::Linear(DTPal::Elite_Light);
	const float Mesh = FMath::Max(UE_KINDA_SMALL_NUMBER, MeshSizeUU);
	const float Z = static_cast<float>(DTConst::SIM_PLANE_Z_UU);
	const float OpticScale = DTConst::ENEMIES_OPTIC_SCALE;

	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		if ((Flags[Index] & DTEnemyFlags::Elite) == 0)
		{
			continue;
		}
		const FDTEnemyAux& A = Pool.GetAux(Index);
		const FVector2f At = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
		const FVector2f F = Facing[Index];
		const FVector2f S(-F.Y, F.X);
		const float R = Radii[Index] * static_cast<float>(DTConst::ELITE_DRAW_SCALE);
		const float P = R * static_cast<float>(DTConst::ELITE_PART_RATIO);
		const float Yaw = F.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X));
		const float Emissive = LightEmissive(A.AffixMask, Seconds);
		auto Add = [&](EDTAffix Affix, const FVector2f& Where, float PartZ, const FVector& Size, float PartYaw, bool bLit)
		{
			FDTElitePart& Part = Out.AddDefaulted_GetRef();
			Part.Transform = FTransform(FRotator(0.0f, PartYaw, 0.0f), FVector(Where.X, Where.Y, PartZ), Size / Mesh);
			Part.Color = bLit ? Lit : Body;
			Part.Emissive = bLit ? Emissive : 0.0f;
			Part.Affix = Affix;
		};

		// The light sits where the role's optic sits (R-ENM-24), larger with the hull.
		const bool bCentred = Roles[Index] == EDTEnemyRole::Turret || Roles[Index] == EDTEnemyRole::Boss;
		const FVector2f LightAt = bCentred ? At : At + F * ((1.0f - OpticScale) * R);
		Add(EDTAffix::Count, LightAt, Z + R, FVector(2.0f * R * OpticScale), Yaw, true);

		if (Has(A.AffixMask, EDTAffix::Armored))
		{
			for (const float Side : { -1.0f, 1.0f })
			{
				Add(EDTAffix::Armored, At + S * (Side * (R + 0.5f * P)), Z, FVector(2.0f * R, P, R), Yaw, false);
			}
		}
		if (Has(A.AffixMask, EDTAffix::Swift))
		{
			for (const float Side : { -1.0f, 1.0f })
			{
				Add(EDTAffix::Swift, At - F * R + S * (Side * R), Z, FVector(VaneLength * P, ThinPart * P, ThinPart * P), Yaw + Side * VaneYawDeg, false);
			}
		}
		if (Has(A.AffixMask, EDTAffix::Volatile))
		{
			Add(EDTAffix::Volatile, At - F * (R + 0.5f * ReactorSize * P), Z, FVector(ReactorSize * P), Yaw, true);
		}
		if (Has(A.AffixMask, EDTAffix::Splitter))
		{
			const int32 Count = DTConst::AFFIX_SPLITTER_COUNT;
			for (int32 Dock = 0; Dock < Count; ++Dock)
			{
				const float Offset = (static_cast<float>(Dock) - 0.5f * static_cast<float>(Count - 1)) * 2.0f * P;
				Add(EDTAffix::Splitter, At - F * (R + P) + S * Offset, Z, FVector(P), Yaw, false);
			}
		}
		if (Has(A.AffixMask, EDTAffix::Vampiric))
		{
			Add(EDTAffix::Vampiric, At + F * (R + 0.5f * SpikeLength * P), Z, FVector(SpikeLength * P, ThinPart * P, ThinPart * P), Yaw, true);
		}
		if (A.ShieldHitsLeft > 0)
		{
			const int32 Total = DTConst::AFFIX_SHIELD_PLATES;
			const int32 Up = FMath::CeilToInt32(static_cast<float>(Total) * A.ShieldHitsLeft / static_cast<float>(DTConst::AFFIX_SHIELD_HITS));
			const float Orbit = FMath::DegreesToRadians(static_cast<float>(DTConst::ELITE_SHIELD_ORBIT_DEG_S) * Seconds);
			for (int32 Plate = 0; Plate < Up; ++Plate)
			{
				const float Angle = Orbit + UE_TWO_PI * static_cast<float>(Plate) / static_cast<float>(Total);
				const FVector2f Where = At + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (R + ShieldOrbit * P);
				Add(EDTAffix::Shielded, Where, Z, FVector(P), FMath::RadiansToDegrees(Angle), true);
			}
		}
	}
}

FString DTElite::MaskNames(uint8 Mask)
{
	FString Out;
	for (int32 Affix = 0; Affix < DTEliteLocal::NumAffixes; ++Affix)
	{
		if (Has(Mask, static_cast<EDTAffix>(Affix)))
		{
			Out += Out.IsEmpty() ? DTCat::Affixes[Affix].Id : *(FString(TEXT(",")) + DTCat::Affixes[Affix].Id);
		}
	}
	return Out;
}
