// Telegraph render and threat-action telemetry tests (02_Systems/enemies.md T-ENM-25, T-ENM-26).

#include "Misc/AutomationTest.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "Telemetry/DTTelemetryCore.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTelegraphTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;

	/** A headless game world with the sim and enemy subsystems up and begun. */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTTelegraphTestWorld")));
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

	/** Instances of Part carrying exactly Color and Emissive in custom data 0-3. */
	static int32 CountCarrying(const UInstancedStaticMeshComponent* Part, const FLinearColor& Color, float Emissive)
	{
		const int32 Stride = Part->NumCustomDataFloats;
		if (Stride < 4 || Part->PerInstanceSMCustomData.Num() != Part->GetInstanceCount() * Stride)
		{
			return 0;
		}
		int32 Count = 0;
		for (int32 Instance = 0; Instance < Part->GetInstanceCount(); ++Instance)
		{
			const float* Data = &Part->PerInstanceSMCustomData[Instance * Stride];
			Count += Data[0] == Color.R && Data[1] == Color.G && Data[2] == Color.B && Data[3] == Emissive ? 1 : 0;
		}
		return Count;
	}

	static int32 ExpectedInstances(TConstArrayView<FDTTelegraph> Telegraphs)
	{
		int32 Sum = 0;
		for (const FDTTelegraph& T : Telegraphs)
		{
			Sum += ADTTelegraphRenderer::InstancesDrawn(T);
		}
		return Sum;
	}

	static int32 CountShape(TConstArrayView<FDTTelegraph> Telegraphs, EDTTelegraphShape Shape)
	{
		int32 Count = 0;
		for (const FDTTelegraph& T : Telegraphs)
		{
			Count += T.Shape == Shape ? 1 : 0;
		}
		return Count;
	}

	/** Plays one telemetry interval with a Turret in range of hero 0; returns the CSV threat_actions_per_min cell. */
	static FString PlayTurretInterval(bool bForward, int64& OutTurretActions)
	{
		const float Range = static_cast<float>(FMath::Min(DTConst::ROLE_TURRET_FIRE_RANGE_UU, DTConst::DIR_THREAT_RING_MAX_UU));
		FDTRunState Run;
		Run.Start(3, 1);
		FDTWorldView View;
		View.HeroPositions = { FVector2f(0.5f * Range, 0.0f) };
		View.HeroIntangible = { true }; // the rounds fly on; this test reads telemetry only
		FDTSimContext Context;
		Context.StepSeconds = StepSeconds;
		Context.Run = &Run;
		Context.View = &View;

		FDTTelemetryConfig Config;
		Config.Seed = 3;
		Config.NumPlayers = 1;
		Config.StepSeconds = StepSeconds;
		Config.IntervalSeconds = 1.0f;
		Config.Build = TEXT("test");
		Config.Culture = TEXT("en");
		Config.WallStart = TEXT("2026-01-01T00:00:00.000Z");
		FDTTelemetryCore Core;
		Core.Start(Config);

		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
		Pool.SetPattern(0, EDTFirePattern::Burst);
		Pool.SetFacing(0, FVector2f(1.0f, 0.0f));

		// One interval: the tell (C.ROLE.TURRET_TELL_S) ends and one burst fires; the next tell waits C.ROLE.TURRET_PERIOD_S.
		const int32 Steps = FMath::RoundToInt32(Config.IntervalSeconds / StepSeconds);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			Context.StepIndex = Step;
			Pool.StepEnemies(Context);
			if (bForward)
			{
				UDTEnemySubsystem::ForwardThreatActions(Pool, Core);
			}
			Pool.ResolveHits(Context);
			FDTTelemetrySample Sample;
			Sample.LiveEnemies = Pool.Num();
			Sample.Heroes = View.HeroPositions;
			Sample.Enemies = Pool.GetPositions();
			Core.EndStep(Step, Sample);
		}
		Core.End(TEXT("test"));
		FString Csv;
		FString Jsonl;
		Core.TakeOutput(Csv, Jsonl);
		OutTurretActions = Pool.GetCounters().ThreatActions[static_cast<int32>(EDTEnemyRole::Turret)];

		TArray<FString> Rows;
		Csv.ParseIntoArrayLines(Rows, true);
		TArray<FString> Header;
		FDTTelemetryCore::GetCsvHeader().ParseIntoArray(Header, TEXT(","), false);
		const int32 Column = Header.IndexOfByKey(FString(TEXT("threat_actions_per_min")));
		TArray<FString> Cells;
		if (Rows.Num() > 1)
		{
			Rows[1].ParseIntoArray(Cells, TEXT(","), false);
		}
		return Cells.IsValidIndex(Column) ? Cells[Column] : FString(TEXT("<missing>"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmTelegraphRender, "Dusktracer.Enemies.TelegraphRender", DTTelegraphTest::Flags) // T-ENM-25
bool FDTEnmTelegraphRender::RunTest(const FString& Parameters)
{
	using namespace DTTelegraphTest;

	// Footprints: a band is one quad over its length and width; a ring and an arc are tessellated around the origin.
	{
		const float Mesh = 100.0f;
		const float Z = static_cast<float>(DTConst::ENEMIES_TELEGRAPH_Z_UU);
		FDTTelegraph Band;
		Band.Shape = EDTTelegraphShape::Band;
		Band.Origin = FVector2f(100.0f, 0.0f);
		Band.Dir = FVector2f(0.0f, 1.0f);
		Band.Length = 400.0f;
		Band.Width = 80.0f;
		TArray<FTransform> Out;
		ADTTelegraphRenderer::AppendTransforms(Band, Mesh, Z, Out);
		TestEqual(TEXT("a band is one quad"), Out.Num(), 1);
		if (Out.Num() == 1)
		{
			TestTrue(TEXT("band centred halfway along its length"), Out[0].GetLocation().Equals(FVector(100.0, 200.0, Z), 1.0e-3));
			TestTrue(TEXT("band scaled to length x width"), Out[0].GetScale3D().Equals(FVector(4.0, 0.8, 1.0), 1.0e-4));
			TestTrue(TEXT("band turned along its direction"), Out[0].GetRotation().GetForwardVector().Equals(FVector(0.0, 1.0, 0.0), 1.0e-4));
		}
		TestTrue(TEXT("above the floor, below hulls and rounds"), Z > 0.0f && Z < static_cast<float>(DTConst::SIM_PLANE_Z_UU));

		FDTTelegraph Ring;
		Ring.Shape = EDTTelegraphShape::Ring;
		Ring.Origin = FVector2f(-50.0f, 30.0f);
		Ring.Length = 300.0f;
		Ring.Width = 60.0f;
		Out.Reset();
		ADTTelegraphRenderer::AppendTransforms(Ring, Mesh, Z, Out);
		TestEqual(TEXT("a ring is RingSegments quads"), Out.Num(), ADTTelegraphRenderer::RingSegments);
		bool bOnBand = true;
		for (const FTransform& Q : Out)
		{
			bOnBand &= FMath::IsNearlyEqual(FVector2D(Q.GetLocation().X + 50.0, Q.GetLocation().Y - 30.0).Size(), 270.0, 1.0e-2);
		}
		TestTrue(TEXT("ring quads sit on the middle of the ring band"), bOnBand);

		FDTTelegraph Arc;
		Arc.Shape = EDTTelegraphShape::Arc;
		Arc.Length = 300.0f;
		Arc.Width = 90.0f;
		TestEqual(TEXT("a quarter arc is a quarter of the ring's quads"), ADTTelegraphRenderer::InstancesFor(Arc), ADTTelegraphRenderer::RingSegments / 4);

		// Layers: the band outside the footprint, the fill inside it, in both directions for every shape.
		const float EdgeUU = static_cast<float>(DTConst::PRE_TELEGRAPH_EDGE_UU);
		const float RimUU = static_cast<float>(DTConst::PRE_TELEGRAPH_RIM_UU);
		FDTTelegraph Layer;
		TestTrue(TEXT("a band grows"), ADTTelegraphRenderer::OffsetShape(Band, EdgeUU, Layer));
		TestEqual(TEXT("grown band length"), Layer.Length, Band.Length + 2.0f * EdgeUU);
		TestEqual(TEXT("grown band width"), Layer.Width, Band.Width + 2.0f * EdgeUU);
		TestTrue(TEXT("grown band starts behind the origin"), Layer.Origin.Equals(Band.Origin - FVector2f(0.0f, EdgeUU), 1.0e-3f));
		TestTrue(TEXT("a band has a fill"), ADTTelegraphRenderer::OffsetShape(Band, -RimUU, Layer) && Layer.Width == Band.Width - 2.0f * RimUU);
		FDTTelegraph Aim = Band;
		Aim.Shape = EDTTelegraphShape::AimLine;
		Aim.Width = 2.0f * static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
		TestFalse(TEXT("an aim line is too thin for a fill"), ADTTelegraphRenderer::OffsetShape(Aim, -RimUU, Layer));
		TestEqual(TEXT("an aim line draws its band and its line"), ADTTelegraphRenderer::InstancesDrawn(Aim), 2);
		TestTrue(TEXT("a ring grows on both edges"), ADTTelegraphRenderer::OffsetShape(Ring, EdgeUU, Layer) && Layer.Length == Ring.Length + EdgeUU && Layer.Width == Ring.Width + 2.0f * EdgeUU);
		FDTTelegraph Lane;
		Lane.Shape = EDTTelegraphShape::RingGap;
		Lane.Length = 600.0f;
		Lane.Width = 40.0f;
		TestTrue(TEXT("growing a lane shape narrows the lane"), ADTTelegraphRenderer::OffsetShape(Lane, EdgeUU, Layer) && Layer.Width < Lane.Width && Layer.Length == Lane.Length + EdgeUU);
		FDTTelegraph Hazard;
		Hazard.Shape = EDTTelegraphShape::HazardRing;
		Hazard.Length = 800.0f;
		TestTrue(TEXT("growing the hazard shrinks the safe circle"), ADTTelegraphRenderer::OffsetShape(Hazard, EdgeUU, Layer) && Layer.Length == Hazard.Length - EdgeUU);
		TestEqual(TEXT("the fill starts dim"), ADTTelegraphRenderer::FillScale(0.0f), static_cast<float>(DTConst::PRE_TELEGRAPH_FILL_MIN));
		TestEqual(TEXT("the fill lands at full emissive"), ADTTelegraphRenderer::FillScale(1.0f), 1.0f);
		TestTrue(TEXT("the top layer stays under hulls and rounds"), Z + 2.0f * static_cast<float>(DTConst::PRE_TELEGRAPH_LAYER_UU) < static_cast<float>(DTConst::SIM_PLANE_Z_UU));
	}

	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies))
	{
		return false;
	}
	ADTTelegraphRenderer* Renderer = Enemies->GetTelegraphRenderer();
	if (!TestNotNull(TEXT("one telegraph renderer"), Renderer))
	{
		return false;
	}
	Enemies->SimRender(0.0f);
	TestEqual(TEXT("an empty arena draws no telegraph"), Renderer->GetInstanceCount(), 0);

	const int32 ActorsBefore = W.CountActors();
	if (Sim->GetView().HeroPositions.IsEmpty())
	{
		Sim->GetView().HeroPositions = { FVector2f::ZeroVector }; // the stress point a run leaves at the origin
	}
	const FVector2f Hero = Sim->GetView().HeroPositions[0];
	for (int32 Copy = 0; Copy < 3; ++Copy)
	{
		Enemies->GetPool().Spawn(EDTEnemyRole::Rusher, Hero + FVector2f(0.5f * DTConst::ROLE_RUSHER_TRIGGER_UU, 150.0f * Copy));
	}
	Enemies->GetPool().Spawn(EDTEnemyRole::Turret, Hero + FVector2f(0.0f, -0.5f * DTConst::ROLE_TURRET_FIRE_RANGE_UU));

	bool bBoth = false;
	int32 MaxSeen = 0;
	bool bFollows = true;
	for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ && !bBoth; ++Step)
	{
		Sim->StepNow(1);
		Enemies->SimRender(0.5f);
		const TConstArrayView<FDTTelegraph> Telegraphs = Sim->GetView().Telegraphs;
		bFollows &= Renderer->GetInstanceCount() == ExpectedInstances(Telegraphs);
		MaxSeen = FMath::Max(MaxSeen, Telegraphs.Num());
		bBoth = CountShape(Telegraphs, EDTTelegraphShape::Band) == 3 && CountShape(Telegraphs, EDTTelegraphShape::AimLine) > 0;
	}
	TestTrue(TEXT("three Rusher bands and a Turret aim line show"), bBoth);
	TestTrue(TEXT("instances follow the Telegraphs count every frame"), bFollows);
	{
		const TConstArrayView<FDTTelegraph> Shown = Sim->GetView().Telegraphs;
		const FLinearColor Red = DTPal::Linear(DTPal::Telegraph);
		const float Full = static_cast<float>(DTConst::ENEMIES_TELEGRAPH_EMISSIVE);
		TestEqual(TEXT("each telegraph has one dark band quad"), CountCarrying(Renderer->GetInstances(), DTPal::Linear(DTPal::Telegraph_Edge), 1.0f), Shown.Num());
		TestTrue(TEXT("every telegraph's rim is Pal.Telegraph at full emissive"), CountCarrying(Renderer->GetInstances(), Red, Full) >= Shown.Num());
		for (const FDTTelegraph& T : Shown)
		{
			FDTTelegraph Fill;
			if (ADTTelegraphRenderer::OffsetShape(T, -static_cast<float>(DTConst::PRE_TELEGRAPH_RIM_UU), Fill))
			{
				TestTrue(TEXT("a fill burns at its Progress"), CountCarrying(Renderer->GetInstances(), Red, Full * ADTTelegraphRenderer::FillScale(T.Progress)) > 0);
			}
		}
	}
	TestEqual(TEXT("telegraph colour is the Pal.Telegraph token"), UDTEnemySubsystem::GetTelegraphColor(), DTPal::Linear(DTPal::Telegraph));
	TestEqual(TEXT("no actor per telegraph"), W.CountActors(), ActorsBefore);

	Enemies->RequestClear();
	Sim->StepNow(1);
	Enemies->SimRender(0.0f);
	TestEqual(TEXT("instances follow the count down"), Renderer->GetInstanceCount(), 0);
	return MaxSeen > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmThreatTelemetry, "Dusktracer.Enemies.ThreatTelemetry", DTTelegraphTest::Flags) // T-ENM-26
bool FDTEnmThreatTelemetry::RunTest(const FString& Parameters)
{
	using namespace DTTelegraphTest;
	int64 Without = 0;
	const FString Off = PlayTurretInterval(false, Without);
	int64 With = 0;
	const FString On = PlayTurretInterval(true, With);
	TestEqual(TEXT("one Turret volley in the interval"), With, static_cast<int64>(1));
	TestEqual(TEXT("without the call the column stays zero"), FCString::Atod(*Off), 0.0);
	// One action in a one-second interval, per C.TEL.RATE_UNIT_S.
	TestEqual(TEXT("the call fills threat_actions_per_min"), FCString::Atod(*On), static_cast<double>(DTConst::TEL_RATE_UNIT_S), 1.0e-3);
	return true;
}

#endif
