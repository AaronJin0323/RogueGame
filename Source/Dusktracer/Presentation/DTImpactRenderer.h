// Draws the live effects of impact_art.md (R-IMP-10): sparks one instanced component, flashes one, hero rings one, debris one per shard
// mesh; never an actor or a component per effect.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presentation/DTImpactArt.h"
#include "DTImpactRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Transient actor spawned by UDTImpactSubsystem; one per world. A spark, flash or ring is written once, when it is born: its transform and
 * its custom data (DTImpactArt::SparkData, FlashData, RingData) hold where it is and when it was born, and its material draws its growth and
 * fade from the scalar parameter Now (seconds of sim time) set once per component per frame. A death zeroes its instance's scale. A ring's
 * centre follows its hero, so a live ring's transform is written each frame. Debris is lit and moves in closed form of its age; its live
 * pieces' transforms are written each frame from DTImpactArt::PieceAt.
 */
UCLASS(NotPlaceable, Transient)
class DUSKTRACER_API ADTImpactRenderer : public AActor
{
	GENERATED_BODY()

public:
	ADTImpactRenderer();

	/** Loads the plane, the shard meshes and the four materials; a missing one leaves its effect undrawn, logged once. */
	void InitRender();

	/**
	 * Writes what changed since the last call: the births and deaths Effects lists (drained here), the rings' centres at Heroes and the
	 * debris at NowSeconds. NowSeconds is sim time (step index plus alpha). Reads nothing of the sim.
	 */
	void Draw(FDTImpactEffects& Effects, double NowSeconds, TConstArrayView<FVector2f> Heroes);

	UInstancedStaticMeshComponent* GetSparks() const { return Sparks; }
	UInstancedStaticMeshComponent* GetFlashes() const { return Flashes; }
	UInstancedStaticMeshComponent* GetRings() const { return Rings; }
	UInstancedStaticMeshComponent* GetDebris(int32 Shard) const { return Debris.IsValidIndex(Shard) ? Debris[Shard].Get() : nullptr; }
	bool HasSparkMaterial() const { return bSpark; }
	bool HasFlashMaterial() const { return bFlash; }
	bool HasRingMaterial() const { return bRing; }
	bool HasDebris() const { return bDebris; }
	/** The spark material's StreakRatio parameter as set (R-IMP-05), or -1 without the material. */
	float GetSparkStreakRatio() const;
	/** Instances of Part whose scale is not zero: what is on screen. */
	static int32 CountShown(const UInstancedStaticMeshComponent* Part);

private:
	void WriteSparks(FDTImpactEffects& Effects);
	void WriteFlashes(FDTImpactEffects& Effects);
	void WriteRings(FDTImpactEffects& Effects, TConstArrayView<FVector2f> Heroes);
	void WriteDebris(FDTImpactEffects& Effects, double NowSeconds);
	/** Grows Part to at least Count instances, every new one hidden. */
	static void EnsureSlots(UInstancedStaticMeshComponent* Part, int32 Count, TArray<FTransform>& Scratch);
	static void Hide(UInstancedStaticMeshComponent* Part, int32 Slot);
	void SetNow(UMaterialInstanceDynamic* Material, double NowSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Sparks;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Flashes;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Rings;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Debris;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SparkMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	bool bSpark = false;
	bool bFlash = false;
	bool bRing = false;
	bool bDebris = false;
	bool bDebrisShown = false;
	TArray<FTransform> AddScratch;
	TArray<FTransform> DebrisScratch[DTImpactArt::NumShards];
	TArray<float> DataScratch;
};
