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
		Renderer->InitRender(Core, DTConst::BULLETS_EMISSIVE_PLAYER);
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
	RenderTransforms.SetNumUninitialized(Count, EAllowShrinking::No);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2f P = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
		RenderTransforms[Index] = FTransform(FQuat::Identity, FVector(P.X, P.Y, Z), FVector(Radii[Index] * InvMeshRadius));
	}
	Renderer->SetInstances(RenderTransforms);
}
