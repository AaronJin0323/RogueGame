// Enemy art tests (02_Systems/enemy_art.md T-EAR-01, T-EAR-02, T-EAR-03, T-EAR-06): the per-role render path, on a headless world.
// The shapes themselves are judged by Tools/enemy_art_check.py from their generators and from captured frames.

#include "Misc/AutomationTest.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTSimTypes.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/DTElite.h"
#include "Enemies/DTEnemyRenderer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTEnemyTypes.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTEnemyArtTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 ArtCopies = 3;
	constexpr float ArtTolerance = 0.01f;

	/** A headless game world with the sim and enemy subsystems up and begun. */
	struct FArtWorld
	{
		UWorld* World = nullptr;
		UDTEnemySubsystem* Enemies = nullptr;
		ADTEnemyRenderer* Renderer = nullptr;

		FArtWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTEnemyArtTestWorld")));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			Enemies = World->GetSubsystem<UDTEnemySubsystem>();
			Renderer = Enemies ? Enemies->GetRenderer() : nullptr;
		}

		~FArtWorld()
		{
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}

		/** ArtCopies of every role, the boss hull among them, each facing a different way. */
		void SpawnEveryRole()
		{
			for (int32 Role = 0; Role < ADTEnemyRenderer::NumRoles; ++Role)
			{
				for (int32 Copy = 0; Copy < ArtCopies; ++Copy)
				{
					Enemies->GetPool().Spawn(static_cast<EDTEnemyRole>(Role), FVector2f(600.0f * Role - 1500.0f, 400.0f * Copy + 800.0f));
				}
			}
		}
	};

	static bool Carries(const UInstancedStaticMeshComponent* Part, const FLinearColor& Color, float Emissive)
	{
		const int32 Stride = Part->NumCustomDataFloats;
		if (Stride < ADTEnemyRenderer::NumCustomData || Part->PerInstanceSMCustomData.Num() != Part->GetInstanceCount() * Stride)
		{
			return false;
		}
		for (int32 Instance = 0; Instance < Part->GetInstanceCount(); ++Instance)
		{
			const float* Data = &Part->PerInstanceSMCustomData[Instance * Stride];
			if (Data[0] != Color.R || Data[1] != Color.G || Data[2] != Color.B || Data[3] != Emissive)
			{
				return false;
			}
		}
		return true;
	}

	static const TCHAR* GreyBoxCVar = TEXT("dt.Exp.EnemyGreyBox");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEarRolePath, "Dusktracer.EnemyArt.RolePath", DTEnemyArtTest::Flags) // T-EAR-01
