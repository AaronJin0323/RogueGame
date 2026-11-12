// Enemy art debug console commands (R-TEC-12: compiled out of Shipping; 02_Systems/enemy_art.md T-EAR-07, T-EAR-08).
// DT.Enemies.LineUp stands the four role hulls side by side for a captured frame; DT.Enemies.EliteLineUp stands them as elites with their parts. Nothing here is a sim enemy: the line-up is a second
// ADTEnemyRenderer fed fixed transforms through the same placement and the same draw path as play, so the pool, the HUD and the seed do not move.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimTypes.h"
#include "Dusktracer.h"
#include "Enemies/DTElite.h"
#include "Enemies/DTEnemyPool.h"
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

	static TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADTEnemyRenderer>>& EliteLineUps()
	{
		static TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADTEnemyRenderer>> Map;
		return Map;
	}

	/**
	 * T-EAR-08: every role as an elite, one row per role (in LineUpRoles' order, down-screen), one column per affix alone and a last
	 * column with every affix, all turned the same way. Drawn only: the parts come from DTElite::BuildParts over a pool of this command's
	 * own, never the sim's, so the run, the HUD and the seed do not move. "cube" places the parts from the hit circle (the placement
	 * before R-EAR-08, as dt.Exp.EliteCubeAnchors 1 does in play).
	 */
	static void EliteLineUp(const TArray<FString>& Args, UWorld* World)
	{
		UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr;
		if (!Enemies)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Enemies.EliteLineUp: game worlds only"));
			return;
		}
		TWeakObjectPtr<ADTEnemyRenderer>& Held = EliteLineUps().FindOrAdd(World);
		if (ADTEnemyRenderer* Old = Held.Get())
		{
			Old->Destroy();
		}
		Held.Reset();
		if (Args.Num() > 0 && Args[0] == TEXT("0"))
		{
			UE_LOG(LogDT, Log, TEXT("DT.Enemies.EliteLineUp removed"));
			return;
		}
		const bool bCube = Args.ContainsByPredicate([](const FString& Arg) { return Arg.Equals(TEXT("cube"), ESearchCase::IgnoreCase); });

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

		UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		const FDTHeroState* Hero = Heroes ? Heroes->GetState(0) : nullptr;
		const FVector2f Origin = Hero ? Hero->Pos : FVector2f::ZeroVector;
		// Half again the line-up's slot: a shielded Tank's orbit is wider than one slot.
		const float Spacing = 1.5f * static_cast<float>(DTConst::EAR_LINEUP_SPACING_UU);
		// Turned up-screen and right, so the nose, one side and the rear all face the camera.
		const float Turn = FMath::DegreesToRadians(45.0f);
		const FVector2f Facing(FMath::Cos(Turn), FMath::Sin(Turn));
		const int32 Affixes = static_cast<int32>(EDTAffix::Count);
		const int32 Columns = Affixes + 1;
		const int32 Rows = UE_ARRAY_COUNT(LineUpRoles);
		uint8 Every = 0;
		for (int32 Affix = 0; Affix < Affixes; ++Affix)
		{
			Every |= DTElite::Bit(static_cast<EDTAffix>(Affix));
		}
		FDTEnemyPool Pool;
		TArray<FTransform> Hulls;
		TArray<EDTEnemyRole> Roles;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			// Two rows up-screen of the hero and two down-screen: no slot holds the hero.
			const float X = Spacing * (Row < Rows / 2 ? static_cast<float>(Rows / 2 - Row) : -static_cast<float>(Row - Rows / 2 + 1));
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const float Y = Spacing * (static_cast<float>(Column) - 0.5f * static_cast<float>(Columns - 1));
				FDTEnemySpawn Spec;
				Spec.Role = LineUpRoles[Row];
				Spec.Position = Origin + FVector2f(X, Y);
				Spec.bElite = true;
				Spec.AffixMask = Column < Affixes ? DTElite::Bit(static_cast<EDTAffix>(Column)) : Every;
				const int32 Index = Pool.Spawn(Spec, nullptr, false);
				Pool.SetFacing(Index, Facing);
				FTransform Hull;
				FTransform Optic;
				UDTEnemySubsystem::PlaceParts(Spec.Position, Facing, Pool.GetRadii()[Index] * static_cast<float>(DTConst::ELITE_DRAW_SCALE), Spec.Role, Renderer->GetMeshSizeUU(), Hull, Optic);
				Hulls.Add(Hull);
				Roles.Add(Spec.Role);
			}
		}
		// An elite has no optic of its own: its light is a part (elites.md R-ELT-10).
		Renderer->UpdateInstances(Hulls, Roles, TArray<FTransform>(), TArray<int32>());
		TArray<FDTElitePart> Parts;
		DTElite::BuildParts(Pool, 1.0f, 0.0f, Renderer->GetMeshSizeUU(), Parts, !bCube && !Renderer->IsGreyBox());
		Renderer->UpdateParts(Parts);
		UE_LOG(LogDT, Log, TEXT("DT.Enemies.EliteLineUp: %d roles x %d affix columns, %d parts, %.0f uu apart, round (%.0f,%.0f), %s%s"), Rows, Columns, Parts.Num(), Spacing,
			Origin.X, Origin.Y, bCube ? TEXT("hit-circle placement") : TEXT("role anchors"), Renderer->IsGreyBox() ? TEXT(" [grey box]") : TEXT(""));
	}

	static FAutoConsoleCommandWithWorldAndArgs EliteLineUpCmd(TEXT("DT.Enemies.EliteLineUp"),
		TEXT("DT.Enemies.EliteLineUp [0|1] [cube]: stand every role round hero 0 as an elite, one row per role and one column per affix plus one with every affix (drawn only: no enemy is spawned); cube places the parts from the hit circle; 0 removes them"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EliteLineUp));

	static FAutoConsoleCommandWithWorldAndArgs LineUpCmd(TEXT("DT.Enemies.LineUp"),
		TEXT("DT.Enemies.LineUp [0|1] [noshadow]: stand the four role hulls round hero 0, one column per role and C.EAR.LINEUP_ROWS facings each (drawn only: no enemy is spawned); 0 removes them"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LineUp));
}

#endif // !UE_BUILD_SHIPPING
