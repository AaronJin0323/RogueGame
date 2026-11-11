// Impact art tests (02_Systems/impact_art.md T-IMP-02 to T-IMP-11; T-PRE-06, the kill half). The frames are read by gate step contrast on
// Tools/Scenes/impact_dense.json and by Tools/ImpactArt.ps1; what only a person can judge is in the spec's human line.

#include "Misc/AutomationTest.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyBulletRenderer.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemyRenderer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "Presentation/DTImpactArt.h"
#include "Presentation/DTImpactRenderer.h"
#include "Presentation/DTImpactSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTImpactArtTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);
	constexpr float Tolerance = 1.0e-3f;
	/** The settings screen's panel width and hint scale on its 1080-line reference canvas (Settings/DTMenuScreen.cpp PanelW, HintScale). */
	constexpr float MenuPanelWidth = 1120.0f;
	constexpr float MenuHintScale = 1.25f;

	/** A headless game world with every world subsystem up and begun (as Tests/Presentation/DTTracerArtTests.cpp builds it). */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTImpactArtTestWorld")));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FTestWorld()
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

		int32 CountActors() const
		{
			int32 Count = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				++Count;
			}
			return Count;
		}
	};

	/** Restores dt.Impact.Draw however a test ends. */
	struct FKeepSettings
	{
		int32 Draw = UDTImpactSubsystem::DrawSetting();
		~FKeepSettings()
		{
			UDTImpactSubsystem::DrawSetting() = Draw;
		}
	};

	static FDTImpact Hit(uint32 Serial, const FVector2f& Contact, const FVector2f& Centre, float Radius, uint8 ImpactFlags = 0)
	{
		FDTImpact Impact;
		Impact.Position = Contact;
		Impact.EnemyPosition = Centre;
		Impact.EnemyRadius = Radius;
		Impact.Dealt = 1.0f;
		Impact.MaxHp = 10.0f;
		Impact.EnemySerial = Serial;
		Impact.OwnerId = 0;
		Impact.WeaponSlot = 0;
		Impact.Flags = ImpactFlags;
		return Impact;
	}

	static FDTImpactStepInput Input(int64 Step, TConstArrayView<FDTImpact> Impacts, EDTDensityTier Tier, TConstArrayView<bool> Hurt = {}, TConstArrayView<FVector2f> Heroes = {}, bool bAllowFlash = true)
	{
		FDTImpactStepInput In;
		In.Step = Step;
		In.Impacts = Impacts;
		In.Tier = Tier;
		In.HeroHurt = Hurt;
		In.HeroPositions = Heroes;
		In.bAllowFlash = bAllowFlash;
		return In;
	}

	static bool SameImpact(const FDTImpact& A, const FDTImpact& B)
	{
		return A.Position == B.Position && A.EnemyPosition == B.EnemyPosition && A.EnemyRadius == B.EnemyRadius && A.Dealt == B.Dealt && A.MaxHp == B.MaxHp
			&& A.EnemySerial == B.EnemySerial && A.OwnerId == B.OwnerId && A.SourceId == B.SourceId && A.WeaponSlot == B.WeaponSlot && A.Role == B.Role && A.Flags == B.Flags;
	}

	/** Every transform and custom float of Part, for an equality test. */
	static TArray<float> Contents(const UInstancedStaticMeshComponent* Part)
	{
		TArray<float> Out;
		if (!Part)
		{
			return Out;
		}
		FTransform At;
		for (int32 Index = 0; Index < Part->GetInstanceCount(); ++Index)
		{
			Part->GetInstanceTransform(Index, At);
			const FVector L = At.GetLocation();
			const FVector S = At.GetScale3D();
			const FQuat R = At.GetRotation();
			Out.Append({ static_cast<float>(L.X), static_cast<float>(L.Y), static_cast<float>(L.Z), static_cast<float>(S.X), static_cast<float>(S.Y), static_cast<float>(S.Z),
				static_cast<float>(R.X), static_cast<float>(R.Y), static_cast<float>(R.Z), static_cast<float>(R.W) });
		}
		Out.Append(Part->PerInstanceSMCustomData);
		return Out;
	}

	/** What the pool, the view and the run hold that a reader must leave as it was. */
	struct FSimSnapshot
	{
		TArray<FVector2f> Pos;
		TArray<float> Hp;
		TArray<uint32> Serials;
		int32 PoolImpacts = 0;
		int32 ViewHits = 0;
		int32 ViewKills = 0;
		TArray<FVector2f> Heroes;
		TArray<bool> Hurt;
		int64 Damage = 0;
		int32 Kills = 0;

		static FSimSnapshot Take(const FDTEnemyPool& Pool, const FDTWorldView& View, const FDTRunState& Run)
		{
			FSimSnapshot S;
			S.Pos = TArray<FVector2f>(Pool.GetPositions());
			S.Hp = TArray<float>(Pool.GetHp());
			S.Serials = TArray<uint32>(Pool.GetSerials());
			S.PoolImpacts = Pool.GetStepImpacts().Num();
			S.ViewHits = View.Hits.Num();
			S.ViewKills = View.Kills.Num();
			S.Heroes = View.HeroPositions;
			S.Hurt = View.HeroHurt;
			for (int32 Player = 0; Player < Run.GetNumPlayers(); ++Player)
			{
				S.Damage += Run.GetPlayer(Player).DamageDealt;
				S.Kills += Run.GetPlayer(Player).Kills;
			}
			return S;
		}

		bool operator==(const FSimSnapshot& O) const
		{
			return Pos == O.Pos && Hp == O.Hp && Serials == O.Serials && PoolImpacts == O.PoolImpacts && ViewHits == O.ViewHits && ViewKills == O.ViewKills
				&& Heroes == O.Heroes && Hurt == O.Hurt && Damage == O.Damage && Kills == O.Kills;
		}
	};

	/** Weapons phase stand-in: every step, one weak hit on each of the first Count enemies, as a weapon emits them. */
	struct FShooter : public IDTSimSystem
	{
		UDTEnemySubsystem* Enemies = nullptr;
		int32 Count = 0;
		virtual void SimStep(const FDTSimContext& Context) override
		{
			const FDTEnemyPool& Pool = Enemies->GetPool();
			for (int32 Index = 0; Index < FMath::Min(Count, Pool.Num()); ++Index)
			{
				FDTHit& Out = Context.View->Hits.AddDefaulted_GetRef();
				Out.EnemyIndex = Index;
				Out.EnemySerial = Pool.GetSerials()[Index];
				Out.OwnerId = 0;
				Out.Damage = 0.01f;
				Out.Position = Pool.GetPositions()[Index] + FVector2f(Pool.GetRadii()[Index], 0.0f);
				Out.WeaponSlot = 0;
			}
		}
	};

	/** A reader as R-IMP-02 has one: registered after ResolveHits, it copies the step list and writes nothing. */
	struct FReader : public IDTSimSystem
	{
		UDTEnemySubsystem* Enemies = nullptr;
		TArray<TArray<FDTImpact>> Seen;
		virtual void SimStep(const FDTSimContext& Context) override
		{
			Seen.Add(TArray<FDTImpact>(Enemies->GetPool().GetStepImpacts()));
		}
	};

	/** Takes the sim's snapshot before the readers of a step and compares after them. */
	struct FWatch : public IDTSimSystem
	{
		UDTEnemySubsystem* Enemies = nullptr;
		FSimSnapshot* Into = nullptr;
		bool bCheck = false;
		int32 Differ = 0;
		virtual void SimStep(const FDTSimContext& Context) override
		{
			const FSimSnapshot Now = FSimSnapshot::Take(Enemies->GetPool(), *Context.View, *Context.Run);
			if (bCheck)
			{
				Differ += Now == *Into ? 0 : 1;
			}
			else
			{
				*Into = Now;
			}
		}
	};

	static int32 SpawnSoaks(FDTEnemyPool& Pool, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FDTEnemySpawn Spawn;
			Spawn.Role = EDTEnemyRole::Tank;
			Spawn.Position = FVector2f(-900.0f + 300.0f * Index, 1500.0f);
			Spawn.HpScale = 1000.0f;
			Pool.Spawn(Spawn, nullptr, false);
		}
		return Pool.Num();
	}
}

