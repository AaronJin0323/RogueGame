// Progression tests (02_Systems/progression.md Acceptance, T-PRG-01..11).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Progression; Quit" -unattended -nullrhi
// T-PRG-04 (docs_lint curve check) is a Tools/ check owned outside this package; it stays PLANNED.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Progression/DTGemPool.h"
#include "Progression/DTGemSubsystem.h"
#include "Progression/DTProgression.h"
#include "Progression/DTXPBar.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTPrgTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ);

	struct FEventLog : IDTRunEventSink
	{
		TArray<FDTGameEvent> Events;
		virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) override { Events.Add(Event); }
	};

	/** Sum of xp_to_next from level From up to (not including) level To. */
	static int64 XpBetween(int32 From, int32 To)
	{
		int64 Sum = 0;
		for (int32 L = From; L < To; ++L)
		{
			Sum += FDTProgression::XPToNext(L);
		}
		return Sum;
	}

	static void StepPool(FDTGemPool& Pool, const TArray<FVector2f>& Heroes, FDTPickupStepResult& Out, const TArray<float>& RadiusMul = {})
	{
		FDTPickupHeroes H;
		H.Positions = Heroes;
		H.RadiusMul = RadiusMul;
		Pool.Step(Dt, H, Out);
	}

	static FDTGemSpawn Gem(float X, float Y, int32 Value)
	{
		FDTGemSpawn G;
		G.Position = FVector2f(X, Y);
		G.Value = Value;
		return G;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgSharedXP, "Dusktracer.Progression.SharedXP", DTPrgTest::Flags) // T-PRG-01
