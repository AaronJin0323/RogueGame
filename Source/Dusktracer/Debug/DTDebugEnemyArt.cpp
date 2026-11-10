// Enemy art debug console command (R-TEC-12: compiled out of Shipping; 02_Systems/enemy_art.md T-EAR-07).
// DT.Enemies.LineUp stands the four role hulls side by side for a captured frame. Nothing here is a sim enemy: the line-up is a second
// ADTEnemyRenderer fed fixed transforms through the same placement and the same draw path as play, so the pool, the HUD and the seed do not move.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimTypes.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemyRenderer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTEnemyTypes.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/DTHeroSubsystem.h"

namespace DTDebugEnemyArt
{
	/** The roles that have a mesh of their own, in the line-up's column order (Tools/enemy_art_check.py reads the same order). */
	static const EDTEnemyRole LineUpRoles[] = { EDTEnemyRole::Swarm, EDTEnemyRole::Tank, EDTEnemyRole::Rusher, EDTEnemyRole::Turret };

	static TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADTEnemyRenderer>>& LineUps()
	{
		static TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADTEnemyRenderer>> Map;
		return Map;
	}

	static void LineUp(const TArray<FString>& Args, UWorld* World)
	{
		UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr;
		if (!Enemies)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Enemies.LineUp: game worlds only"));
			return;
		}
		TWeakObjectPtr<ADTEnemyRenderer>& Held = LineUps().FindOrAdd(World);
		if (ADTEnemyRenderer* Old = Held.Get())
		{
			Old->Destroy();
		}
		Held.Reset();
		if (Args.Num() > 0 && Args[0] == TEXT("0"))
		{
			UE_LOG(LogDT, Log, TEXT("DT.Enemies.LineUp removed"));
			return;
		}

		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTEnemyRenderer* Renderer = World->SpawnActor<ADTEnemyRenderer>(ADTEnemyRenderer::StaticClass(), FTransform::Identity, Params);
		if (!Renderer)
		{
			return;
		}
		Renderer->Setup(UDTEnemySubsystem::GetHullColor(), 0.0f, UDTEnemySubsystem::GetOpticColor(), DTConst::ENEMIES_OPTIC_EMISSIVE);
		Held = Renderer;

		// "noshadow": the hulls cast none, so a captured frame holds each hull's own outline and not the sun's (Tools/EnemyArt.ps1).
		if (Args.ContainsByPredicate([](const FString& Arg) { return Arg.Equals(TEXT("noshadow"), ESearchCase::IgnoreCase); }))
		{
			for (const EDTEnemyRole Role : LineUpRoles)
			{
				Renderer->GetRoleHull(Role)->SetCastShadow(false);
			}
			Renderer->GetHull()->SetCastShadow(false);
		}

		// One column per role across the screen (+Y), centred on hero 0; one row per facing. The first row stands two spacings up-screen
		// of the hero (+X), clear of the hero's shadow; the others stand down-screen, one spacing apart. No slot holds the hero.
		UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		const FDTHeroState* Hero = Heroes ? Heroes->GetState(0) : nullptr;
		const FVector2f Origin = Hero ? Hero->Pos : FVector2f::ZeroVector;
		const float Spacing = static_cast<float>(DTConst::EAR_LINEUP_SPACING_UU);
		const int32 Rows = DTConst::EAR_LINEUP_ROWS;
		const int32 Columns = UE_ARRAY_COUNT(LineUpRoles);
		TArray<FTransform> Hulls;
		TArray<FTransform> Optics;
		TArray<EDTEnemyRole> Roles;
		TArray<int32> OpticHulls;
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const EDTEnemyRole Role = LineUpRoles[Column];
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				const float X = Row == 0 ? 2.0f * Spacing : -Spacing * static_cast<float>(Row);
				const float Y = Spacing * (static_cast<float>(Column) - 0.5f * static_cast<float>(Columns - 1));
				const float Turn = UE_TWO_PI * static_cast<float>(Row) / static_cast<float>(Rows);
				FTransform Hull;
				FTransform Optic;
				UDTEnemySubsystem::PlaceParts(Origin + FVector2f(X, Y), FVector2f(FMath::Cos(Turn), FMath::Sin(Turn)), FDTEnemyRoleDef::Get(Role).Radius, Role,
					Renderer->GetMeshSizeUU(), Hull, Optic);
				OpticHulls.Add(Hulls.Add(Hull));
				Optics.Add(Optic);
				Roles.Add(Role);
			}
		}
		Renderer->UpdateInstances(Hulls, Roles, Optics, OpticHulls);
		UE_LOG(LogDT, Log, TEXT("DT.Enemies.LineUp: %d roles x %d facings, %.0f uu apart, round (%.0f,%.0f)%s"), Columns, Rows, Spacing, Origin.X, Origin.Y,
			Renderer->IsGreyBox() ? TEXT(" [grey box]") : TEXT(""));
	}

	static FAutoConsoleCommandWithWorldAndArgs LineUpCmd(TEXT("DT.Enemies.LineUp"),
		TEXT("DT.Enemies.LineUp [0|1] [noshadow]: stand the four role hulls round hero 0, one column per role and C.EAR.LINEUP_ROWS facings each (drawn only: no enemy is spawned); 0 removes them"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LineUp));
}

#endif // !UE_BUILD_SHIPPING