// T-IMP-02: two readers registered after ResolveHits see the whole list of every step of a frame that ran several, the same records in the
// same order, and leave the pool, the view and the run as they were.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactReaders, "Dusktracer.Impact.Readers", DTImpactArtTest::Flags)
bool FDTImpactReaders::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies))
	{
		return false;
	}
	const int32 Count = SpawnSoaks(Enemies->GetPool(), 5);
	FShooter Shooter;
	Shooter.Enemies = Enemies;
	Shooter.Count = Count;
	FReader First;
	First.Enemies = Enemies;
	FReader Second;
	Second.Enemies = Enemies;
	FSimSnapshot Before;
	FWatch Take;
	Take.Enemies = Enemies;
	Take.Into = &Before;
	FWatch Check = Take;
	Check.bCheck = true;
	Sim->Register(EDTSimPhase::Weapons, &Shooter);
	Sim->Register(EDTSimPhase::Director, &Take);
	Sim->Register(EDTSimPhase::Director, &First);
	Sim->Register(EDTSimPhase::Director, &Second);
	Sim->Register(EDTSimPhase::Director, &Check);
	const int32 Ran = Sim->AdvanceFrame(3.5f / Hz);
	Sim->Unregister(&Shooter);
	Sim->Unregister(&Take);
	Sim->Unregister(&First);
	Sim->Unregister(&Second);
	Sim->Unregister(&Check);

	TestTrue(TEXT("the frame ran several steps"), Ran >= 2);
	TestEqual(TEXT("the first reader read every step"), First.Seen.Num(), Ran);
	TestEqual(TEXT("so did the second"), Second.Seen.Num(), Ran);
	for (int32 Step = 0; Step < FMath::Min(First.Seen.Num(), Second.Seen.Num()); ++Step)
	{
		TestEqual(TEXT("the whole step: one record per hit applied"), First.Seen[Step].Num(), Count);
		bool bSame = First.Seen[Step].Num() == Second.Seen[Step].Num();
		for (int32 Index = 0; bSame && Index < First.Seen[Step].Num(); ++Index)
		{
			bSame = SameImpact(First.Seen[Step][Index], Second.Seen[Step][Index]);
		}
		TestTrue(TEXT("both readers see the same records in the same order"), bSame);
	}
	TestEqual(TEXT("the pool, the view and the run are as they were after both read"), Check.Differ, 0);
	return true;
}

// T-IMP-03, T-IMP-10: what a step raises, and nothing for a tick; two steps in one frame both stay; ages follow the step and the alpha;
// the same kill throws the same pieces; feeding and drawing leave the sim as it was and add no actor per effect.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactFeed, "Dusktracer.Impact.Feed", DTImpactArtTest::Flags)
bool FDTImpactFeed::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	const float Small = 30.0f;
	const float Large = 60.0f;
	TArray<FDTImpact> Step;
	for (uint32 Serial = 1; Serial <= 5; ++Serial)
	{
		Step.Add(Hit(Serial, FVector2f(100.0f * Serial + 20.0f, 0.0f), FVector2f(100.0f * Serial, 0.0f), 20.0f));
	}
	Step.Add(Hit(6, FVector2f(600.0f, 0.0f), FVector2f(600.0f, 0.0f), 20.0f, DTImpactFlags::FieldTick));
	Step.Add(Hit(7, FVector2f(700.0f, 0.0f), FVector2f(700.0f, 0.0f), 20.0f, DTImpactFlags::StatusTick));
	Step.Add(Hit(8, FVector2f(780.0f, 0.0f), FVector2f(800.0f, 0.0f), Small, DTImpactFlags::Kill));
	Step.Add(Hit(9, FVector2f(900.0f, 0.0f), FVector2f(900.0f, 0.0f), Large, DTImpactFlags::Kill | DTImpactFlags::StatusTick));
	const int32 Pieces = DTImpactArt::DebrisPieces(Small) + DTImpactArt::DebrisPieces(Large);

	FDTImpactEffects Effects;
	const FDTImpactStepCounts Counts = Effects.Feed(Input(100, Step, EDTDensityTier::PerKill));
	TestEqual(TEXT("counted hits: neither tick nor kill"), Counts.CountedHits, 5);
	TestEqual(TEXT("kills, a tick's included"), Counts.Kills, 2);
	TestEqual(TEXT("a spark per counted hit, none for a tick or a kill"), Effects.Sparks.NumLive, 5);
	TestEqual(TEXT("a flash per kill"), Effects.Flashes.NumLive, 2);
	TestEqual(TEXT("the stated pieces per kill"), Effects.Pieces.NumLive, Pieces);
	TestEqual(TEXT("pieces follow the drawn radius"), DTImpactArt::DebrisPieces(Small), FMath::RoundToInt32(Small / static_cast<float>(DTConst::IMP_DEBRIS_RADIUS_PER_PIECE_UU)));

	// Two steps in one frame: the second's effects join the first's.
	Effects.Feed(Input(101, Step, EDTDensityTier::PerKill));
	TestEqual(TEXT("both steps' sparks are live"), Effects.Sparks.NumLive, 10);
	TestEqual(TEXT("both steps' flashes are live"), Effects.Flashes.NumLive, 4);

	// Age: the step index and the alpha, nothing else.
	const double AtBirth = DTImpactArt::NowSeconds(101, 1.0f); // 101 steps run: step 100 is the last, drawn whole at alpha 1
	TestEqual(TEXT("a held step draws its own effects at age 0"), DTImpactArt::AgeSeconds(100, AtBirth), 0.0f, Tolerance);
	TestEqual(TEXT("half way into the next step, half a step older"), DTImpactArt::AgeSeconds(100, DTImpactArt::NowSeconds(102, 0.5f)), 0.5f / Hz, Tolerance);
	TestEqual(TEXT("no step, no age: the same alpha draws the same age"), DTImpactArt::AgeSeconds(100, DTImpactArt::NowSeconds(102, 0.5f)), DTImpactArt::AgeSeconds(100, DTImpactArt::NowSeconds(102, 0.5f)));
	TestEqual(TEXT("never younger than its birth"), DTImpactArt::AgeSeconds(100, DTImpactArt::NowSeconds(100, 0.0f)), 0.0f);
	const int32 SparkSteps = FMath::CeilToInt32(static_cast<float>(DTConst::IMP_SPARK_S) * Hz) + 2;
	Effects.Expire(101 + SparkSteps);
	TestEqual(TEXT("sparks end after their life"), Effects.Sparks.NumLive, 0);
	const int32 DebrisSteps = FMath::CeilToInt32(static_cast<float>(DTConst::IMP_DEBRIS_S) * Hz) + 2;
	Effects.Expire(101 + DebrisSteps);
	TestEqual(TEXT("debris ends after its life"), Effects.Pieces.NumLive, 0);
	TestEqual(TEXT("flashes too"), Effects.Flashes.NumLive, 0);

	// The same step and serial give the same pieces.
	const DTImpactArt::FDTPiece A = DTImpactArt::MakePiece(42, 7, 3, FVector2f(10.0f, 20.0f), FVector2f(1.0f, 0.0f));
	const DTImpactArt::FDTPiece B = DTImpactArt::MakePiece(42, 7, 3, FVector2f(10.0f, 20.0f), FVector2f(1.0f, 0.0f));
	TestTrue(TEXT("the same piece twice"), A.Dir == B.Dir && A.Speed == B.Speed && A.Yaw == B.Yaw && A.Spin == B.Spin && A.Shard == B.Shard);
	const DTImpactArt::FDTPiece Other = DTImpactArt::MakePiece(42, 8, 3, FVector2f(10.0f, 20.0f), FVector2f(1.0f, 0.0f));
	TestTrue(TEXT("another step, another piece"), Other.Dir != A.Dir || Other.Speed != A.Speed);

	// In a world: the subsystem reads and draws, the sim is as it was, and no actor is added per effect.
	FKeepSettings Keep;
	UDTImpactSubsystem::DrawSetting() = 1;
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	UDTImpactSubsystem* Impact = W.World->GetSubsystem<UDTImpactSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("the impact subsystem"), Impact) || !TestNotNull(TEXT("its renderer"), Impact->GetRenderer()))
	{
		return false;
	}
	const int32 Count = SpawnSoaks(Enemies->GetPool(), 6);
	FShooter Shooter;
	Shooter.Enemies = Enemies;
	Shooter.Count = Count;
	Sim->Register(EDTSimPhase::Weapons, &Shooter);
	Sim->StepNow(2);
	Impact->SimRender(1.0f);
	const int32 ActorsBefore = W.CountActors();
	const FSimSnapshot Before = FSimSnapshot::Take(Enemies->GetPool(), Sim->GetView(), Sim->GetRun());
	FDTSimContext Context;
	Context.StepSeconds = 1.0f / Hz;
	Context.StepIndex = Sim->GetStepIndex() - 1;
	Context.Run = &Sim->GetRun();
	Context.View = &Sim->GetView();
	Impact->SimStep(Context);
	Impact->SimRender(0.5f);
	TestTrue(TEXT("feeding and drawing leave the pool, the view and the run as they were"), Before == FSimSnapshot::Take(Enemies->GetPool(), Sim->GetView(), Sim->GetRun()));
	Sim->StepNow(10);
	Impact->SimRender(1.0f);
	Sim->Unregister(&Shooter);
	TestTrue(TEXT("the world's hits raised sparks"), Impact->GetEffects().Sparks.NumLive > 0);
	TestEqual(TEXT("no actor per effect"), W.CountActors(), ActorsBefore);
	TestTrue(TEXT("sparks are drawn by one component"), ADTImpactRenderer::CountShown(Impact->GetRenderer()->GetSparks()) == Impact->GetEffects().Sparks.NumLive
		|| !Impact->GetRenderer()->HasSparkMaterial());
	return true;
}

