// What an elite adds to its role: affix rolls, the light, attached parts, the label (02_Systems/elites.md). Plain C++.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Core/DTSimTypes.h"

class FDTEnemyPool;

/** One drawn elite part (elites.md R-ELT-10, R-ELT-11). Affix = Count marks the elite's light. */
struct FDTElitePart
{
	FTransform Transform;
	FLinearColor Color = FLinearColor::Black;
	float Emissive = 0.0f;
	EDTAffix Affix = EDTAffix::Count;
};

namespace DTElite
{
	/** RequestSpawnRole: roll C.ELITE.DEBUG_AFFIXES on the Elite stream instead of naming a mask. */
	inline constexpr uint8 RollMask = 0xFF;

	constexpr uint8 Bit(EDTAffix Affix) { return static_cast<uint8>(1u << static_cast<uint32>(Affix)); }
	constexpr bool Has(uint8 Mask, EDTAffix Affix) { return (Mask & Bit(Affix)) != 0; }

	/** R-ELT-02: Count distinct affixes Role may carry; favoured rows weigh FavorWeight (<= 0: one). */
	DUSKTRACER_API uint8 Roll(EDTEnemyRole Role, int32 Count, float FavorWeight, FRandomStream& Rng);
	/** R-ELT-10: the affix with the highest danger; Count for an empty mask. */
	DUSKTRACER_API EDTAffix Lead(uint8 Mask);
	/** R-ELT-10: the light pattern at Seconds, zero to one. */
	DUSKTRACER_API float LightLevel(uint8 Mask, float Seconds);
	/** R-ELT-10: the emissive at Seconds. */
	DUSKTRACER_API float LightEmissive(uint8 Mask, float Seconds);
	/** "Armored+Swift" or "none", tag root optional; false on an unknown name. */
	DUSKTRACER_API bool ParseMask(const FString& Text, uint8& OutMask);
	/** R-ELT-12: the mask's ST_Enemies keys, lead first. */
	DUSKTRACER_API void LabelKeys(uint8 Mask, TArray<FString>& OutKeys);
	/** R-ELT-12: the joined label. */
	DUSKTRACER_API FText LabelText(uint8 Mask);
	/** R-ELT-12: the live elites that carry a label: nearest to At first, at most C.ELITE.LABELS_MAX. */
	DUSKTRACER_API void LabelledElites(const FDTEnemyPool& Pool, const FVector2f& At, TArray<int32>& OutIndices);
	/** R-ELT-10, R-ELT-11: every part of every elite at render time. Reads the pool, never writes it. */
	DUSKTRACER_API void BuildParts(const FDTEnemyPool& Pool, float Alpha, float Seconds, float MeshSizeUU, TArray<FDTElitePart>& Out);
	/** "Armored,Swift" for logs and telemetry. */
	DUSKTRACER_API FString MaskNames(uint8 Mask);
}
