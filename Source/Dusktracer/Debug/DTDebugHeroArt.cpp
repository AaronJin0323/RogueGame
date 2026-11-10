// Hero art debug console command (R-TEC-12: compiled out of Shipping; 02_Systems/hero_art.md T-HAR-07).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/SceneComponent.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/DTHeroFigure.h"

namespace DTDebugHeroArt
{
	// The sheet's layout (a staged frame, not a tunable): four facings across, one row per pose, round the hero at the origin.
	constexpr int32 NumFacings = 4;
	constexpr float ColumnStepUU = 300.0f;
	constexpr float RowIdleX = 560.0f;
	constexpr float RowMoveX = 280.0f;
	constexpr float RowDashX = -300.0f;
	constexpr float QuarterTurnDeg = 90.0f;

	static void SpawnFigure(UWorld* World, const FVector& At, const FDTHeroPoseInput& Input)
	{
		AActor* Stand = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At));
		if (!Stand)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Stand, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Movable);
		Stand->SetRootComponent(Root);
		Root->RegisterComponent();
		Stand->SetActorLocation(At);
		UDTHeroFigureComponent* Figure = NewObject<UDTHeroFigureComponent>(Stand, TEXT("Figure"));
		Figure->SetupAttachment(Root);
		Figure->RegisterComponent();
		Figure->Build();
		Figure->Apply(DTHeroPose::Evaluate(Input));
	}

	/**
	 * DT.HeroArt.LineUp: stands twelve figures on the floor round the hero, four facings (up, right, down and left on the screen) in
	 * three poses (idle, moving at full speed along the facing, dashing along it). They are drawn and nothing else: no hero, no sim
	 * state, no collision. The staged frame of Tools/Scenes/hero_lineup.json.
	 */
	static void LineUp(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogDT, Warning, TEXT("DT.HeroArt.LineUp: game worlds only"));
			return;
		}
		for (int32 Facing = 0; Facing < NumFacings; ++Facing)
		{
			const float Radians = FMath::DegreesToRadians(QuarterTurnDeg * static_cast<float>(Facing));
			const FVector2f Dir(FMath::Cos(Radians), FMath::Sin(Radians));
			const float Y = (static_cast<float>(Facing) - 0.5f * static_cast<float>(NumFacings - 1)) * ColumnStepUU;
			FDTHeroPoseInput Idle;
			Idle.AimDir = Dir;
			FDTHeroPoseInput Moving = Idle;
			Moving.Vel = Dir * static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S);
			FDTHeroPoseInput Dashing = Idle;
			Dashing.bDashing = true;
			Dashing.DashDir = Dir;
			SpawnFigure(World, FVector(RowIdleX, Y, 0.0), Idle);
			SpawnFigure(World, FVector(RowMoveX, Y, 0.0), Moving);
			SpawnFigure(World, FVector(RowDashX, Y, 0.0), Dashing);
		}
		UE_LOG(LogDT, Log, TEXT("DT.HeroArt.LineUp: %d figures (facings up, right, down, left; rows idle, moving, dashing)"), NumFacings * 3);
	}

	static FAutoConsoleCommandWithWorldAndArgs LineUpCmd(TEXT("DT.HeroArt.LineUp"),
		TEXT("DT.HeroArt.LineUp: stand twelve drawn figures round the hero, four facings in the idle, moving and dashing poses (staged frames)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LineUp));
}

#endif // !UE_BUILD_SHIPPING