// T-IMP-04: both heights lie over the tails and cues and under every telegraph layer and round; the materials' shading and blend; the tokens.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactUnderTelegraphs, "Dusktracer.Impact.UnderTelegraphs", DTImpactArtTest::Flags)
bool FDTImpactUnderTelegraphs::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	const float Layer = static_cast<float>(DTConst::PRE_TELEGRAPH_LAYER_UU);
	const float RoundBottom = static_cast<float>(DTConst::SIM_PLANE_Z_UU) - static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU * DTConst::ENEMY_FIRE_DRAW_RADIUS_RATIO);
	for (const float Height : { static_cast<float>(DTConst::IMP_DEBRIS_Z_UU), static_cast<float>(DTConst::IMP_FLASH_Z_UU) })
	{
		TestTrue(TEXT("over the player tails"), Height > static_cast<float>(DTConst::TRC_TAIL_Z_UU));
		TestTrue(TEXT("over the control cues"), Height > static_cast<float>(DTConst::PRE_CUE_Z_UU));
		TestTrue(TEXT("over the zones"), Height > static_cast<float>(DTConst::PRE_ZONE_Z_UU + DTConst::PRE_ZONE_HALF_UU));
		// The lowest thing a telegraph draws is the Danger outlines band, one layer under the dark band (settings.md R-SET-10).
		TestTrue(TEXT("under the lowest telegraph layer"), Height < static_cast<float>(DTConst::ENEMIES_TELEGRAPH_Z_UU) - Layer);
		TestTrue(TEXT("under the lowest point of a drawn enemy round"), Height < RoundBottom);
	}
	TestTrue(TEXT("debris lies under the flashes"), DTConst::IMP_DEBRIS_Z_UU < DTConst::IMP_FLASH_Z_UU);

	const TCHAR* Additive[] = { TEXT("/Game/DT/Materials/M_Fx_Spark.M_Fx_Spark"), TEXT("/Game/DT/Materials/M_Fx_Flash.M_Fx_Flash"), TEXT("/Game/DT/Materials/M_Fx_Ring.M_Fx_Ring") };
	for (const TCHAR* Path : Additive)
	{
		const UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
		if (!TestNotNull(Path, Material))
		{
			return false;
		}
		TestTrue(TEXT("additive"), Material->GetBlendMode() == BLEND_Additive);
		TestTrue(TEXT("unlit: the sun does not move it off its token"), Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit));
		float Now = -1.0f;
		TestTrue(TEXT("its age comes from the parameter Now"), Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Now")), Now));
	}
	const UMaterialInterface* Debris = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DT/Materials/M_Fx_Debris.M_Fx_Debris"));
	if (!TestNotNull(TEXT("M_Fx_Debris"), Debris))
	{
		return false;
	}
	TestTrue(TEXT("debris is opaque"), Debris->GetBlendMode() == BLEND_Opaque);
	TestTrue(TEXT("debris is lit, as the hull it came from"), Debris->GetShadingModels().HasShadingModel(MSM_DefaultLit));

	// As drawn: the tokens by name.
	FTestWorld W;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	ADTImpactRenderer* Renderer = W.World->SpawnActor<ADTImpactRenderer>(ADTImpactRenderer::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("renderer"), Renderer))
	{
		return false;
	}
	Renderer->InitRender();
	TestTrue(TEXT("every effect has its material and meshes"), Renderer->HasSparkMaterial() && Renderer->HasFlashMaterial() && Renderer->HasRingMaterial() && Renderer->HasDebris());
	TestEqual(TEXT("the spark's streak width at the root is the constant's, set on the material (R-IMP-05)"), Renderer->GetSparkStreakRatio(), static_cast<float>(DTConst::IMP_SPARK_STREAK_RATIO), Tolerance);
	TArray<FDTImpact> Step = { Hit(1, FVector2f(120.0f, 0.0f), FVector2f(100.0f, 0.0f), 20.0f), Hit(2, FVector2f(380.0f, 0.0f), FVector2f(400.0f, 0.0f), 40.0f, DTImpactFlags::Kill) };
	const bool Hurt[] = { true };
	const FVector2f Heroes[] = { FVector2f(-300.0f, 0.0f) };
	FDTImpactEffects Effects;
	Effects.Feed(Input(10, Step, EDTDensityTier::PerKill, Hurt, Heroes));
	Renderer->Draw(Effects, DTImpactArt::NowSeconds(11, 1.0f), Heroes);
	const auto Carries = [](const UInstancedStaticMeshComponent* Part, const FColor& Token, int32 Live)
	{
		const FLinearColor Want = DTPal::Linear(Token);
		const int32 Stride = Part->NumCustomDataFloats;
		int32 Found = 0;
		for (int32 Index = 0; Index < Part->GetInstanceCount(); ++Index)
		{
			const float* Row = Part->PerInstanceSMCustomData.GetData() + Index * Stride;
			Found += Row[0] == Want.R && Row[1] == Want.G && Row[2] == Want.B ? 1 : 0;
		}
		return Found >= Live && Live > 0;
	};
	TestTrue(TEXT("sparks carry Pal.Player.Core"), Carries(Renderer->GetSparks(), DTPal::Player_Core, Effects.Sparks.NumLive));
	TestTrue(TEXT("kill flashes carry Pal.Player.Core"), Carries(Renderer->GetFlashes(), DTPal::Player_Core, Effects.Flashes.NumLive));
	TestTrue(TEXT("the hero ring carries Pal.Enemy.Muzzle"), Carries(Renderer->GetRings(), DTPal::Enemy_Muzzle, 1));
	int32 Pieces = 0;
	for (int32 Shard = 0; Shard < DTImpactArt::NumShards; ++Shard)
	{
		const UInstancedStaticMeshComponent* Part = Renderer->GetDebris(Shard);
		TestEqual(TEXT("debris carries no emissive: base colour only"), Part->NumCustomDataFloats, DTImpactArt::DebrisData);
		Pieces += ADTImpactRenderer::CountShown(Part);
		if (Part->GetInstanceCount() > 0)
		{
			TestTrue(TEXT("debris carries Pal.Enemy.Body"), Carries(Part, DTPal::Enemy_Body, 1));
		}
	}
	TestEqual(TEXT("every live piece is drawn"), Pieces, Effects.Pieces.NumLive);
	return true;
}

