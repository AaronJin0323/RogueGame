// Sim pause at a step boundary (07_Technical R-TEC-14; offers.md R-OFF-11: in solo the sim pauses while a hand is open).
// A hand queued inside a step must stop the frame's step loop on that exact step, not at the frame boundary.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Core.Sim; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Offers/DTOfferSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTSimPauseTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr double StepS = 1.0 / static_cast<double>(DTConst::SIM_FIXED_HZ);

	/** Pickups phase: queues a level-up hand for player 0 on step QueueAt (as progression does on a level-up). */
	struct FLevelUpAt : IDTSimSystem
	{
		int64 QueueAt = -1; // none until a test sets it
		virtual void SimStep(const FDTSimContext& Context) override
		{
			if (Context.StepIndex == QueueAt)
			{
				FDTOfferHand Request;
				Request.HandKind = EDTHandKind::Level;
				Request.Level = 2;
				Context.Run->GetPlayer(0).Offer.PendingHands.Add(Request);
			}
		}
	};

	/** Director phase (the last): records every step that ran to its end. */
	struct FStepLog : IDTSimSystem
	{
		TArray<int64> Steps;
		virtual void SimStep(const FDTSimContext& Context) override { Steps.Add(Context.StepIndex); }
	};

	/** A frame long enough for every allowed substep (C.SIM.MAX_SUBSTEPS) to be due. */
	static float FullFrame()
	{
		return static_cast<float>(StepS * DTConst::SIM_MAX_SUBSTEPS + StepS * 0.25);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSimPauseAtStep, "Dusktracer.Core.Sim.PauseAtStep", DTSimPauseTest::Flags)
bool FDTSimPauseAtStep::RunTest(const FString& Parameters)
{
	using namespace DTSimPauseTest;
	TestTrue(TEXT("a frame can run several steps"), DTConst::SIM_MAX_SUBSTEPS >= 2);

	UDTSimSubsystem* Sim = NewObject<UDTSimSubsystem>(GetTransientPackage());
	Sim->StartRun(5, 1);
	FLevelUpAt LevelUp;
	FStepLog Log;
	Sim->Register(EDTSimPhase::Pickups, &LevelUp);
	Sim->Register(EDTSimPhase::Director, &Log);
	// What the offer system does: a hand pending at a step end requests the pause.
	Sim->OnStepEnd.AddLambda([Sim]()
	{
		if (Sim->GetRun().GetPlayer(0).Offer.PendingHands.Num() > 0)
		{
			Sim->RequestPause();
		}
	});

	// Settle the accumulator on a step boundary, then queue the level-up on the first step of a frame with several due.
	TestEqual(TEXT("a full frame runs every substep with no pause"), Sim->AdvanceFrame(FullFrame()), DTConst::SIM_MAX_SUBSTEPS);
	const int64 LevelStep = Sim->GetStepIndex();
	LevelUp.QueueAt = LevelStep;
	const int32 Ran = Sim->AdvanceFrame(FullFrame());
	TestEqual(TEXT("the frame stops on the level-up step"), Ran, 1);
	TestEqual(TEXT("the step index is one past the level-up step"), Sim->GetStepIndex(), LevelStep + 1);
	TestTrue(TEXT("the level-up step ran every phase"), Log.Steps.Num() > 0 && Log.Steps.Last() == LevelStep);
	TestTrue(TEXT("pause requested"), Sim->IsPauseRequested());

	// Paused: no step, no time.
	const double Held = Sim->GetClock().GetAccumulator();
	TestEqual(TEXT("no step while paused"), Sim->AdvanceFrame(FullFrame() * 4.0f), 0);
	TestEqual(TEXT("sim time does not pass while paused"), Sim->GetClock().GetAccumulator(), Held, 1e-12);
	TestEqual(TEXT("still at the level-up step"), Sim->GetStepIndex(), LevelStep + 1);

	// The hand is picked: the request clears and the sim resumes.
	Sim->GetRun().GetPlayer(0).Offer.PendingHands.Reset();
	Sim->ClearPauseRequest();
	TestEqual(TEXT("resumes after the request is cleared"), Sim->AdvanceFrame(FullFrame()), DTConst::SIM_MAX_SUBSTEPS);

	// StepNow (tests, bots) runs exactly what it is asked, whatever the request.
	Sim->RequestPause();
	const int64 Before = Sim->GetStepIndex();
	Sim->StepNow(3);
	TestEqual(TEXT("StepNow ignores the pause request"), Sim->GetStepIndex(), Before + 3);
	Sim->Unregister(&LevelUp);
	Sim->Unregister(&Log);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSimPauseOffers, "Dusktracer.Core.Sim.PauseOnPendingHand", DTSimPauseTest::Flags)
bool FDTSimPauseOffers::RunTest(const FString& Parameters)
{
	using namespace DTSimPauseTest;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTSimPauseWorld")));
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTOfferSubsystem* Offers = World->GetSubsystem<UDTOfferSubsystem>();
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("offers"), Offers))
	{
		Sim->StartRun(9, 1);
		FLevelUpAt LevelUp;
		FStepLog Log;
		Sim->Register(EDTSimPhase::Pickups, &LevelUp);
		Sim->Register(EDTSimPhase::Director, &Log);
		Sim->ClearPauseRequest();
		TestEqual(TEXT("no hand: the full frame runs"), Sim->AdvanceFrame(FullFrame()), DTConst::SIM_MAX_SUBSTEPS);
		const int64 LevelStep = Sim->GetStepIndex();
		LevelUp.QueueAt = LevelStep;
		TestEqual(TEXT("the offer system stops the frame on the level-up step"), Sim->AdvanceFrame(FullFrame()), 1);
		TestTrue(TEXT("last step run is the level-up step"), Log.Steps.Num() > 0 && Log.Steps.Last() == LevelStep);
		TestTrue(TEXT("the offer system requested the pause"), Sim->IsPauseRequested());
		TestEqual(TEXT("held while the hand is pending"), Sim->AdvanceFrame(FullFrame()), 0);

		// No hand left (picked): the offer flow clears the request.
		Sim->GetRun().GetPlayer(0).Offer.PendingHands.Reset();
		Sim->GetRun().GetPlayer(0).Offer.PatronPath = EDTPath::Storm; // no run-start Patron hand in this test
		Offers->UpdateFlow();
		TestFalse(TEXT("cleared once no hand is pending or open"), Sim->IsPauseRequested());
		TestEqual(TEXT("resumes"), Sim->AdvanceFrame(FullFrame()), DTConst::SIM_MAX_SUBSTEPS);
		Sim->Unregister(&LevelUp);
		Sim->Unregister(&Log);
	}

	for (AActor* Actor : FActorRange(World))
	{
		if (Actor)
		{
			Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
		}
	}
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