bool FDTPrgSharedXP::RunTest(const FString& Parameters)
{
	FDTRunState Run;
	Run.Start(1, 2);
	FDTProgression::EnsureCurve(Run);
	DTPrgTest::FEventLog Log;
	Run.SetEventSink(&Log);
	const TArray<float> Mul = { 1.0f, 2.0f };
	TArray<int64> Credits;
	TestTrue(TEXT("credit routes"), FDTProgression::CreditPickup(Run, 1, 5, Mul, nullptr, &Credits));
	TestEqual(TEXT("player 0 gets value x 1"), Run.GetPlayer(0).XP, 5);
	TestEqual(TEXT("player 1 gets value x 2"), Run.GetPlayer(1).XP, 10);
	TestEqual(TEXT("both on the same curve"), Run.GetPlayer(0).XPToNext, Run.GetPlayer(1).XPToNext);
	TestTrue(TEXT("credits reported per player"), Credits == TArray<int64>{ 5, 10 });
	TestEqual(TEXT("XPTotal counts each player's credit once (0)"), Run.GetPlayer(0).XPTotal, static_cast<int64>(5));
	TestEqual(TEXT("XPTotal counts each player's credit once (1)"), Run.GetPlayer(1).XPTotal, static_cast<int64>(10));
	TestEqual(TEXT("one pickup event"), Log.Events.Num(), 1);
	if (Log.Events.Num() == 1)
	{
		TestEqual(TEXT("the event is a pickup"), static_cast<int32>(Log.Events[0].Kind), static_cast<int32>(EDTGameEventKind::Pickup));
		TestEqual(TEXT("the event carries the collector"), Log.Events[0].OwnerId, 1);
	}
	TestEqual(TEXT("the collector counts the pickup"), Run.GetPlayer(1).Pickups, 1);
	TestEqual(TEXT("the other player does not"), Run.GetPlayer(0).Pickups, 0);
	TestFalse(TEXT("a collector outside the run changes nothing"), FDTProgression::CreditPickup(Run, 7, 5, Mul));
	TestEqual(TEXT("unchanged"), Run.GetPlayer(0).XP, 5);
	Run.SetEventSink(nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgCurve, "Dusktracer.Progression.Curve", DTPrgTest::Flags) // T-PRG-02
bool FDTPrgCurve::RunTest(const FString& Parameters)
{
	for (int32 Row = 0; Row < DTCat::NumXpLevels; ++Row)
	{
		TestEqual(*FString::Printf(TEXT("row %d is level %d"), Row, Row + 1), DTCat::XpCurve[Row].Level, Row + 1);
		TestEqual(*FString::Printf(TEXT("XPToNext(%d) matches the CSV"), Row + 1), FDTProgression::XPToNext(Row + 1), DTCat::XpCurve[Row].XpToNext);
	}
	int64 Prev = DTCat::XpCurve[DTCat::NumXpLevels - 1].XpToNext;
	for (int32 L = DTCat::NumXpLevels + 1; L <= DTCat::NumXpLevels + 30; ++L)
	{
		const int64 Expected = FMath::RoundToInt64(static_cast<double>(Prev) * static_cast<double>(DTConst::XP_GROWTH));
		TestEqual(*FString::Printf(TEXT("XPToNext(%d) follows the growth rule"), L), static_cast<int64>(FMath::Max(0, FDTProgression::XPToNext(L))), Expected);
		Prev = Expected;
	}

	// Well beyond the table the level still advances (no cap).
	FDTRunState Run;
	Run.Start(1, 1);
	const int32 Target = DTCat::NumXpLevels * 3;
	FDTProgression::AddXP(Run, 0, DTPrgTest::XpBetween(1, Target));
	TestEqual(TEXT("level far past the table"), Run.GetPlayer(0).Level, Target);
	TestEqual(TEXT("no remainder"), Run.GetPlayer(0).XP, 0);
	TestEqual(TEXT("XPToNext follows"), Run.GetPlayer(0).XPToNext, FDTProgression::XPToNext(Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgAnchors, "Dusktracer.Progression.Anchors", DTPrgTest::Flags) // T-PRG-03
bool FDTPrgAnchors::RunTest(const FString& Parameters)
{
	// The check is offline on the CSV (ref_reached_s is a doc-only column, not generated; D-014).
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/09_Catalog/xp_curve.csv"));
	TArray<FString> Lines;
	if (!TestTrue(TEXT("xp_curve.csv readable"), FFileHelper::LoadFileToStringArray(Lines, *Path)) || !TestTrue(TEXT("has rows"), Lines.Num() > 1))
	{
		return false;
	}
	TArray<FString> Header;
	Lines[0].TrimStartAndEnd().ParseIntoArray(Header, TEXT(","), false);
	const int32 ColLevel = Header.IndexOfByKey(TEXT("level"));
	const int32 ColNext = Header.IndexOfByKey(TEXT("xp_to_next"));
	const int32 ColRef = Header.IndexOfByKey(TEXT("ref_reached_s"));
	if (!TestTrue(TEXT("columns present"), ColLevel != INDEX_NONE && ColNext != INDEX_NONE && ColRef != INDEX_NONE))
	{
		return false;
	}
	TMap<int32, double> RefReached;
	int32 Rows = 0;
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		TArray<FString> Cells;
		Lines[Index].TrimStartAndEnd().ParseIntoArray(Cells, TEXT(","), false);
		if (Cells.Num() <= FMath::Max3(ColLevel, ColNext, ColRef))
		{
			continue;
		}
		const int32 Level = FCString::Atoi(*Cells[ColLevel]);
		++Rows;
		TestEqual(*FString::Printf(TEXT("generated table matches the CSV at level %d"), Level), FDTProgression::XPToNext(Level), FCString::Atoi(*Cells[ColNext]));
		if (!Cells[ColRef].IsEmpty())
		{
			RefReached.Add(Level, FCString::Atod(*Cells[ColRef]));
		}
	}
	TestEqual(TEXT("generated table has every CSV row"), Rows, DTCat::NumXpLevels);

	const double Tol = DTConst::XP_ANCHOR_TOLERANCE_S;
	const int32 L8 = DTConst::OFFER_SECOND_KEEPER_LEVEL;
	const int32 L16 = DTConst::OFFER_REFORGING_LEVEL;
	if (TestTrue(TEXT("L8 has a reference time"), RefReached.Contains(L8)))
	{
		const double At = RefReached[L8];
		AddInfo(FString::Printf(TEXT("L8 at %.0f s (target %d s, tolerance %.0f s)"), At, DTConst::LEVEL_L8_MIN, Tol));
		TestTrue(TEXT("L8 within tolerance of C.LEVEL.L8_MIN"), FMath::Abs(At - DTConst::LEVEL_L8_MIN) <= Tol);
	}
	if (TestTrue(TEXT("L16 has a reference time"), RefReached.Contains(L16)))
	{
		const double At = RefReached[L16];
		AddInfo(FString::Printf(TEXT("L16 at %.0f s (target %d s, tolerance %.0f s)"), At, DTConst::LEVEL_L16_MIN, Tol));
		TestTrue(TEXT("L16 within tolerance of C.LEVEL.L16_MIN"), FMath::Abs(At - DTConst::LEVEL_L16_MIN) <= Tol);
	}
	// The hand budget (level hands + milestones) is reached by the level 1 + budget; it must land within the tolerance
	// of the boss time.
	const int32 Budget = DTConst::OFFER_BUDGET_LEVEL_HANDS + DTConst::OFFER_BUDGET_MILESTONES;
	const int32 BudgetLevel = 1 + Budget;
	if (TestTrue(TEXT("budget level has a reference time"), RefReached.Contains(BudgetLevel)))
	{
		const double At = RefReached[BudgetLevel];
		AddInfo(FString::Printf(TEXT("hand %d (L%d) at %.0f s (boss at %d s)"), Budget, BudgetLevel, At, DTConst::RUN_LENGTH_TO_BOSS));
		TestTrue(TEXT("hand budget reached within tolerance of C.RUN.LENGTH_TO_BOSS"), FMath::Abs(At - DTConst::RUN_LENGTH_TO_BOSS) <= Tol);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgOverflow, "Dusktracer.Progression.Overflow", DTPrgTest::Flags) // T-PRG-05
bool FDTPrgOverflow::RunTest(const FString& Parameters)
{
	FDTRunState Run;
	Run.Start(1, 1);
	FDTProgression::EnsureCurve(Run);
	constexpr int32 Remainder = 3;
	TArray<FDTLevelGain> Gains;
	const int32 Gained = FDTProgression::AddXP(Run, 0, DTPrgTest::XpBetween(1, 5) + Remainder, &Gains);
	const FDTPlayerRunState& P = Run.GetPlayer(0);
	TestEqual(TEXT("four levels from one grant"), Gained, 4);
	TestEqual(TEXT("level 5"), P.Level, 5);
	TestEqual(TEXT("remainder kept"), P.XP, Remainder);
	TestEqual(TEXT("XPToNext for level 5"), P.XPToNext, FDTProgression::XPToNext(5));
	TestEqual(TEXT("XPTotal counts everything"), P.XPTotal, DTPrgTest::XpBetween(1, 5) + Remainder);
	TestEqual(TEXT("one hand per level"), P.Offer.PendingHands.Num(), 4);
	for (int32 Index = 0; Index < P.Offer.PendingHands.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("hand %d in level order"), Index), P.Offer.PendingHands[Index].Level, Index + 2);
		TestEqual(*FString::Printf(TEXT("gain %d in level order"), Index), Gains[Index].Level, Index + 2);
		TestEqual(TEXT("hand id left to the offer system"), P.Offer.PendingHands[Index].HandId, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("cards left to the offer system"), P.Offer.PendingHands[Index].Cards.Num(), 0);
	}
	// No XP is lost or capped while hands are pending.
	FDTProgression::AddXP(Run, 0, DTPrgTest::XpBetween(5, 7));
	TestEqual(TEXT("levels keep coming with hands pending"), P.Level, 7);
	TestEqual(TEXT("remainder still kept"), P.XP, Remainder);
	TestEqual(TEXT("hands keep queueing"), P.Offer.PendingHands.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgLevelGrants, "Dusktracer.Progression.LevelGrants", DTPrgTest::Flags) // T-PRG-06
bool FDTPrgLevelGrants::RunTest(const FString& Parameters)
{
	FDTRunState Run;
	Run.Start(1, 1);
	FDTProgression::EnsureCurve(Run);
	const FDTPlayerRunState Before = Run.GetPlayer(0);
	const int32 Top = DTConst::OFFER_REFORGING_LEVEL + 1;
	FDTProgression::AddXP(Run, 0, DTPrgTest::XpBetween(1, Top));
	const FDTPlayerRunState& P = Run.GetPlayer(0);
	TestEqual(TEXT("one hand per level"), P.Offer.PendingHands.Num(), Top - 1);
	for (const FDTOfferHand& Hand : P.Offer.PendingHands)
	{
		const EDTHandKind Want = Hand.Level == DTConst::OFFER_SECOND_KEEPER_LEVEL ? EDTHandKind::Keeper
			: Hand.Level == DTConst::OFFER_REFORGING_LEVEL ? EDTHandKind::Reforging : EDTHandKind::Level;
		TestEqual(*FString::Printf(TEXT("hand kind at level %d"), Hand.Level), static_cast<int32>(Hand.HandKind), static_cast<int32>(Want));
	}
	// A level by itself grants nothing else: no rerolls, Favor, Keepers, damage or pickups.
	TestEqual(TEXT("rerolls unchanged"), P.Offer.RerollsLeft, Before.Offer.RerollsLeft);
	TestEqual(TEXT("banish unchanged"), P.Offer.BanishLeft, Before.Offer.BanishLeft);
	TestEqual(TEXT("no blessing granted"), P.Keepers.Owned.Num(), Before.Keepers.Owned.Num());
	int32 FavorSum = 0;
	for (const int32 F : P.Keepers.Favor)
	{
		FavorSum += F;
	}
	TestEqual(TEXT("no Favor"), FavorSum, 0);
	TestEqual(TEXT("no pickups counted"), P.Pickups, Before.Pickups);

	// Level hands queue before chest hands (offers.md R-OFF-14).
	FDTRunState Run2;
	Run2.Start(1, 1);
	FDTProgression::EnsureCurve(Run2);
	FDTProgression::QueueHand(Run2.GetPlayer(0).Offer, EDTHandKind::Chest, 1);
	FDTProgression::AddXP(Run2, 0, DTPrgTest::XpBetween(1, 3));
	const TArray<FDTOfferHand>& Q = Run2.GetPlayer(0).Offer.PendingHands;
	if (TestEqual(TEXT("three hands"), Q.Num(), 3))
	{
		TestEqual(TEXT("level 2 first"), Q[0].Level, 2);
		TestEqual(TEXT("level 3 second"), Q[1].Level, 3);
		TestEqual(TEXT("the chest last"), static_cast<int32>(Q[2].HandKind), static_cast<int32>(EDTHandKind::Chest));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgGemPool, "Dusktracer.Progression.GemPool", DTPrgTest::Flags) // T-PRG-07
bool FDTPrgGemPool::RunTest(const FString& Parameters)
{
	using namespace DTPrgTest;
	FDTGemPool Pool;
	int64 Expected = 0;
	for (int32 Index = 0; Index < DTConst::PICKUP_GEM_CAP; ++Index)
	{
		TestTrue(TEXT("added under the cap"), Pool.Spawn(Gem(static_cast<float>(Index), 2000.0f, 1 + Index % 3), 0) == EDTGemSpawnResult::Added);
		Expected += 1 + Index % 3;
	}
	TestEqual(TEXT("at the cap"), Pool.Num(), DTConst::PICKUP_GEM_CAP);
	const int32 OldestBefore = Pool.GetValues()[0];
	TestTrue(TEXT("a spawn at the cap merges"), Pool.Spawn(Gem(0.0f, 0.0f, 7), 1) == EDTGemSpawnResult::Merged);
	Expected += 7;
	TestEqual(TEXT("count unchanged"), Pool.Num(), DTConst::PICKUP_GEM_CAP);
	TestEqual(TEXT("value went to the oldest gem"), Pool.GetValues()[0], OldestBefore + 7);
	TestEqual(TEXT("XP on the floor unchanged by the merge"), Pool.GetFloorXP(), Expected);
	TestTrue(TEXT("non-positive gems are rejected"), Pool.Spawn(Gem(0.0f, 0.0f, 0), 1) == EDTGemSpawnResult::Rejected);

	// Gems never expire: thousands of steps with no hero in reach change nothing.
	FDTPickupStepResult Out;
	for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ * 10; ++Step)
	{
		StepPool(Pool, { FVector2f(0.0f, -3000.0f) }, Out);
	}
	TestEqual(TEXT("gems never expire"), Pool.Num(), DTConst::PICKUP_GEM_CAP);
	TestEqual(TEXT("nothing collected"), Out.Gems.Num(), 0);

	// The subsystem adds no Actor per gem.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	World->BeginPlay();
	UDTGemSubsystem* Gems = World->GetSubsystem<UDTGemSubsystem>();
	if (TestNotNull(TEXT("gem subsystem"), Gems))
	{
		int32 ActorsBefore = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorsBefore;
		}
		for (int32 Index = 0; Index < 500; ++Index)
		{
			Gems->Spawn(Gem(static_cast<float>(Index), 3000.0f, 1));
		}
		Gems->SpawnChest(FVector2f(-3000.0f, -3000.0f));
		Gems->SimRender(0.0f);
		int32 ActorsAfter = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorsAfter;
		}
		TestEqual(TEXT("500 gems live"), Gems->GetPool().Num(), 500);
		TestEqual(TEXT("gems and chests add no Actor"), ActorsAfter, ActorsBefore);
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgMagnet, "Dusktracer.Progression.Magnet", DTPrgTest::Flags) // T-PRG-08
bool FDTPrgMagnet::RunTest(const FString& Parameters)
{
	using namespace DTPrgTest;
	const float Radius = static_cast<float>(DTConst::PICKUP_RADIUS_UU);
	const float Collect = static_cast<float>(DTConst::PICKUP_COLLECT_RADIUS_UU);
	const float Accel = static_cast<float>(DTConst::PICKUP_MAGNET_ACCEL_UU_S2);
	const float SpeedMax = static_cast<float>(DTConst::PICKUP_MAGNET_SPEED_MAX_UU_S);
	TestTrue(TEXT("magnet top speed above hero speed (combat_core.md)"), DTConst::PICKUP_MAGNET_SPEED_MAX_UU_S > DTConst::HERO_MOVE_SPEED_UU_S);

	// Binding: inside the radius binds to the nearest player; outside every radius stays put.
	{
		FDTGemPool Pool;
		const TArray<FVector2f> Heroes = { FVector2f(0.0f, 0.0f), FVector2f(2.0f * Radius, 0.0f) };
		Pool.Spawn(Gem(0.9f * Radius, 0.0f, 1), 0);   // 0.9R from player 0, 1.1R from player 1 -> player 0
		Pool.Spawn(Gem(1.2f * Radius, 0.0f, 1), 0);   // 1.2R from 0 (outside), 0.8R from 1 -> player 1
		Pool.Spawn(Gem(0.0f, 5.0f * Radius, 1), 0);   // outside every radius
		FDTPickupStepResult Out;
		StepPool(Pool, Heroes, Out);
		TestEqual(TEXT("gem 0 binds to the nearest player"), Pool.GetBoundTo()[0], 0);
		TestEqual(TEXT("gem 1 binds to the nearest player in range"), Pool.GetBoundTo()[1], 1);
		TestEqual(TEXT("gem 2 stays free"), Pool.GetBoundTo()[2], static_cast<int32>(INDEX_NONE));
		TestTrue(TEXT("gem 2 does not move"), Pool.GetPositions()[2] == FVector2f(0.0f, 5.0f * Radius));
		TestTrue(TEXT("bound gems move toward their player (0)"), Pool.GetPositions()[0].X < 0.9f * Radius);
		TestTrue(TEXT("bound gems move toward their player (1)"), Pool.GetPositions()[1].X > 1.2f * Radius);
	}

	// Motion: accelerates by C.PICKUP.MAGNET_ACCEL_UU_S2 up to the cap, collected inside the collection distance.
	{
		FDTGemPool Pool;
		const float RadiusMul = 10.0f;
		const float Start = 0.9f * Radius * RadiusMul;
		Pool.Spawn(Gem(Start, 0.0f, 4), 0);
		const TArray<FVector2f> Heroes = { FVector2f::ZeroVector };
		const TArray<float> Mul = { RadiusMul };
		FDTPickupStepResult Out;
		float PrevSpeed = 0.0f;
		float MaxSpeed = 0.0f;
		int32 Steps = 0;
		bool bAccelOk = true;
		while (Out.Gems.Num() == 0 && Steps < DTConst::SIM_FIXED_HZ * 10)
		{
			StepPool(Pool, Heroes, Out, Mul);
			++Steps;
			if (Pool.Num() == 1)
			{
				const float Speed = Pool.GetVelocities()[0].Size();
				bAccelOk &= Speed <= PrevSpeed + Accel * Dt + 0.01f && Speed <= SpeedMax + 0.01f;
				MaxSpeed = FMath::Max(MaxSpeed, Speed);
				PrevSpeed = Speed;
				TestTrue(TEXT("not collected while outside the collection distance"), Pool.GetPositions()[0].Size() > Collect);
			}
		}
		TestTrue(TEXT("acceleration and top speed respected"), bAccelOk);
		TestEqual(TEXT("reaches the top speed"), MaxSpeed, SpeedMax, 0.01f);
		if (TestEqual(TEXT("collected"), Out.Gems.Num(), 1))
		{
			TestEqual(TEXT("by player 0"), Out.Gems[0].PlayerId, 0);
			TestEqual(TEXT("with its value"), Out.Gems[0].Value, 4);
		}
		TestEqual(TEXT("gone from the pool"), Pool.Num(), 0);
		// Lower bound on the time: ramp to the cap, then cruise.
		const float RampDist = SpeedMax * SpeedMax / (2.0f * Accel);
		const float MinSeconds = SpeedMax / Accel + (Start - Collect - RampDist) / SpeedMax;
		TestTrue(TEXT("not faster than the motion allows"), Steps >= FMath::FloorToInt(MinSeconds / Dt));
	}

	// A bound gem whose player leaves is freed.
	{
		FDTGemPool Pool;
		Pool.Spawn(Gem(0.9f * Radius, 0.0f, 1), 0);
		FDTPickupStepResult Out;
		StepPool(Pool, { FVector2f::ZeroVector }, Out);
		TestEqual(TEXT("bound"), Pool.GetBoundTo()[0], 0);
		const TArray<FVector2f> Heroes = { FVector2f::ZeroVector };
		const TArray<bool> Present = { false };
		FDTPickupHeroes H;
		H.Positions = Heroes;
		H.Present = Present;
		Pool.Step(Dt, H, Out);
		TestEqual(TEXT("freed when its player leaves"), Pool.GetBoundTo()[0], static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("not collected by an absent player"), Out.Gems.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgMagnetAll, "Dusktracer.Progression.MagnetAll", DTPrgTest::Flags) // T-PRG-09
bool FDTPrgMagnetAll::RunTest(const FString& Parameters)
{
	using namespace DTPrgTest;
	const float Radius = static_cast<float>(DTConst::PICKUP_RADIUS_UU);
	const float Far = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU) * 0.9f;
	{
		FDTGemPool Pool;
		const TArray<FVector2f> Heroes = { FVector2f::ZeroVector, FVector2f(Far, Far) };
		Pool.Spawn(Gem(Far + 0.5f * Radius, Far, 2), 0); // will bind to player 1 first
		FDTPickupStepResult Out;
		StepPool(Pool, Heroes, Out);
		TestEqual(TEXT("pre-bound to player 1"), Pool.GetBoundTo()[0], 1);
		for (int32 Index = 0; Index < 20; ++Index)
		{
			Pool.Spawn(Gem(-Far + Index * 10.0f, Far - Index * 50.0f, 1), 0);
		}
		TestEqual(TEXT("MagnetAll binds every free gem"), Pool.MagnetAll(0), 20);
		// Bound: the longest distance at the top speed plus the ramp, plus one step of slack.
		const float MaxDist = FMath::Sqrt(2.0f) * 2.0f * Far;
		const int32 Bound = FMath::CeilToInt((MaxDist / DTConst::PICKUP_MAGNET_SPEED_MAX_UU_S
			+ static_cast<float>(DTConst::PICKUP_MAGNET_SPEED_MAX_UU_S) / DTConst::PICKUP_MAGNET_ACCEL_UU_S2) / Dt) + 1;
		int32 ByZero = 0;
		int32 ByOne = 0;
		for (int32 Step = 0; Step < Bound && Pool.Num() > 0; ++Step)
		{
			Out.Reset();
			StepPool(Pool, Heroes, Out);
			for (const FDTGemCollect& C : Out.Gems)
			{
				(C.PlayerId == 0 ? ByZero : ByOne) += 1;
			}
		}
		TestEqual(TEXT("every gem collected within the bound"), Pool.Num(), 0);
		TestEqual(TEXT("player 0 collected the magnetised gems"), ByZero, 20);
		TestEqual(TEXT("the already-bound gem kept its player"), ByOne, 1);
	}

	// A touched chest queues one chest hand for its collector and magnetises their gems (through the subsystem).
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	World->BeginPlay();
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTGemSubsystem* Gems = World->GetSubsystem<UDTGemSubsystem>();
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("gems"), Gems))
	{
		Sim->StartRun(4, 1);
		Sim->StepNow(1);
		const FVector2f Hero = Sim->GetView().HeroPositions[0];
		Gems->Spawn(Gem(Hero.X + Far * 0.5f, Hero.Y, 3));
		Gems->SpawnChest(Hero);
		Sim->StepNow(1);
		const FDTPlayerRunState& P = Sim->GetRun().GetPlayer(0);
		TestEqual(TEXT("chest collected"), Gems->GetPool().NumChests(), 0);
		if (TestEqual(TEXT("one chest hand"), P.Offer.PendingHands.Num(), 1))
		{
			TestEqual(TEXT("it is a chest hand"), static_cast<int32>(P.Offer.PendingHands[0].HandKind), static_cast<int32>(EDTHandKind::Chest));
		}
		TestEqual(TEXT("the far gem is now bound to the collector"), Gems->GetPool().GetBoundTo()[0], 0);
		Sim->StepNow(DTConst::SIM_FIXED_HZ * 3);
		TestEqual(TEXT("the gem reached the collector"), Gems->GetPool().Num(), 0);
		TestEqual(TEXT("its XP was credited"), P.XP, 3);
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgDeterminism, "Dusktracer.Progression.Determinism", DTPrgTest::Flags) // T-PRG-10
bool FDTPrgDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTPrgTest;
	struct FTrace
	{
		TArray<FVector2f> Positions;
		TArray<int32> CollectSteps;
		TArray<int32> LevelSteps;
	};
	auto RunOnce = [](FTrace& Trace)
	{
		FDTRunState Run;
		Run.Start(9, 2);
		FDTProgression::EnsureCurve(Run);
		FDTGemPool Pool;
		FDTPickupStepResult Out;
		TArray<FDTLevelGain> Levels;
		const TArray<float> Mul = { 1.0f, 1.0f };
		for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ * 20; ++Step)
		{
			if (Step % 3 == 0)
			{
				Pool.Spawn(Gem(FMath::Sin(Step * 0.37f) * 900.0f, FMath::Cos(Step * 0.11f) * 900.0f, 1 + Step % 4), Step);
			}
			if (Step == DTConst::SIM_FIXED_HZ * 5)
			{
				Pool.SpawnChest(FVector2f(300.0f, 0.0f));
			}
			const float T = Step * Dt;
			const TArray<FVector2f> Heroes = { FVector2f(FMath::Cos(T) * 600.0f, FMath::Sin(T) * 600.0f), FVector2f(-400.0f + T * 20.0f, 100.0f) };
			Out.Reset();
			StepPool(Pool, Heroes, Out);
			for (const FDTGemCollect& C : Out.Gems)
			{
				Trace.CollectSteps.Add(Step * 10 + C.PlayerId);
				Levels.Reset();
				FDTProgression::CreditPickup(Run, C.PlayerId, C.Value, Mul, &Levels);
				for (const FDTLevelGain& L : Levels)
				{
					Trace.LevelSteps.Add(Step * 100 + L.Level * 2 + L.OwnerId);
				}
			}
			Trace.Positions.Append(Pool.GetPositions().GetData(), Pool.Num());
		}
	};
	FTrace A;
	FTrace B;
	RunOnce(A);
	RunOnce(B);
	TestTrue(TEXT("some gems were collected"), A.CollectSteps.Num() > 0);
	TestTrue(TEXT("some levels were gained"), A.LevelSteps.Num() > 0);
	TestTrue(TEXT("identical gem positions"), A.Positions == B.Positions);
	TestTrue(TEXT("identical collection steps"), A.CollectSteps == B.CollectSteps);
	TestTrue(TEXT("identical level timeline"), A.LevelSteps == B.LevelSteps);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrgXPBar, "Dusktracer.Progression.XPBar", DTPrgTest::Flags) // T-PRG-11