// T-IMP-05, T-IMP-06, T-IMP-07: the shapes over their lives.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactShapes, "Dusktracer.Impact.Shapes", DTImpactArtTest::Flags)
bool FDTImpactShapes::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	// R-IMP-05: away from the hull along the line from its centre through the contact point; away from the owner's hero when the two coincide.
	const FVector2f Dir = DTImpactArt::SparkDirection(FVector2f(110.0f, 10.0f), FVector2f(100.0f, 0.0f), nullptr);
	TestTrue(TEXT("from the centre through the contact point"), Dir.Equals(FVector2f(1.0f, 1.0f).GetSafeNormal(), Tolerance));
	const FVector2f Hero(0.0f, -100.0f);
	TestTrue(TEXT("away from the owner's hero when contact and centre coincide"), DTImpactArt::SparkDirection(FVector2f(0.0f, 0.0f), FVector2f(0.0f, 0.0f), &Hero).Equals(FVector2f(0.0f, 1.0f), Tolerance));
	const float SparkLife = static_cast<float>(DTConst::IMP_SPARK_S);
	const DTImpactArt::FDTSparkShape Born = DTImpactArt::SparkAt(0.0f);
	const DTImpactArt::FDTSparkShape Half = DTImpactArt::SparkAt(0.5f * SparkLife);
	TestEqual(TEXT("born at its start ratio"), Born.Length, static_cast<float>(DTConst::IMP_SPARK_LENGTH_UU * DTConst::IMP_SPARK_START_RATIO), Tolerance);
	TestEqual(TEXT("born at its full strength"), Born.Emissive, static_cast<float>(DTConst::IMP_SPARK_EMISSIVE), Tolerance);
	TestTrue(TEXT("grows and fades"), Half.Length > Born.Length && Half.Width > Born.Width && Half.Emissive < Born.Emissive);
	TestEqual(TEXT("full size at the end"), DTImpactArt::SparkAt(SparkLife * 0.999f).Width, static_cast<float>(DTConst::IMP_SPARK_WIDTH_UU), 0.1f);
	for (const float Age : { 0.0f, 0.5f * SparkLife, 0.99f * SparkLife })
	{
		const DTImpactArt::FDTSparkShape At = DTImpactArt::SparkAt(Age);
		TestEqual(TEXT("each streak's root is C.IMP.SPARK_STREAK_RATIO of the spark's width"), At.StreakRoot, static_cast<float>(DTConst::IMP_SPARK_STREAK_RATIO) * At.Width, Tolerance);
	}
	TestFalse(TEXT("gone after its life"), DTImpactArt::SparkAt(SparkLife).bAlive);

	// R-IMP-06: the flash's two ratios, an elite's drawn radius and the widest radius.
	const float FlashLife = static_cast<float>(DTConst::IMP_KILL_FLASH_S);
	const float Drawn = DTImpactArt::DrawnRadius(40.0f, false);
	TestEqual(TEXT("born at its start ratio"), DTImpactArt::FlashAt(Drawn, 0.0f, true).Radius, Drawn * static_cast<float>(DTConst::IMP_KILL_FLASH_START_RATIO), Tolerance);
	TestEqual(TEXT("ends at its end ratio"), DTImpactArt::FlashAt(Drawn, FlashLife * 0.9999f, true).Radius, Drawn * static_cast<float>(DTConst::IMP_KILL_FLASH_END_RATIO), 0.1f);
	TestTrue(TEXT("fades"), DTImpactArt::FlashAt(Drawn, 0.5f * FlashLife, true).Emissive < DTImpactArt::FlashAt(Drawn, 0.0f, true).Emissive);
	TestFalse(TEXT("gone after its life"), DTImpactArt::FlashAt(Drawn, FlashLife, true).bAlive);
	TestEqual(TEXT("an elite's drawn radius"), DTImpactArt::DrawnRadius(40.0f, true), 40.0f * static_cast<float>(DTConst::ELITE_DRAW_SCALE), Tolerance);
	const float Warden = DTImpactArt::DrawnRadius(static_cast<float>(DTConst::BOSS_RADIUS_UU), false);
	for (float Age = 0.0f; Age < FlashLife; Age += 0.25f / Hz)
	{
		TestTrue(TEXT("never wider than C.IMP.FLASH_RADIUS_MAX_UU, the Warden's included"), DTImpactArt::FlashAt(Warden, Age, true).Radius <= static_cast<float>(DTConst::IMP_FLASH_RADIUS_MAX_UU) + Tolerance);
	}

	// R-IMP-07: pieces, their path, shrink and end.
	TestEqual(TEXT("never fewer than the least"), DTImpactArt::DebrisPieces(1.0f), static_cast<int32>(DTConst::IMP_DEBRIS_PIECES_MIN));
	TestEqual(TEXT("never more than the most"), DTImpactArt::DebrisPieces(Warden), static_cast<int32>(DTConst::IMP_DEBRIS_PIECES_MAX));
	int32 Last = 0;
	for (float Radius = 1.0f; Radius < 200.0f; Radius += 3.0f)
	{
		const int32 Pieces = DTImpactArt::DebrisPieces(Radius);
		TestTrue(TEXT("more drawn radius never throws fewer pieces"), Pieces >= Last);
		Last = Pieces;
	}
	const FVector2f Contact(90.0f, 0.0f);
	const FVector2f Centre(100.0f, 0.0f);
	const FVector2f Blow = DTImpactArt::BlowDirection(Contact, Centre, 5, 9);
	TestTrue(TEXT("the blow runs from the contact point through the centre"), Blow.Equals(FVector2f(1.0f, 0.0f), Tolerance));
	const float Spread = FMath::DegreesToRadians(static_cast<float>(DTConst::IMP_DEBRIS_SPREAD_DEG));
	const float DebrisLife = static_cast<float>(DTConst::IMP_DEBRIS_S);
	const float Drag = static_cast<float>(DTConst::IMP_DEBRIS_DRAG_S);
	const float Share = static_cast<float>(DTConst::IMP_DEBRIS_SHRINK_SHARE);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const DTImpactArt::FDTPiece Piece = DTImpactArt::MakePiece(5, 9, Index, Centre, Blow);
		TestTrue(TEXT("leaves along the blow, inside the spread"), FVector2f::DotProduct(Piece.Dir, Blow) >= FMath::Cos(Spread) - Tolerance);
		const float Fastest = static_cast<float>(DTConst::IMP_DEBRIS_SPEED_UU_S) * (1.0f + static_cast<float>(DTConst::IMP_DEBRIS_SPEED_VARIATION));
		TestTrue(TEXT("its speed inside the variation"), Piece.Speed <= Fastest + Tolerance
			&& Piece.Speed >= static_cast<float>(DTConst::IMP_DEBRIS_SPEED_UU_S) * (1.0f - static_cast<float>(DTConst::IMP_DEBRIS_SPEED_VARIATION)) - Tolerance);
		const DTImpactArt::FDTPieceAt End = DTImpactArt::PieceAt(Piece, DebrisLife * 0.999f);
		TestTrue(TEXT("never further than its speed times the drag constant"), FVector2f::Distance(End.Position, Centre) <= Piece.Speed * Drag + Tolerance);
		TestEqual(TEXT("full size before the shrink"), DTImpactArt::PieceAt(Piece, DebrisLife * (1.0f - Share) * 0.99f).Scale, 1.0f);
		TestTrue(TEXT("shrinks over the stated share"), DTImpactArt::PieceAt(Piece, DebrisLife * (1.0f - 0.5f * Share)).Scale < 1.0f);
		TestTrue(TEXT("turns as it goes"), !FMath::IsNearlyEqual(End.Yaw, Piece.Yaw));
		TestFalse(TEXT("gone after its life"), DTImpactArt::PieceAt(Piece, DebrisLife).bAlive);
	}
	return true;
}

