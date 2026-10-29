// The bullet system: owns the SoA pool, steps it in EDTSimPhase::Bullets, draws it as instances (02_Systems/bullets.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DTBulletPool.h"
#include "Combat/DTBulletRenderer.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "DTBulletSubsystem.generated.h"

/**
 * Game/PIE worlds only. Registers with UDTSimSubsystem on world begin play; weapons call Spawn() during
 * EDTSimPhase::Weapons, hits appear in FDTWorldView::Hits for EDTSimPhase::ResolveHits.
 */
UCLASS()
class DUSKTRACER_API UDTBulletSubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// IDTSimSystem
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	int32 Spawn(const FDTBulletSpawn& Params) { return Pool.Spawn(Params); }
	FDTBulletPool& GetPool() { return Pool; }
	const FDTBulletPool& GetPool() const { return Pool; }
	ADTBulletRenderer* GetRenderer() const { return Renderer; }

private:
	FDTBulletPool Pool;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TArray<FTransform> RenderTransforms;

	UPROPERTY(Transient)
	TObjectPtr<ADTBulletRenderer> Renderer;
};
