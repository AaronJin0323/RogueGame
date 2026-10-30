// The weapon system in a game world (02_Systems/weapons.md, modifiers.md): owns FDTWeaponSim, registers it for
// EDTSimPhase::Weapons (fire), ::Bullets (after the pool: mirror rounds, capture impacts) and ::Effects (reactions),
// and draws drones, mines, fields and arcs as instances in palette tokens (R-WPN-11).
//
// Seams for later packages:
//   offers   - Give / LevelUp / Socket (EDTWeaponResult), DTModCompat::IsCompatible (Combat/DTWeaponCatalog.h),
//              IsInPool (starter column + DT.Unlock, D-013)
//   keepers  - AddFlagSource(OwnerId, SourceTag, FDTWeaponFlags) / RemoveFlagSource (Combat/DTWeaponFlags.h)
//   hero     - SetHeroStats (the FDTStatSet slice), SetAimManual (combat_core.md R-CMB-04)
//   telemetry- GetCounters (spawned, throttled, impacts) per player and slot
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DTBulletRenderer.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "DTWeaponSubsystem.generated.h"

class UDTWeaponSubsystem;
class UDTBulletSubsystem;

/** Palette tokens of weapon visuals (R-WPN-11, R-PRE-01): cores are Pal.Player.Core; zones the leading Keeper's colour. */
namespace DTWeaponPalette
{
	/** Renderer buckets: 0 core, 1 neutral zone, 2 + EDTPath for Keeper zones, then the Frozen wall plate (R-PRE-11). */
	DUSKTRACER_API int32 NumBuckets();
	DUSKTRACER_API int32 WallBucket();
	DUSKTRACER_API FColor BucketToken(int32 Bucket);
	DUSKTRACER_API FColor CoreToken();
	/** Pal.Keeper.<Path>, or Pal.Player.TrailNeutral for EDTPath::None. */
	DUSKTRACER_API FColor ZoneToken(EDTPath Path);
	DUSKTRACER_API int32 BucketOf(const FDTWeaponVisual& Visual);
}

/**
 * Control cues that draw as flat quads (04_Presentation R-PRE-11): one renderer of ADTTelegraphRenderer's shapes per bucket,
 * in cool tokens. Ground cues (the aura, the Stillness dial) lie under every telegraph; hull cues (the hold frame, the plain
 * Frozen diamond) ring the hull at its own height, so the camera's tilt cannot slide them off it. The Frozen wall is a flat
 * plate instead (DTWeaponPalette::WallBucket).
 */
namespace DTWeaponCues
{
	enum EBucket : int32 { Ground, Hold, Frozen, NumBuckets };
	DUSKTRACER_API bool IsCue(const FDTWeaponVisual& Visual);
	DUSKTRACER_API FColor Token(int32 Bucket);
	DUSKTRACER_API float Emissive(int32 Bucket);
	DUSKTRACER_API float DrawZ(int32 Bucket);
	DUSKTRACER_API int32 BucketOf(const FDTWeaponVisual& Visual);
	/** The footprint of one cue: the aura's ring ends at its reach and thickens inward on a pulse; the dial shows charges; a hold is a square, a plain Frozen a diamond. */
	DUSKTRACER_API void Append(const FDTWeaponVisual& Visual, TArray<FDTTelegraph>& Out);
}

/** Forwards one phase to the weapon subsystem (a system registers for one phase per object). */
class FDTWeaponPhaseAdapter : public IDTSimSystem
{
public:
	UDTWeaponSubsystem* Owner = nullptr;
	EDTSimPhase Phase = EDTSimPhase::Bullets;
	virtual void SimStep(const FDTSimContext& Context) override;
};

UCLASS()
class DUSKTRACER_API UDTWeaponSubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// IDTSimSystem (EDTSimPhase::Weapons)
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	/** EDTSimPhase::Bullets (after the bullet pool) and EDTSimPhase::Effects, through the adapters. */
	void StepPhase(EDTSimPhase Phase, const FDTSimContext& Context);

	// Inventory on the world's run (offers, debug).
	EDTWeaponResult Give(int32 OwnerId, FName WeaponId, int32 Level = 1);
	EDTWeaponResult LevelUp(int32 OwnerId, FName WeaponId, int32 Level = 0);
	EDTWeaponResult Socket(int32 OwnerId, FName WeaponId, FName ModId);

	// The flag seam (keepers).
	void AddFlagSource(int32 OwnerId, FName SourceTag, const FDTWeaponFlags& Flags) { Sim.AddFlagSource(OwnerId, SourceTag, Flags); }
	void RemoveFlagSource(int32 OwnerId, FName SourceTag) { Sim.RemoveFlagSource(OwnerId, SourceTag); }

	// Hero inputs.
	void SetHeroStats(int32 OwnerId, const FDTWeaponHeroStats& Stats) { Sim.SetHeroStats(OwnerId, Stats); }
	void SetAimManual(int32 OwnerId, bool bManual) { Sim.SetAimManual(OwnerId, bManual); }

	/** D-013: Gravity Mine, Fork and Ricochet reach the run pool only through DT.Unlock until meta (S3). */
	void Unlock(FName Id) { Unlocked.Add(Id); }
	/** True for a starter row (weapons.csv / modifiers.csv `starter`) or an unlocked id. */
	bool IsInPool(FName Id) const;

	FDTWeaponSim& GetSim() { return Sim; }
	const FDTWeaponSim& GetSim() const { return Sim; }
	/** Number of renderer actors spawned so far (lazily, on the first visual of each colour). */
	int32 GetRendererCount() const;

private:
	ADTBulletRenderer* EnsureRenderer(int32 Bucket);
	ADTTelegraphRenderer* EnsureCueRenderer(int32 Bucket);
	FDTRunState* GetRun() const;

	FDTWeaponSim Sim;
	FDTWeaponPhaseAdapter BulletsPhase;
	FDTWeaponPhaseAdapter EffectsPhase;
	TWeakObjectPtr<UDTSimSubsystem> SimSubsystem;
	TWeakObjectPtr<UDTBulletSubsystem> Bullets;
	TSet<FName> Unlocked;
	TArray<FDTWeaponVisual> Visuals;
	TArray<TArray<FTransform>> BucketTransforms;
	TArray<FDTTelegraph> CueShapes[DTWeaponCues::NumBuckets];

	/** Bucket 0 = Pal.Player.Core (rounds' cores: drones, mines); 1 = Pal.Player.TrailNeutral; 2.. = Pal.Keeper.<Path>. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADTBulletRenderer>> Renderers;

	/** One per DTWeaponCues bucket, spawned on the first cue of that bucket. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADTTelegraphRenderer>> CueRenderers;
};