// T-IMP-08: the tier rule, what a Crowd and a Flood step raise, the live caps under a long dense run, and the enemy side drawn the same.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactDensity, "Dusktracer.Impact.Density", DTImpactArtTest::Flags)
bool FDTImpactDensity::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	using DTImpactArt::NextTier;
	const float On = static_cast<float>(DTConst::PRE_DENSITY_LOD_KILLS_PER_S);
	const float CrowdOff = static_cast<float>(DTConst::IMP_DENSITY_CROWD_OFF_KILLS_PER_S);
	const float FloodOn = static_cast<float>(DTConst::IMP_DENSITY_FLOOD_KILLS_PER_S);
	const float FloodOff = static_cast<float>(DTConst::IMP_DENSITY_FLOOD_OFF_KILLS_PER_S);
	TestTrue(TEXT("each tier is left under where it is entered"), CrowdOff < On && FloodOff < FloodOn && On < FloodOff);
	TestTrue(TEXT("above the threshold enters Crowd"), NextTier(EDTDensityTier::PerKill, On + 0.5f) == EDTDensityTier::Crowd);
	TestTrue(TEXT("at it does not"), NextTier(EDTDensityTier::PerKill, On) == EDTDensityTier::PerKill);
	TestTrue(TEXT("falling back, Crowd holds above its exit"), NextTier(EDTDensityTier::Crowd, CrowdOff + 0.5f) == EDTDensityTier::Crowd);
	TestTrue(TEXT("and leaves under it"), NextTier(EDTDensityTier::Crowd, CrowdOff - 0.5f) == EDTDensityTier::PerKill);
	TestTrue(TEXT("above the flood threshold enters Flood"), NextTier(EDTDensityTier::Crowd, FloodOn + 0.5f) == EDTDensityTier::Flood);
	TestTrue(TEXT("at it does not"), NextTier(EDTDensityTier::Crowd, FloodOn) == EDTDensityTier::Crowd);
	TestTrue(TEXT("Flood holds above its exit"), NextTier(EDTDensityTier::Flood, FloodOff + 0.5f) == EDTDensityTier::Flood);
	TestTrue(TEXT("and leaves under it"), NextTier(EDTDensityTier::Flood, FloodOff - 0.5f) == EDTDensityTier::Crowd);
	TestTrue(TEXT("two tiers up in one step"), NextTier(EDTDensityTier::PerKill, FloodOn + 1.0f) == EDTDensityTier::Flood);
	TestTrue(TEXT("two tiers down in one step"), NextTier(EDTDensityTier::Flood, CrowdOff - 1.0f) == EDTDensityTier::PerKill);

	// A Crowd step: kills in three cells of the grid, one cell holding three.
	const float Cell = static_cast<float>(DTConst::IMP_AGG_CELL_UU);
	TArray<FDTImpact> Step;
	const float Radii[] = { 20.0f, 30.0f, 40.0f };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Step.Add(Hit(100 + Index, FVector2f(0.1f * Cell + 10.0f * Index, 0.1f * Cell), FVector2f(0.1f * Cell + 10.0f * Index + 5.0f, 0.1f * Cell), Radii[Index], DTImpactFlags::Kill));
	}
	Step.Add(Hit(110, FVector2f(2.5f * Cell, 0.5f * Cell), FVector2f(2.5f * Cell, 0.5f * Cell), 20.0f, DTImpactFlags::Kill));
	Step.Add(Hit(111, FVector2f(-3.5f * Cell, 0.5f * Cell), FVector2f(-3.5f * Cell, 0.5f * Cell), 20.0f, DTImpactFlags::Kill));
	for (int32 Index = 0; Index < 40; ++Index)
	{
		const uint32 Serial = 200 + Index % 20; // twenty enemies, two hits each
		Step.Add(Hit(Serial, FVector2f(10.0f * Serial, 900.0f), FVector2f(10.0f * Serial, 910.0f), 20.0f));
	}
	{
		FDTImpactEffects Effects;
		const FDTImpactStepCounts Counts = Effects.Feed(Input(50, Step, EDTDensityTier::Crowd));
		TestEqual(TEXT("Crowd: no debris"), Effects.Pieces.NumLive, 0);
		TestEqual(TEXT("one flash per occupied cell"), Counts.Flashes, FMath::Min(3, static_cast<int32>(DTConst::IMP_AGG_PER_STEP_MAX)));
		TestEqual(TEXT("sparks: at most one per enemy and the tier's cap"), Counts.Sparks, FMath::Min(20, static_cast<int32>(DTConst::IMP_SPARKS_DENSE_PER_STEP_MAX)));
		TArray<DTImpactArt::FDTAggregate> Cells;
		TArray<FVector2f> At;
		TArray<float> Drawn;
		for (const FDTImpact& Impact : Step)
		{
			if (DTImpactArt::IsKill(Impact))
			{
				At.Add(Impact.EnemyPosition);
				Drawn.Add(Impact.EnemyRadius);
			}
		}
		DTImpactArt::Aggregate(At, Drawn, Cell, DTConst::IMP_AGG_PER_STEP_MAX, Cells);
		if (TestTrue(TEXT("the fullest cell first"), Cells.Num() > 0 && Cells[0].Count == 3))
		{
			TestEqual(TEXT("its radius is the root of the summed squares"), Cells[0].Drawn, FMath::Sqrt(20.0f * 20.0f + 30.0f * 30.0f + 40.0f * 40.0f), Tolerance);
			TestEqual(TEXT("at the centre of its kills"), Cells[0].Centre.X, 0.1f * Cell + 15.0f, Tolerance);
		}
		DTImpactArt::Aggregate(At, Drawn, Cell, 1, Cells);
		TestTrue(TEXT("over the cap, the fullest are raised"), Cells.Num() == 1 && Cells[0].Count == 3);
		for (int32 Slot = 0; Slot < Effects.Flashes.NumSlots(); ++Slot)
		{
			if (Effects.Flashes.IsLive(Slot))
			{
				const float Widest = DTImpactArt::FlashAt(Effects.Flashes.Items[Slot].Drawn, 0.999f * static_cast<float>(DTConst::IMP_KILL_FLASH_S), true).Radius;
				TestTrue(TEXT("inside the widest radius"), Widest <= static_cast<float>(DTConst::IMP_FLASH_RADIUS_MAX_UU) + Tolerance);
			}
		}
	}
	{
		FDTImpactEffects Effects;
		const FDTImpactStepCounts Counts = Effects.Feed(Input(50, Step, EDTDensityTier::Flood));
		TestEqual(TEXT("Flood: no spark"), Counts.Sparks, 0);
		TestEqual(TEXT("no debris"), Counts.Pieces, 0);
		TestTrue(TEXT("at most its own cap of flashes"), Counts.Flashes <= DTConst::IMP_AGG_FLOOD_PER_STEP_MAX && Counts.Flashes > 0);
		TArray<DTImpactArt::FDTAggregate> Cells;
		TArray<FVector2f> At = { Step[0].EnemyPosition, Step[3].EnemyPosition, Step[4].EnemyPosition };
		TArray<float> Drawn = { 20.0f, 20.0f, 20.0f };
		DTImpactArt::Aggregate(At, Drawn, static_cast<float>(DTConst::IMP_AGG_FLOOD_CELL_UU), DTConst::IMP_AGG_FLOOD_PER_STEP_MAX, Cells);
		TestEqual(TEXT("on its own grid"), Counts.Flashes, Cells.Num());
	}

	// A long run far over the highest threshold, in every tier the rule gives, never passes a live cap.
	{
		FDTImpactEffects Effects;
		EDTDensityTier Tier = EDTDensityTier::PerKill;
		TArray<FDTImpact> Dense;
		for (int32 Step2 = 0; Step2 < 600; ++Step2)
		{
			Dense.Reset();
			for (int32 Index = 0; Index < 80; ++Index)
			{
				const uint32 Serial = static_cast<uint32>(Step2 * 100 + Index + 1);
				const FVector2f At(37.0f * Index - 1500.0f, 23.0f * (Index % 9) - 100.0f);
				Dense.Add(Hit(Serial, At + FVector2f(5.0f, 0.0f), At, 60.0f, Index % 2 == 0 ? DTImpactFlags::Kill : 0));
			}
			// The first steps in PerKill, then whatever the rule makes of a rate ten times the flood threshold.
			Effects.Feed(Input(Step2, Dense, Tier));
			Tier = Step2 < 30 ? EDTDensityTier::PerKill : NextTier(Tier, 10.0f * FloodOn);
			if (Effects.Sparks.NumLive > DTConst::IMP_SPARK_LIVE_MAX || Effects.Flashes.NumLive > DTConst::IMP_FLASH_LIVE_MAX || Effects.Pieces.NumLive > DTConst::IMP_DEBRIS_LIVE_MAX)
			{
				AddError(FString::Printf(TEXT("a live cap passed on step %d"), Step2));
				break;
			}
		}
		TestTrue(TEXT("caps refused what would pass them"), Effects.RefusedPieces > 0 || Effects.RefusedFlashes > 0 || Effects.RefusedSparks > 0);
	}

	// Enemy hull, optic, round, trail and telegraph instance data are the same drawn with this package and without it, in each tier.
	FKeepSettings Keep;
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	UDTImpactSubsystem* Impact = W.World->GetSubsystem<UDTImpactSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("impact"), Impact))
	{
		return false;
	}
	FDTEnemyPool& Pool = Enemies->GetPool();
	SpawnSoaks(Pool, 4);
	Pool.Spawn(EDTEnemyRole::Turret, FVector2f(600.0f, 600.0f));
	FShooter Shooter;
	Shooter.Enemies = Enemies;
	Shooter.Count = Pool.Num();
	Sim->Register(EDTSimPhase::Weapons, &Shooter);
	const float Rates[] = { 0.0f, On + 2.0f, FloodOn + 5.0f };
	const EDTDensityTier Tiers[] = { EDTDensityTier::PerKill, EDTDensityTier::Crowd, EDTDensityTier::Flood };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UDTImpactSubsystem::DrawSetting() = 1;
		Impact->SetRateOverride(Rates[Index]);
		Sim->StepNow(3);
		TestTrue(TEXT("the tier staged"), Impact->GetDensityTier() == Tiers[Index]);
		const auto EnemySide = [&]()
		{
			Enemies->SimRender(0.5f);
			Impact->SimRender(0.5f);
			TArray<float> All;
			if (ADTEnemyRenderer* Hulls = Enemies->GetRenderer())
			{
				for (UInstancedStaticMeshComponent* Part : TInlineComponentArray<UInstancedStaticMeshComponent*>(Hulls))
				{
					All.Append(Contents(Part));
				}
			}
			if (ADTEnemyBulletRenderer* Rounds = Enemies->GetRoundRenderer())
			{
				All.Append(Contents(Rounds->GetInstances()));
				All.Append(Contents(Rounds->GetTrails()));
				All.Append(Contents(Rounds->GetFlashes()));
			}
			if (ADTTelegraphRenderer* Telegraphs = Enemies->GetTelegraphRenderer())
			{
				for (UInstancedStaticMeshComponent* Part : TInlineComponentArray<UInstancedStaticMeshComponent*>(Telegraphs))
				{
					All.Append(Contents(Part));
				}
			}
			return All;
		};
		const TArray<float> Drawing = EnemySide();
		UDTImpactSubsystem::DrawSetting() = 0;
		Impact->SimRender(0.5f);
		const TArray<float> Off = EnemySide();
		TestTrue(FString::Printf(TEXT("the enemy side is drawn the same with and without the effects, tier %s"), DTImpactArt::TierName(Tiers[Index])), Drawing == Off);
	}
	Sim->Unregister(&Shooter);
	Impact->SetRateOverride(-1.0f);
	return true;
}

