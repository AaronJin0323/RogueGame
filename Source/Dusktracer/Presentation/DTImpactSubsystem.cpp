#include "Presentation/DTImpactSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Presentation/DTImpactRenderer.h"
#include "Settings/DTUserSettings.h"

int32& UDTImpactSubsystem::DrawSetting()
{
	static int32 Draw = 1;
	return Draw;
}

bool UDTImpactSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTImpactSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Enemies = Collection.InitializeDependency<UDTEnemySubsystem>();
	HitWindow.SetNumZeroed(DTConst::SIM_FIXED_HZ);
	KillWindow.SetNumZeroed(DTConst::SIM_FIXED_HZ);
	ResetRun();
}

void UDTImpactSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
	}
	if (Renderer)
	{
		Renderer->Destroy();
		Renderer = nullptr;
	}
	Super::Deinitialize();
}

void UDTImpactSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	if (!Enemies.IsValid())
	{
		Enemies = InWorld.GetSubsystem<UDTEnemySubsystem>();
	}
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Register(EDTSimPhase::Director, this);
	}
	EnsureRenderer();
}

void UDTImpactSubsystem::EnsureRenderer()
{
	UWorld* World = GetWorld();
	if (Renderer || !World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Renderer = World->SpawnActor<ADTImpactRenderer>(ADTImpactRenderer::StaticClass(), FTransform::Identity, Params);
	if (Renderer)
	{
		Renderer->InitRender();
	}
}

void UDTImpactSubsystem::ResetRun()
{
	Effects.Reset();
	Tier = EDTDensityTier::PerKill;
	KillsPerSecond = 0.0f;
	DensityStep = -1;
	LastStep = -1;
	HeroPrev.Reset();
	HeroNow.Reset();
	for (int32& Count : HitWindow)
	{
		Count = 0;
	}
	for (int32& Count : KillWindow)
	{
		Count = 0;
	}
	WindowCursor = 0;
	HitsInWindow = 0;
	KillsInWindow = 0;
}

void UDTImpactSubsystem::SimStep(const FDTSimContext& Context)
{
	CSV_SCOPED_TIMING_STAT(DT, ImpactStep);
	// A run starts at step 0 (UDTSimSubsystem::StartRun): the tier is PerKill again and nothing of the last run is shown (R-IMP-11).
	if (Context.StepIndex <= LastStep)
	{
		ResetRun();
	}
	LastStep = Context.StepIndex;

	const UDTEnemySubsystem* EnemySystem = Enemies.Get();
	const FDTEnemyPool* Pool = EnemySystem ? &EnemySystem->GetPool() : nullptr;
	// R-IMP-08: the pool's own rate, read after this step's kills were counted (ResolveHits records them).
	KillsPerSecond = RateOverride >= 0.0f ? RateOverride : (Pool ? Pool->GetKillRate().GetKillsPerSecond() : 0.0f);
	Tier = DTImpactArt::NextTier(Tier, KillsPerSecond);
	DensityStep = Context.StepIndex;

	const FDTWorldView* View = Context.View;
	HeroPrev = HeroNow.Num() > 0 ? HeroNow : (View ? View->HeroPositions : TArray<FVector2f>());
	if (View)
	{
		HeroNow = View->HeroPositions;
	}

	FDTImpactStepInput Input;
	Input.Step = Context.StepIndex;
	Input.Impacts = Pool ? Pool->GetStepImpacts() : TConstArrayView<FDTImpact>();
	Input.HeroHurt = View ? TConstArrayView<bool>(View->HeroHurt) : TConstArrayView<bool>();
	Input.HeroPositions = HeroNow;
	Input.Tier = Tier;
	Input.bAllowFlash = DTSettings::AllowFlash();

	int32 Hits = 0;
	int32 Kills = 0;
	if (IsDrawing())
	{
		const FDTImpactStepCounts Counts = Effects.Feed(Input);
		Hits = Counts.CountedHits;
		Kills = Counts.Kills;
	}
	else
	{
		Effects.ClearAll();
		for (const FDTImpact& Impact : Input.Impacts)
		{
			Hits += DTImpactArt::IsCountedHit(Impact) ? 1 : 0;
			Kills += DTImpactArt::IsKill(Impact) ? 1 : 0;
		}
	}
	HitsInWindow += Hits - HitWindow[WindowCursor];
	KillsInWindow += Kills - KillWindow[WindowCursor];
	HitWindow[WindowCursor] = Hits;
	KillWindow[WindowCursor] = Kills;
	WindowCursor = (WindowCursor + 1) % HitWindow.Num();
}

void UDTImpactSubsystem::SimRender(float Alpha)
{
	EnsureRenderer();
	const UDTSimSubsystem* SimSystem = Sim.Get();
	if (!Renderer || !SimSystem)
	{
		return;
	}
	// Sim time of the drawn frame: the last step run, Alpha of the way from the one before (a held step draws alpha 1, R-IMP-03).
	const double Now = DTImpactArt::NowSeconds(SimSystem->GetStepIndex(), Alpha);
	const int32 Players = HeroNow.Num();
	HeroDrawn.SetNumUninitialized(Players, EAllowShrinking::No);
	for (int32 Player = 0; Player < Players; ++Player)
	{
		HeroDrawn[Player] = HeroPrev.IsValidIndex(Player) ? FMath::Lerp(HeroPrev[Player], HeroNow[Player], Alpha) : HeroNow[Player];
	}
	Renderer->Draw(Effects, Now, HeroDrawn);
}

FString UDTImpactSubsystem::DescribeStats() const
{
	return FString::Printf(TEXT("DT.Impact.Stats: step %lld tier %s rate %.1f kills/s; live sparks %d flashes %d pieces %d rings %d; last second hits %d kills %d; refused by cap sparks %lld flashes %lld pieces %lld; draw %d"),
		DensityStep, DTImpactArt::TierName(Tier), KillsPerSecond, Effects.Sparks.NumLive, Effects.Flashes.NumLive, Effects.Pieces.NumLive, Effects.NumRingsLive(),
		HitsInWindow, KillsInWindow, Effects.RefusedSparks, Effects.RefusedFlashes, Effects.RefusedPieces, DrawSetting());
}
