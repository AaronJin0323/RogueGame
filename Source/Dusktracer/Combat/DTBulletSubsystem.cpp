#include "Combat/DTBulletSubsystem.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/World.h"

bool UDTBulletSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTBulletSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDTSimSubsystem>();
}

void UDTBulletSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UDTSimSubsystem* SimSubsystem = InWorld.GetSubsystem<UDTSimSubsystem>())
	{
		SimSubsystem->Register(EDTSimPhase::Bullets, this);
		Sim = SimSubsystem;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Renderer = InWorld.SpawnActor<ADTBulletRenderer>(ADTBulletRenderer::StaticClass(), FTransform::Identity, Params);
	if (Renderer)
	{
		const FLinearColor Core = DTPal::Linear(DTPal::Player_Core);
		// tracer_art.md R-TRC-01: the head is Pal.Player.Core at C.BULLETS.EMISSIVE_PLAYER, the undimmed level.
		Renderer->InitTracerHead(Core, DTConst::BULLETS_EMISSIVE_PLAYER);
	}
}

void UDTBulletSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSubsystem = Sim.Get())
	{
		SimSubsystem->Unregister(this);
	}
	Sim.Reset();
	Renderer = nullptr;
	Pool.Reset();
	Super::Deinitialize();
}

void UDTBulletSubsystem::SimStep(const FDTSimContext& Context)
{
	if (Context.View)
	{
		Pool.Step(Context.StepSeconds, *Context.View);
	}
}

void UDTBulletSubsystem::SimRender(float Alpha)
{
	CSV_CUSTOM_STAT(DT, LiveBullets, Pool.Num(), ECsvCustomStatOp::Set);
	if (!Renderer)
	{
		return;
	}
	const int32 Count = Pool.Num();
	const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
	const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
	const TConstArrayView<float> Radii = Pool.GetRadii();
	const float InvMeshRadius = 1.0f / Renderer->GetMeshRadius();
	const double Z = DTConst::SIM_PLANE_Z_UU;
	// R-TRC-01, D-068: no head inside the hero's clear zone. The heroes are read once per frame, one squared distance per round.
	const DTTracerArt::FClearZone Clear = DTTracerArt::FClearZone::OfWorld(GetWorld(), Alpha);
	const TConstArrayView<int32> Owners = Pool.GetOwners();
	RenderTransforms.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// tracer_art.md R-TRC-01: drawn apart from the hit radius, stretched along the flight. Read from the pool, never written.
		const FVector2f P = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
		if (Clear.Hides(Owners[Index], P))
		{
			continue;
		}
		RenderTransforms.Add(DTTracerArt::Head(P, Pos[Index] - Prev[Index], Radii[Index], InvMeshRadius, Z));
	}
	Renderer->SetInstances(RenderTransforms);
	// R-TRC-06 (R-PRE-06): player fire dims with its own live count; one material parameter, no instance rewrite.
	Renderer->SetDim(DTTracerArt::HeadDim(Count));
}