// T-PRE-06, the kill half: above C.PRE.DENSITY_LOD_KILLS_PER_S per-kill effects aggregate and their live count stays bounded.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPreDensityLodKill, "Dusktracer.Pre.DensityLodKill", DTImpactArtTest::Flags)
bool FDTPreDensityLodKill::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	const float On = static_cast<float>(DTConst::PRE_DENSITY_LOD_KILLS_PER_S);
	TestTrue(TEXT("above the threshold kills aggregate"), DTImpactArt::NextTier(EDTDensityTier::PerKill, On + 1.0f) != EDTDensityTier::PerKill);
	TestTrue(TEXT("at it they do not"), DTImpactArt::NextTier(EDTDensityTier::PerKill, On) == EDTDensityTier::PerKill);
	// Twenty kills in one cell: one flash, no debris, above the threshold; twenty flashes and their debris under it.
	TArray<FDTImpact> Step;
	for (uint32 Serial = 1; Serial <= 20; ++Serial)
	{
		Step.Add(Hit(Serial, FVector2f(5.0f * Serial, 10.0f), FVector2f(5.0f * Serial, 12.0f), 20.0f, DTImpactFlags::Kill));
	}
	FDTImpactEffects Dense;
	Dense.Feed(Input(1, Step, EDTDensityTier::Crowd));
	TestEqual(TEXT("aggregated: one flash for the cell"), Dense.Flashes.NumLive, 1);
	TestEqual(TEXT("no debris"), Dense.Pieces.NumLive, 0);
	FDTImpactEffects Sparse;
	Sparse.Feed(Input(1, Step, EDTDensityTier::PerKill));
	TestEqual(TEXT("under it, every kill its flash"), Sparse.Flashes.NumLive, 20);
	TestTrue(TEXT("and its debris"), Sparse.Pieces.NumLive > 0);
	TestTrue(TEXT("the live counts are bounded whatever the rate"), DTConst::IMP_FLASH_LIVE_MAX > 0 && DTConst::IMP_DEBRIS_LIVE_MAX > 0 && DTConst::IMP_SPARK_LIVE_MAX > 0);
	return true;
}