bool FDTPrgXPBar::RunTest(const FString& Parameters)
{
	FDTRunState Run;
	Run.Start(2, 2);
	FDTProgression::EnsureCurve(Run);
	FDTProgression::AddXP(Run, 1, DTPrgTest::XpBetween(1, 3) + 5);
	TArray<FDTXPBarRow> Rows;
	const TArray<double> LastUp = { -1.0, 10.0 };
	FDTXPBar::BuildRows(Run, 10.2, LastUp, Rows);
	if (TestEqual(TEXT("one bar per player"), Rows.Num(), 2))
	{
		TestEqual(TEXT("player 0 at level 1"), Rows[0].Level, 1);
		TestEqual(TEXT("player 1 at level 3"), Rows[1].Level, 3);
		TestEqual(TEXT("progress to the next level"), Rows[1].Progress01, 5.0f / FDTProgression::XPToNext(3), 1e-5f);
		TestEqual(TEXT("pending-hand count shown"), Rows[1].PendingHands, 2);
		TestFalse(TEXT("no flash without a level-up"), Rows[0].bFlash);
		TestTrue(TEXT("level-up flash"), Rows[1].bFlash);
	}
	// Still one bar per player while hands are open/queued (it never hides behind the hand UI).
	FDTProgression::QueueHand(Run.GetPlayer(0).Offer, EDTHandKind::Chest, 1);
	FDTXPBar::BuildRows(Run, 60.0, LastUp, Rows);
	TestEqual(TEXT("bars stay with hands queued"), Rows.Num(), 2);
	TestFalse(TEXT("flash ends"), Rows.Num() == 2 && Rows[1].bFlash);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