bool FDTEarRolePath::RunTest(const FString& Parameters)
{
	using namespace DTEnemyArtTest;
	FArtWorld W;
	if (!TestNotNull(TEXT("enemies"), W.Enemies) || !TestNotNull(TEXT("enemy renderer"), W.Renderer))
	{
		return false;
	}
	W.SpawnEveryRole();
	FDTEnemySpawn Elite;
	Elite.Role = EDTEnemyRole::Rusher;
	Elite.Position = FVector2f(0.0f, -900.0f);
	Elite.bElite = true;
	W.Enemies->GetPool().Spawn(Elite, nullptr, false);
	W.Enemies->SimRender(0.0f);

	TestFalse(TEXT("every role mesh is in the build: no mesh fallback"), W.Renderer->IsUsingFallbackMesh());
	TestFalse(TEXT("the grey box is not what is drawn"), W.Renderer->IsGreyBox());
	TestFalse(TEXT("the hull cubes are hidden"), W.Renderer->GetHull()->IsVisible());
	TestFalse(TEXT("the optic cubes are hidden"), W.Renderer->GetOptic()->IsVisible());

	const UStaticMesh* Cube = W.Renderer->GetHull()->GetStaticMesh();
	const UMaterialInterface* Material = W.Renderer->GetHull()->GetMaterial(0);
	const FLinearColor Body = DTPal::Linear(DTPal::Enemy_Body);
	const FLinearColor Lens = DTPal::Linear(DTPal::Enemy_Optic);
	int32 Hulls = 0;
	int32 Optics = 0;
	TSet<const UStaticMesh*> HullMeshes;
	for (int32 Index = 0; Index < ADTEnemyRenderer::NumRoles; ++Index)
	{
		const EDTEnemyRole Role = static_cast<EDTEnemyRole>(Index);
		const UInstancedStaticMeshComponent* Hull = W.Renderer->GetRoleHull(Role);
		const UInstancedStaticMeshComponent* Optic = W.Renderer->GetRoleOptic(Role);
		const FString What = FString::Printf(TEXT("role %d"), Index);
		const int32 ElitesHere = Role == EDTEnemyRole::Rusher ? 1 : 0;
		TestEqual(*(What + TEXT(": one hull per enemy of the role")), Hull->GetInstanceCount(), ArtCopies + ElitesHere);
		TestEqual(*(What + TEXT(": one optic per enemy of the role that is not an elite")), Optic->GetInstanceCount(), ArtCopies);
		TestTrue(*(What + TEXT(": drawn")), Hull->IsVisible() && Optic->IsVisible());
		TestTrue(*(What + TEXT(": hull = Pal.Enemy.Body with zero emissive")), Carries(Hull, Body, 0.0f));
		TestTrue(*(What + TEXT(": optic = Pal.Enemy.Optic at the optic emissive")), Carries(Optic, Lens, DTConst::ENEMIES_OPTIC_EMISSIVE));
		TestTrue(*(What + TEXT(": both on the grey box's material (M_InstanceColor)")), Hull->GetMaterial(0) == Material && Optic->GetMaterial(0) == Material);
		TestFalse(*(What + TEXT(": the optic casts no shadow")), Optic->CastShadow);
		if (Role == EDTEnemyRole::Boss)
		{
			TestTrue(*(What + TEXT(": the boss hull stays the cube")), Hull->GetStaticMesh() == Cube && Optic->GetStaticMesh() == Cube);
			TestTrue(*(What + TEXT(": the boss has no mesh key")), ADTEnemyRenderer::RoleMeshPath(Role, false).IsEmpty());
		}
		else
		{
			TestTrue(*(What + TEXT(": the hull is the role's own mesh")), Hull->GetStaticMesh() && Hull->GetStaticMesh()->GetPathName() == ADTEnemyRenderer::RoleMeshPath(Role, false));
			TestTrue(*(What + TEXT(": the optic is the role's own mesh")), Optic->GetStaticMesh() && Optic->GetStaticMesh()->GetPathName() == ADTEnemyRenderer::RoleMeshPath(Role, true));
			TestTrue(*(What + TEXT(": the hull is not the cube")), Hull->GetStaticMesh() != Cube);
		}
		HullMeshes.Add(Hull->GetStaticMesh());
		Hulls += Hull->GetInstanceCount();
		Optics += Optic->GetInstanceCount();
	}
	const int32 Live = W.Enemies->GetLiveCount();
	TestEqual(TEXT("one mesh per role"), HullMeshes.Num(), ADTEnemyRenderer::NumRoles);
	TestEqual(TEXT("drawn hulls: one per live enemy"), Hulls, Live);
	TestEqual(TEXT("drawn optics: one per live enemy that is not an elite"), Optics, Live - 1);
	TestEqual(TEXT("the grey box holds the same enemies"), W.Renderer->GetHull()->GetInstanceCount(), Live);
	TestEqual(TEXT("and the same optics"), W.Renderer->GetOptic()->GetInstanceCount(), Live - 1);

	W.Enemies->GetPool().Reset(false);
	W.Enemies->SimRender(0.0f);
	for (int32 Index = 0; Index < ADTEnemyRenderer::NumRoles; ++Index)
	{
		TestEqual(TEXT("instances follow the count down"), W.Renderer->GetRoleHull(static_cast<EDTEnemyRole>(Index))->GetInstanceCount() + W.Renderer->GetRoleOptic(static_cast<EDTEnemyRole>(Index))->GetInstanceCount(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEarPlacement, "Dusktracer.EnemyArt.Placement", DTEnemyArtTest::Flags) // T-EAR-02
bool FDTEarPlacement::RunTest(const FString& Parameters)
{
	using namespace DTEnemyArtTest;
	FArtWorld W;
	if (!TestNotNull(TEXT("enemies"), W.Enemies) || !TestNotNull(TEXT("enemy renderer"), W.Renderer))
	{
		return false;
	}
	FDTEnemyPool& Pool = W.Enemies->GetPool();
	const FVector2f At(700.0f, -300.0f);
	for (int32 Index = 0; Index < ADTEnemyRenderer::NumRoles; ++Index)
	{
		const EDTEnemyRole Role = static_cast<EDTEnemyRole>(Index);
		Pool.Reset(false);
		Pool.Spawn(Role, At);
		FDTEnemySpawn Elite;
		Elite.Role = Role;
		Elite.Position = At + FVector2f(0.0f, 500.0f);
		Elite.bElite = true;
		Pool.Spawn(Elite, nullptr, false);
		W.Enemies->SimRender(1.0f);

		const FString What = FString::Printf(TEXT("role %d"), Index);
		const float Radius = FDTEnemyRoleDef::Get(Role).Radius;
		TestEqual(*(What + TEXT(": the hit radius is the catalogue's")), Pool.GetRadii()[0], Radius);
		TestEqual(*(What + TEXT(": an elite keeps its hit radius")), Pool.GetRadii()[1], Radius);

		FTransform Box;
		FTransform OpticBox;
		FTransform Hull;
		FTransform Optic;
		FTransform EliteHull;
		W.Renderer->GetHull()->GetInstanceTransform(0, Box, true);
		W.Renderer->GetOptic()->GetInstanceTransform(0, OpticBox, true);
		W.Renderer->GetRoleHull(Role)->GetInstanceTransform(0, Hull, true);
		W.Renderer->GetRoleOptic(Role)->GetInstanceTransform(0, Optic, true);
		W.Renderer->GetRoleHull(Role)->GetInstanceTransform(1, EliteHull, true);
		const float Scale = 2.0f * Radius / W.Renderer->GetMeshSizeUU();
		TestEqual(*(What + TEXT(": drawn at the hit diameter per mesh unit")), static_cast<float>(Hull.GetScale3D().X), Scale, 0.0001f);
		TestTrue(*(What + TEXT(": the scale is uniform")), Hull.GetScale3D().AllComponentsEqual(0.0001));
		TestEqual(*(What + TEXT(": an elite's hull is drawn at C.ELITE.DRAW_SCALE")), static_cast<float>(EliteHull.GetScale3D().X), Scale * static_cast<float>(DTConst::ELITE_DRAW_SCALE), 0.0001f);
		TestEqual(*(What + TEXT(": over the enemy, X")), static_cast<float>(Hull.GetLocation().X), At.X, ArtTolerance);
		TestEqual(*(What + TEXT(": over the enemy, Y")), static_cast<float>(Hull.GetLocation().Y), At.Y, ArtTolerance);
		TestTrue(*(What + TEXT(": turned as the grey box is")), Hull.GetRotation().Equals(Box.GetRotation(), 0.0001));
		if (Role == EDTEnemyRole::Boss)
		{
			TestTrue(*(What + TEXT(": the boss hull is the grey box's cube, where it was")), Hull.Equals(Box, 0.0001));
			TestTrue(*(What + TEXT(": the boss optic is the grey box's optic cube")), Optic.Equals(OpticBox, 0.0001));
		}
		else
		{
			TestEqual(*(What + TEXT(": a role mesh stands on the floor")), static_cast<float>(Hull.GetLocation().Z), 0.0f, ArtTolerance);
			TestEqual(*(What + TEXT(": the cube stays centred on the sim plane")), static_cast<float>(Box.GetLocation().Z), static_cast<float>(DTConst::SIM_PLANE_Z_UU), ArtTolerance);
			TestTrue(*(What + TEXT(": the optic takes its hull's transform")), Optic.Equals(Hull, 0.0001));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEarMeshes, "Dusktracer.EnemyArt.Meshes", DTEnemyArtTest::Flags) // T-EAR-03
bool FDTEarMeshes::RunTest(const FString& Parameters)
{
	for (int32 Index = 0; Index < ADTEnemyRenderer::NumRoles; ++Index)
	{
		const EDTEnemyRole Role = static_cast<EDTEnemyRole>(Index);
		if (Role == EDTEnemyRole::Boss)
		{
			continue;
		}
		int32 Triangles = 0;
		for (const bool bOptic : { false, true })
		{
			const FString Path = ADTEnemyRenderer::RoleMeshPath(Role, bOptic);
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
			if (!TestNotNull(*Path, Mesh))
			{
				continue;
			}
			Triangles += Mesh->GetNumTriangles(0);
			TestEqual(*(Path + TEXT(": one level of detail (the triangle cap stands in for LODs)")), Mesh->GetNumLODs(), 1);
			TestEqual(*(Path + TEXT(": one material slot")), Mesh->GetStaticMaterials().Num(), 1);
#if WITH_EDITOR
			// UStaticMesh::IsNaniteEnabled is editor-only: the Development game target (gate step devgame) compiles this test without it.
			TestFalse(*(Path + TEXT(": Nanite off (04_Presentation §4)")), Mesh->IsNaniteEnabled());
#endif // WITH_EDITOR
			const FBox Box = Mesh->GetBoundingBox();
			TestTrue(*(Path + TEXT(": nothing under the floor")), Box.Min.Z >= -0.01);
			// One mesh unit is the hit diameter: a hull that did not reach the hit circle would mark it badly, one many times wider is another role.
			if (!bOptic)
			{
				const float Unit = 100.0f;
				TestTrue(*(Path + TEXT(": the hull reaches the hit circle and stays under twice its width")), Box.GetSize().GetMax() >= Unit && Box.GetSize().GetMax() <= 2.0f * Unit);
			}
		}
		TestTrue(*FString::Printf(TEXT("role %d: hull and optic within C.EAR.ROLE_TRIS_MAX (%d triangles)"), Index, Triangles), Triangles > 0 && Triangles <= DTConst::EAR_ROLE_TRIS_MAX);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEarGreyBox, "Dusktracer.EnemyArt.GreyBox", DTEnemyArtTest::Flags) // T-EAR-06
bool FDTEarGreyBox::RunTest(const FString& Parameters)
{
	using namespace DTEnemyArtTest;
	IConsoleVariable* GreyBox = IConsoleManager::Get().FindConsoleVariable(GreyBoxCVar);
	if (!TestNotNull(TEXT("dt.Exp.EnemyGreyBox exists outside Shipping"), GreyBox))
	{
		return false;
	}
	const int32 Before = GreyBox->GetInt();
	{
		FArtWorld W;
		if (TestNotNull(TEXT("enemies"), W.Enemies) && TestNotNull(TEXT("enemy renderer"), W.Renderer))
		{
			W.SpawnEveryRole();
			GreyBox->Set(1, ECVF_SetByCode);
			W.Enemies->SimRender(0.0f);
			TestTrue(TEXT("on: the grey box is what is drawn"), W.Renderer->IsGreyBox() && W.Renderer->GetHull()->IsVisible() && W.Renderer->GetOptic()->IsVisible());
			for (int32 Index = 0; Index < ADTEnemyRenderer::NumRoles; ++Index)
			{
				const EDTEnemyRole Role = static_cast<EDTEnemyRole>(Index);
				TestFalse(TEXT("on: no role mesh is drawn over it"), W.Renderer->GetRoleHull(Role)->IsVisible() || W.Renderer->GetRoleOptic(Role)->IsVisible());
			}
			TestEqual(TEXT("on: one cube per live enemy"), W.Renderer->GetHull()->GetInstanceCount(), W.Enemies->GetLiveCount());
			GreyBox->Set(0, ECVF_SetByCode);
			W.Enemies->SimRender(0.0f);
			TestFalse(TEXT("off: the role meshes are back"), W.Renderer->IsGreyBox() || W.Renderer->GetHull()->IsVisible());
			TestTrue(TEXT("off: a role hull is drawn"), W.Renderer->GetRoleHull(EDTEnemyRole::Swarm)->IsVisible());
		}
	}
	GreyBox->Set(Before, ECVF_SetByCode);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