// T-IMP-09: one ring per hurt hero, following it, restarted by a second hit, ended after its life; with Screen flashes off the ring and
// the kill flashes are steady and reduced, and sparks and debris do not change.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactHeroRing, "Dusktracer.Impact.HeroRing", DTImpactArtTest::Flags)
bool FDTImpactHeroRing::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	const bool Hurt[] = { true };
	const bool Fine[] = { false };
	FVector2f Heroes[] = { FVector2f(100.0f, 50.0f) };
	FDTImpactEffects Effects;
	Effects.Feed(Input(20, {}, EDTDensityTier::PerKill, Hurt, Heroes));
	TestEqual(TEXT("one ring for the hurt hero"), Effects.NumRingsLive(), 1);
	Effects.Feed(Input(22, {}, EDTDensityTier::PerKill, Hurt, Heroes));
	TestEqual(TEXT("a second hit restarts it, still one"), Effects.NumRingsLive(), 1);
	TestEqual(TEXT("from the second hit's step"), Effects.Rings[0].Born, static_cast<int64>(22));
	const int32 Life = FMath::CeilToInt32(static_cast<float>(DTConst::IMP_HERO_FLASH_S) * Hz) + 2;
	Effects.Feed(Input(22 + Life, {}, EDTDensityTier::PerKill, Fine, Heroes));
	TestEqual(TEXT("it ends after its life"), Effects.NumRingsLive(), 0);

	// Drawn: it follows its hero.
	FTestWorld W;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	ADTImpactRenderer* Renderer = W.World->SpawnActor<ADTImpactRenderer>(ADTImpactRenderer::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("renderer"), Renderer))
	{
		return false;
	}
	Renderer->InitRender();
	FDTImpactEffects Drawn;
	Drawn.Feed(Input(30, {}, EDTDensityTier::PerKill, Hurt, Heroes));
	Renderer->Draw(Drawn, DTImpactArt::NowSeconds(31, 0.0f), Heroes);
	if (Renderer->HasRingMaterial() && TestEqual(TEXT("one ring instance"), Renderer->GetRings()->GetInstanceCount(), 1))
	{
		FTransform At;
		Renderer->GetRings()->GetInstanceTransform(0, At);
		TestTrue(TEXT("centred on the hero"), FVector2f(static_cast<float>(At.GetLocation().X), static_cast<float>(At.GetLocation().Y)).Equals(Heroes[0], Tolerance));
		Heroes[0] = FVector2f(300.0f, -40.0f);
		Renderer->Draw(Drawn, DTImpactArt::NowSeconds(31, 0.5f), Heroes);
		Renderer->GetRings()->GetInstanceTransform(0, At);
		TestTrue(TEXT("and follows it"), FVector2f(static_cast<float>(At.GetLocation().X), static_cast<float>(At.GetLocation().Y)).Equals(Heroes[0], Tolerance));
	}

	// Screen flashes off (R-SET-09): steady, at the reduced ratio, and at the final size for the whole life.
	const float RingLife = static_cast<float>(DTConst::IMP_HERO_FLASH_S);
	const float Reduced = static_cast<float>(DTConst::IMP_HERO_FLASH_EMISSIVE * DTConst::IMP_REDUCED_EMISSIVE_RATIO);
	for (const float Age : { 0.0f, 0.5f * RingLife, 0.95f * RingLife })
	{
		const DTImpactArt::FDTRingShape Ring = DTImpactArt::RingAt(Age, false);
		TestEqual(TEXT("the ring at the reduced ratio"), Ring.Emissive, Reduced, Tolerance);
		TestEqual(TEXT("the ring does not grow"), Ring.Outer, static_cast<float>(DTConst::HERO_RADIUS_UU * DTConst::IMP_HERO_FLASH_END_RATIO), Tolerance);
		const float FlashAge = Age * static_cast<float>(DTConst::IMP_KILL_FLASH_S) / RingLife;
		const DTImpactArt::FDTFlashShape Flash = DTImpactArt::FlashAt(40.0f, FlashAge, false);
		TestEqual(TEXT("the kill flash at the reduced ratio"), Flash.Emissive, static_cast<float>(DTConst::IMP_KILL_FLASH_EMISSIVE * DTConst::IMP_REDUCED_EMISSIVE_RATIO), Tolerance);
		TestEqual(TEXT("the kill flash does not grow"), Flash.Radius, 40.0f * static_cast<float>(DTConst::IMP_KILL_FLASH_END_RATIO), Tolerance);
	}
	TestTrue(TEXT("with flashes on the ring grows"), DTImpactArt::RingAt(0.0f, true).Outer < DTImpactArt::RingAt(0.9f * RingLife, true).Outer);
	// Sparks and debris are the same either way.
	TArray<FDTImpact> Step = { Hit(1, FVector2f(120.0f, 0.0f), FVector2f(100.0f, 0.0f), 20.0f), Hit(2, FVector2f(380.0f, 0.0f), FVector2f(400.0f, 0.0f), 40.0f, DTImpactFlags::Kill) };
	FDTImpactEffects On;
	FDTImpactEffects Off;
	On.Feed(Input(5, Step, EDTDensityTier::PerKill, {}, {}, true));
	Off.Feed(Input(5, Step, EDTDensityTier::PerKill, {}, {}, false));
	TestEqual(TEXT("sparks unchanged"), Off.Sparks.NumLive, On.Sparks.NumLive);
	TestEqual(TEXT("debris unchanged"), Off.Pieces.NumLive, On.Pieces.NumLive);
	bool bSamePieces = true;
	for (int32 Slot = 0; Slot < On.Pieces.NumSlots(); ++Slot)
	{
		bSamePieces &= On.Pieces.Items[Slot].Piece.Dir == Off.Pieces.Items[Slot].Piece.Dir && On.Pieces.Items[Slot].Piece.Speed == Off.Pieces.Items[Slot].Piece.Speed;
	}
	TestTrue(TEXT("the same pieces"), bSamePieces);

	// The Screen flashes row's focus hint says what Off now does (settings.md R-SET-09); drawn on one line, it must fit the settings panel
	// at the largest text size in both languages: the hint's scale times the text size times its width at scale 1, against the panel.
	FString Csv;
	const FString CsvPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DT/Text/ST_UI.csv"));
	if (TestTrue(TEXT("ST_UI.csv is readable"), FFileHelper::LoadFileToString(Csv, *CsvPath)) && TestNotNull(TEXT("the menu font"), GEngine->GetLargeFont()))
	{
		const FCsvParser Parser(Csv);
		bool bFound = false;
		for (const TArray<const TCHAR*>& Row : Parser.GetRows())
		{
			if (Row.Num() < 3 || FCString::Strcmp(Row[0], TEXT("Settings.Hint.Flash")) != 0)
			{
				continue;
			}
			bFound = true;
			const float Largest = static_cast<float>(DTConst::SETTINGS_TEXT_MAX_PCT) / static_cast<float>(DTConst::SETTINGS_TEXT_MIN_PCT);
			for (int32 Column = 1; Column <= 2; ++Column)
			{
				const float Width = static_cast<float>(GEngine->GetLargeFont()->GetStringSize(Row[Column])) * DTImpactArtTest::MenuHintScale * Largest;
				TestTrue(FString::Printf(TEXT("the flash hint fits the panel at the largest text (%s: %.0f of %.0f)"), Column == 1 ? TEXT("en") : TEXT("zh-Hans"), Width, DTImpactArtTest::MenuPanelWidth),
					Width <= DTImpactArtTest::MenuPanelWidth);
			}
		}
		TestTrue(TEXT("the row Settings.Hint.Flash is there"), bFound);
	}
	return true;
}

