// The Reclaimer as drawn: one instanced mesh per rigid part, posed by DTHeroPose (02_Systems/hero_art.md R-HAR-02, R-HAR-05).
// Presentation: it reads a pose and draws; nothing in the sim knows it exists (04_Presentation R-PRE-11).
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Presentation/DTHeroPose.h"
#include "DTHeroFigure.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Attach to an actor standing on the floor at the hero's sim position. Build() loads SM_Hero_* and gives each part one instance on
 * M_InstanceColor in its palette token, lit by the scene and never self-lit; Apply() moves the parts to a pose. No skeleton, no
 * collision, no physics, no tick: the owner decides when a pose is applied.
 */
UCLASS()
class DUSKTRACER_API UDTHeroFigureComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDTHeroFigureComponent();

	/** Creates the parts. False, with a warning, when a mesh is missing: the figure then draws nothing. Safe to call again. */
	bool Build();
	void Apply(const FDTHeroPose& Pose);

	bool IsBuilt() const { return bBuilt; }
	/** Null before Build() or for a part that failed to load. */
	UInstancedStaticMeshComponent* GetPart(EDTHeroPart Part) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Parts;

	bool bBuilt = false;
};