// T-IMP-11: the shared reader follows NextTier folded over a staged trace, step for step, drawing or not; PerKill again after a run starts;
// asking changes nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactDensityReader, "Dusktracer.Impact.DensityReader", DTImpactArtTest::Flags)
bool FDTImpactDensityReader::RunTest(const FString& Parameters)
{
	using namespace DTImpactArtTest;
	FKeepSettings Keep;
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	UDTImpactSubsystem* Impact = W.World->GetSubsystem<UDTImpactSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("impact"), Impact))
	{
		return false;
	}
	const float On = static_cast<float>(DTConst::PRE_DENSITY_LOD_KILLS_PER_S);
	const float FloodOn = static_cast<float>(DTConst::IMP_DENSITY_FLOOD_KILLS_PER_S);
	const float Trace[] = { 0.0f, On, On + 1.0f, On - 1.0f, static_cast<float>(DTConst::IMP_DENSITY_CROWD_OFF_KILLS_PER_S) - 1.0f, FloodOn + 10.0f, FloodOn - 1.0f,
		static_cast<float>(DTConst::IMP_DENSITY_FLOOD_OFF_KILLS_PER_S) - 1.0f, 0.0f, 3.0f * FloodOn, 1.0f };
	for (const int32 Draw : { 1, 0 })
	{
		UDTImpactSubsystem::DrawSetting() = Draw;
		Sim->StartRun(0, 1);
		EDTDensityTier Folded = EDTDensityTier::PerKill;
		for (const float Rate : Trace)
		{
			Impact->SetRateOverride(Rate);
			Sim->StepNow(1);
			Folded = DTImpactArt::NextTier(Folded, Rate);
			TestTrue(FString::Printf(TEXT("the tier is NextTier folded over the trace (draw %d, rate %.1f)"), Draw, Rate), Impact->GetDensityTier() == Folded);
			TestEqual(TEXT("judged on that rate"), Impact->GetKillsPerSecond(), Rate);
			TestEqual(TEXT("on that step"), Impact->GetDensityStep(), Sim->GetStepIndex() - 1);
		}
	}
	// A run started in Flood reads PerKill after its first step: a rate in Crowd's band (between the two exits) keeps Crowd from Flood but
	// does not enter it from PerKill, so only a reset tier reads PerKill here.
	UDTImpactSubsystem::DrawSetting() = 1;
	Impact->SetRateOverride(3.0f * FloodOn);
	Sim->StepNow(1);
	TestTrue(TEXT("Flood before the new run"), Impact->GetDensityTier() == EDTDensityTier::Flood);
	Sim->StartRun(0, 1);
	const float Between = 0.5f * (static_cast<float>(DTConst::IMP_DENSITY_CROWD_OFF_KILLS_PER_S) + On);
	Impact->SetRateOverride(Between);
	Sim->StepNow(1);
	TestTrue(TEXT("PerKill again after a run starts"), Impact->GetDensityTier() == EDTDensityTier::PerKill);
	TestTrue(TEXT("(from Flood the same rate would read Crowd)"), DTImpactArt::NextTier(EDTDensityTier::Flood, Between) == EDTDensityTier::Crowd);

	const FSimSnapshot Before = FSimSnapshot::Take(Enemies->GetPool(), Sim->GetView(), Sim->GetRun());
	for (int32 Ask = 0; Ask < 10; ++Ask)
	{
		(void)Impact->GetDensityTier();
		(void)Impact->GetKillsPerSecond();
		(void)Impact->GetDensityStep();
	}
	TestTrue(TEXT("asking changes nothing in the pool, the view or the run"), Before == FSimSnapshot::Take(Enemies->GetPool(), Sim->GetView(), Sim->GetRun()));
	Impact->SetRateOverride(-1.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
