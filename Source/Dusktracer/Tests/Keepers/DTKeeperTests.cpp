// Keeper tests (02_Systems/keepers.md T-KEE-01..20) plus one behaviour test per archetype Key on a Pulse Rifle.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Keepers; Quit" -unattended -nullrhi
// T-KEE-15 IntercessionBand and T-KEE-18 Budget are bot probes (Tools/power_split.py ablations): PLANNED until bot v1.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTPaletteGen.h"
#include "Keepers/DTKeeperSim.h"
#include "Keepers/DTKeeperSpec.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTKeeperTest
{
	using namespace DTWeaponTestHarness;

	static const FName PulseRifle(TEXT("Weapon.PulseRifle"));
	static const FName RingEmitter(TEXT("Weapon.RingEmitter"));
	static const FName OrbitalDrones(TEXT("Weapon.OrbitalDrones"));
	static const FName ArcBlade(TEXT("Weapon.ArcBlade"));

	static FName Blessing(const TCHAR* Archetype, const TCHAR* Column)
	{
		return FName(*FString::Printf(TEXT("Blessing.%s.%s"), Archetype, Column));
	}

	static int32 Row(const TCHAR* Archetype)
	{
		return DTKeepers::FindArchetype(FName(*FString::Printf(TEXT("Archetype.%s"), Archetype)));
	}

	/** The weapon harness plus the Keeper sim, in 07_Technical §4 phase order (enemies apply statuses and pulls, never move). */
	struct FDTKeeperHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTEnemyPool Enemies;
		FDTBulletPool Bullets;
		FDTWeaponSim Weapons;
		FDTKeeperSim Keepers;
		int64 Step = 0;
		TArray<FDTHit> HitLog;
		TArray<FDTStatusApply> StatusLog;
		TArray<FDTDisplacement> MoveLog;

		explicit FDTKeeperHarness(int32 Seed = 7, int32 NumPlayers = 1)
		{
			Run.Start(Seed, NumPlayers);
			Weapons.SetEnemies(&Enemies);
			View.HeroPositions.Init(FVector2f::ZeroVector, Run.GetNumPlayers());
			View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), Run.GetNumPlayers());
			View.HeroIntangible.Init(false, Run.GetNumPlayers());
		}

		FDTSimContext Context()
		{
			FDTSimContext Ctx;
			Ctx.StepSeconds = Dt;
			Ctx.StepIndex = Step;
			Ctx.Run = &Run;
			Ctx.View = &View;
			return Ctx;
		}

		FDTPlayerRunState& P(int32 Owner = 0) { return Run.GetPlayer(Owner); }

		void Patron(EDTPath Path, int32 Owner = 0, bool bWeapon = true)
		{
			P(Owner).Keepers.Primary = Path;
			P(Owner).Offer.PatronPath = Path;
			if (bWeapon)
			{
				Weapons.Give(Run, Owner, PulseRifle, 1); // D-015: the run starts with the Pulse Rifle
			}
			Keepers.RebuildFlags(Run, Owner, Weapons);
		}

		void Commit(EDTPath Path, int32 Owner = 0)
		{
			P(Owner).Keepers.Second = Path;
			P(Owner).Keepers.bCommitDone = true;
			P(Owner).Offer.SecondPath = Path;
			Keepers.RebuildFlags(Run, Owner, Weapons);
		}

		bool Bless(FName Id, int32 Owner = 0)
		{
			const bool bOk = DTOffers::AddBlessing(P(Owner).Keepers, Id);
			Keepers.RebuildFlags(Run, Owner, Weapons);
			return bOk;
		}

		int32 AddEnemy(const FVector2f& Position, float Hp = ToughHp)
		{
			const int32 Index = Enemies.Spawn(EDTEnemyRole::Swarm, Position);
			if (Index != INDEX_NONE)
			{
				Enemies.SetHp(Index, Hp);
			}
			return Index;
		}

		/** A 5 x 5 block of tough enemies ahead of the hero (within the cleave and slam reach). */
		void AddBlock(float NearX = 200.0f, float Spacing = 45.0f)
		{
			for (int32 X = 0; X < 5; ++X)
			{
				for (int32 Y = -2; Y <= 2; ++Y)
				{
					AddEnemy(FVector2f(NearX + Spacing * X, Spacing * Y));
				}
			}
		}

		void StepOnce()
		{
			const FDTSimContext Ctx = Context();
			// Enemies: queued statuses and displacements first (no movement in these tests).
			for (const FDTStatusApply& Apply : View.StatusApplies)
			{
				StatusLog.Add(Apply);
				Enemies.ApplyStatus(Apply);
			}
			View.StatusApplies.Reset();
			for (const FDTDisplacement& Move : View.Displacements)
			{
				MoveLog.Add(Move);
				if (Enemies.IsHandleValid(FDTEnemyHandle{ Move.EnemyIndex, Move.EnemySerial }))
				{
					Enemies.SetPosition(Move.EnemyIndex, Enemies.GetPositions()[Move.EnemyIndex] + Move.Delta);
				}
			}
			View.Displacements.Reset();
			Enemies.Publish(View);
			Weapons.StepWeapons(Ctx, &Bullets);
			Bullets.Step(Dt, View);
			Weapons.ObserveBullets(Ctx);
			Keepers.CaptureHits(Ctx, &Enemies);
			HitLog.Append(View.Hits);
			Enemies.ResolveHits(Ctx);
			Enemies.Publish(View);
			Weapons.StepEffects(Ctx, &Bullets);
			Keepers.StepEffects(Ctx, &Weapons);
			++Step;
		}

		void StepMany(int32 Steps)
		{
			for (int32 Index = 0; Index < Steps; ++Index)
			{
				StepOnce();
			}
		}

		double Damage(uint16 Source) const
		{
			double Sum = 0.0;
			for (const FDTHit& Hit : HitLog)
			{
				Sum += Hit.SourceId == Source ? Hit.Damage : 0.0;
			}
			return Sum;
		}

		double TotalDamage() const
		{
			double Sum = 0.0;
			for (const FDTHit& Hit : HitLog)
			{
				Sum += Hit.Damage;
			}
			return Sum;
		}

		/** Keeper share of damage by source (Tools/power_split.py reads the same split from telemetry). */
		double KeeperShare(EDTPath Path) const
		{
			const double Total = TotalDamage();
			return Total > 0.0 ? Damage(DTKeepers::KeeperSource(Path)) / Total : 0.0;
		}

		int32 CountHits(uint16 Source) const
		{
			int32 Count = 0;
			for (const FDTHit& Hit : HitLog)
			{
				Count += Hit.SourceId == Source ? 1 : 0;
			}
			return Count;
		}

		int32 CountStatuses(EDTStatus Status, uint16 Source) const
		{
			int32 Count = 0;
			for (const FDTStatusApply& Apply : StatusLog)
			{
				Count += (Apply.Status == Status && Apply.SourceId == Source) ? 1 : 0;
			}
			return Count;
		}

		int32 FrozenEnemies() const
		{
			int32 Count = 0;
			for (int32 Index = 0; Index < Enemies.Num(); ++Index)
			{
				Count += Enemies.GetAux(Index).IsFrozen() ? 1 : 0;
			}
			return Count;
		}

		/** Crafts Count player hits on enemy 0 (weapon slot 0) and runs only capture + effects (statistical procs). */
		void CraftedStep(int32 Count, float Damage, int32 Owner = 0)
		{
			const FDTSimContext Ctx = Context();
			Enemies.Publish(View);
			View.Hits.Reset();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FDTHit& Hit = View.Hits.AddDefaulted_GetRef();
				Hit.EnemyIndex = 0;
				Hit.EnemySerial = View.EnemySerials[0];
				Hit.OwnerId = Owner;
				Hit.Damage = Damage;
				Hit.WeaponSlot = 0;
			}
			Keepers.CaptureHits(Ctx, &Enemies);
			View.Hits.Reset();
			Keepers.StepEffects(Ctx, nullptr);
			++Step;
		}

		const FDTFlagEntry* Built(EDTPath Path, DTCat::EFlag Flag, int32 Owner = 0) const
		{
			return Keepers.GetBuiltFlags(Owner, Path).Entries.FindByPredicate([Flag](const FDTFlagEntry& E)
			{
				return E.Flag == Flag && E.Params.GetWord(DTFlagParam::On).IsNone();
			});
		}
	};

	/** Steps a harness for a few seconds of fire. */
	constexpr int32 FireSteps = 600;
}

// ---- catalog and data load ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersCatalog, "Dusktracer.Keepers.Catalog", DTWeaponTestHarness::TestFlags) // T-KEE-01
bool FDTKeepersCatalog::RunTest(const FString& Parameters)
{
	const TArray<EDTPath> Keepers = DTOffers::RunKeepers();
	TestEqual(TEXT("three S1 Keepers"), Keepers.Num(), 3);
	TestTrue(TEXT("Storm, Forge, Tide"), Keepers.Contains(EDTPath::Storm) && Keepers.Contains(EDTPath::Forge) && Keepers.Contains(EDTPath::Tide));
	TestEqual(TEXT("rows = Keepers x archetypes"), DTCat::NumArchetypes, 3 * DTConst::KEEPER_ARCHETYPES_PER_KEEPER);
	int32 Blessings = 0;
	for (int32 R = 0; R < DTCat::NumArchetypes; ++R)
	{
		const DTCat::FArchetypeRow& Row = DTCat::Archetypes[R];
		TestTrue(FString::Printf(TEXT("%s is an S1 Keeper"), Row.Id), Row.Path == EDTPath::Storm || Row.Path == EDTPath::Forge || Row.Path == EDTPath::Tide);
		for (int32 Column = 0; Column < DTConst::KEEPER_BLESSINGS_PER_ARCHETYPE; ++Column)
		{
			const FDTBlessingInfo Info = DTOffers::FindBlessing(FName(Row.BlessingIds[Column]));
			TestTrue(FString::Printf(TEXT("%s found"), Row.BlessingIds[Column]), Info.IsValid() && Info.Archetype == R && Info.Column == Column);
			TestTrue(FString::Printf(TEXT("%s accepted at load"), Row.BlessingIds[Column]), DTKeepers::IsBlessingAccepted(R, Column));
			++Blessings;
		}
	}
	TestEqual(TEXT("blessings = 3 x archetypes x per archetype"), Blessings,
		3 * DTConst::KEEPER_ARCHETYPES_PER_KEEPER * DTConst::KEEPER_BLESSINGS_PER_ARCHETYPE);
	TestEqual(TEXT("the generated catalog loads clean"), DTKeepers::CatalogErrors().Num(), 0);
	for (const FString& Error : DTKeepers::CatalogErrors())
	{
		AddError(Error);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersNoStatOnly, "Dusktracer.Keepers.NoStatOnly", DTWeaponTestHarness::TestFlags) // T-KEE-02
bool FDTKeepersNoStatOnly::RunTest(const FString& Parameters)
{
	TSet<DTCat::EFlag> KeyFlags;
	for (int32 R = 0; R < DTCat::NumArchetypes; ++R)
	{
		const DTCat::FArchetypeRow& Row = DTCat::Archetypes[R];
		FString Error;
		TestTrue(FString::Printf(TEXT("%s Key carries its key flag: %s"), Row.Id, *Error),
			DTKeepers::ValidateBlessing(Row.Effects[0], EDTBlessingRole::Key, Row.KeyFlag, &Error));
		TestFalse(FString::Printf(TEXT("%s key flag unique"), Row.Id), KeyFlags.Contains(Row.KeyFlag));
		KeyFlags.Add(Row.KeyFlag);
	}
	using DTCat::EFlag;
	TestFalse(TEXT("a stat-only Amp fixture is rejected"), DTKeepers::ValidateBlessing(TEXT("Stat.Damage{mult=+0.10}"), EDTBlessingRole::Amp, EFlag::Split));
	TestFalse(TEXT("a stat delta dressed as a flag is rejected"), DTKeepers::ValidateBlessing(TEXT("Flag.Split{damage=+0.10;crit_chance=+0.05}"), EDTBlessingRole::Amp, EFlag::Split));
	TestFalse(TEXT("a Key with a second behaviour flag is rejected"), DTKeepers::ValidateBlessing(TEXT("Flag.Chain{jumps=2} + Flag.Split{children=2}"), EDTBlessingRole::Key, EFlag::Chain));
	TestFalse(TEXT("a Key without its key flag is rejected"), DTKeepers::ValidateBlessing(TEXT("Flag.Echo{count=1}"), EDTBlessingRole::Key, EFlag::Chain));
	TestTrue(TEXT("a StatusApply rider is not a behaviour"), DTKeepers::ValidateBlessing(TEXT("Flag.Pull{distance_uu=120} + Flag.StatusApply{status=Status.Chill;chance=0.5}"), EDTBlessingRole::Key, EFlag::Pull));
	TestTrue(TEXT("a flag parameter change is accepted"), DTKeepers::ValidateBlessing(TEXT("Flag.Echo{dmg=+0.15}"), EDTBlessingRole::Amp, EFlag::Echo));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersExpressionCsv, "Dusktracer.Keepers.ExpressionCsv", DTWeaponTestHarness::TestFlags) // T-KEE-20
bool FDTKeepersExpressionCsv::RunTest(const FString& Parameters)
{
	TArray<DTCat::FArchetypeRow> Rows(DTCat::Archetypes, DTCat::NumArchetypes);
	TArray<FString> Errors;
	TestTrue(TEXT("the generated rows validate"), DTKeepers::ValidateCatalog(Rows, Errors));
	{
		TArray<DTCat::FArchetypeRow> Missing = Rows;
		Missing.RemoveAt(Missing.Num() - 1);
		TArray<FString> E;
		TestFalse(TEXT("a missing row is rejected"), DTKeepers::ValidateCatalog(Missing, E));
	}
	{
		TArray<DTCat::FArchetypeRow> Unknown = Rows;
		Unknown[1].Effects[1] = TEXT("Flag.Bogus{x=1}");
		TArray<FString> E;
		TestFalse(TEXT("an unknown flag tag is rejected"), DTKeepers::ValidateCatalog(Unknown, E));
		Unknown = Rows;
		Unknown[1].Effects[2] = TEXT("Flag.Chain{status=Status.Wet}");
		E.Reset();
		TestFalse(TEXT("an unknown status tag is rejected"), DTKeepers::ValidateCatalog(Unknown, E));
	}
	{
		TArray<DTCat::FArchetypeRow> Asymmetric = Rows;
		const int32 Tempest = DTKeeperTest::Row(TEXT("Storm.Tempest"));
		Asymmetric[Tempest].IncompatFlags = 0u; // Hammer still lists Split
		TArray<FString> E;
		TestFalse(TEXT("an asymmetric incompatibility is rejected"), DTKeepers::ValidateCatalog(Asymmetric, E));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersTierLineLength, "Dusktracer.Keepers.TierLineLength", DTWeaponTestHarness::TestFlags) // T-KEE-08
bool FDTKeepersTierLineLength::RunTest(const FString& Parameters)
{
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("DT/Text/ST_Keepers.csv");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		AddError(FString::Printf(TEXT("cannot read %s"), *Path));
		return false;
	}
	// Minimal CSV reader for the loc source format (quoted fields may hold commas, "" escapes a quote).
	auto ParseLine = [](const FString& Line)
	{
		TArray<FString> Fields;
		FString Field;
		bool bQuoted = false;
		for (int32 I = 0; I < Line.Len(); ++I)
		{
			const TCHAR C = Line[I];
			if (bQuoted && C == TEXT('"') && I + 1 < Line.Len() && Line[I + 1] == TEXT('"'))
			{
				Field.AppendChar(C);
				++I;
			}
			else if (C == TEXT('"'))
			{
				bQuoted = !bQuoted;
			}
			else if (C == TEXT(',') && !bQuoted)
			{
				Fields.Add(Field);
				Field.Reset();
			}
			else
			{
				Field.AppendChar(C);
			}
		}
		Fields.Add(Field);
		return Fields;
	};
	auto StripArgs = [](const FString& S)
	{
		FString Out;
		int32 Depth = 0;
		for (const TCHAR C : S)
		{
			Depth += C == TEXT('{') ? 1 : 0;
			if (Depth == 0 && !FChar::IsWhitespace(C))
			{
				Out.AppendChar(C);
			}
			Depth -= (C == TEXT('}') && Depth > 0) ? 1 : 0;
		}
		return Out;
	};
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	TMap<FString, TPair<FString, FString>> ByKey;
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		const TArray<FString> F = ParseLine(Lines[Index]);
		if (F.Num() >= 3)
		{
			ByKey.Add(F[0], TPair<FString, FString>(F[1], F[2]));
		}
	}
	for (int32 R = 0; R < DTCat::NumArchetypes; ++R)
	{
		const FString Prefix = FString(DTCat::Archetypes[R].TextPrefix).Mid(FString(TEXT("ST_Keepers.")).Len());
		for (int32 Tier = 1; Tier <= 3; ++Tier)
		{
			const FString Key = FString::Printf(TEXT("%s.Tier%d"), *Prefix, Tier);
			const TPair<FString, FString>* Line = ByKey.Find(Key);
			if (!TestNotNull(FString::Printf(TEXT("%s exists"), *Key), Line))
			{
				continue;
			}
			TArray<FString> Words;
			Line->Key.ParseIntoArrayWS(Words);
			TestTrue(FString::Printf(TEXT("%s en %d words <= C.TIER.LINE_WORDS_EN_MAX"), *Key, Words.Num()), Words.Num() <= DTConst::TIER_LINE_WORDS_EN_MAX);
			const int32 Chars = StripArgs(Line->Value).Len();
			TestTrue(FString::Printf(TEXT("%s zh %d chars <= C.TIER.LINE_CHARS_ZH_MAX"), *Key, Chars), Chars > 0 && Chars <= DTConst::TIER_LINE_CHARS_ZH_MAX);
		}
		for (const TCHAR* Suffix : { TEXT("Ult.Desc"), TEXT("Layer.Name"), TEXT("Layer.Desc") })
		{
			TestTrue(FString::Printf(TEXT("%s.%s exists"), *Prefix, Suffix), ByKey.Contains(FString::Printf(TEXT("%s.%s"), *Prefix, Suffix)));
		}
	}
	return true;
}

// ---- flags on weapons -----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersFlagsOnAllWeapons, "Dusktracer.Keepers.FlagsOnAllWeapons", DTWeaponTestHarness::TestFlags) // T-KEE-03
bool FDTKeepersFlagsOnAllWeapons::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Storm);
	H.Weapons.Give(H.Run, 0, RingEmitter, 1);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Amp1")));
	H.Keepers.RebuildFlags(H.Run, 0, H.Weapons); // a rebuild replaces, never stacks
	const uint16 Storm = DTKeepers::KeeperSource(EDTPath::Storm);
	int32 Splits = 0;
	for (const FDTFlagEntry& Entry : H.Weapons.GetPlayerFlags(0))
	{
		Splits += (Entry.Flag == DTCat::EFlag::Split && Entry.SourceId == Storm) ? 1 : 0;
	}
	TestEqual(TEXT("one Split entry for the Keeper, whatever the rebuilds"), Splits, 1);
	const FDTFlagEntry* Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
	TestTrue(TEXT("Key + Amp1 stack once: children 3 + 1"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Children), 4.0f));
	TestTrue(TEXT("child_dmg 0.45 + 0.10"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::ChildDmg), 0.55f, 1e-4f));
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		TestTrue(FString::Printf(TEXT("slot %d carries the Split"), Slot), H.Weapons.ComposeSlot(H.Run, 0, Slot).HasFlag(DTCat::EFlag::Split));
	}
	// A weapon acquired later gets it too; where it cannot express Split natively the fallback applies, never nothing.
	H.Weapons.Give(H.Run, 0, ArcBlade, 1);
	const FDTWeaponSpec Later = H.Weapons.ComposeSlot(H.Run, 0, 2);
	const FDTResolvedFlag* LaterSplit = Later.FindFlag(DTCat::EFlag::Split);
	TestTrue(TEXT("a later weapon carries the Split"), LaterSplit != nullptr);
	TestTrue(TEXT("Arc Blade expresses Split by its capability fallback"), LaterSplit && LaterSplit->IsFallback());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersNoFitMultiplier, "Dusktracer.Keepers.NoFitMultiplier", DTWeaponTestHarness::TestFlags) // T-KEE-17
bool FDTKeepersNoFitMultiplier::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Storm);
	H.Weapons.Give(H.Run, 0, RingEmitter, 1);
	H.Weapons.Give(H.Run, 0, OrbitalDrones, 1);
	H.Weapons.Give(H.Run, 0, ArcBlade, 1);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	const FDTWeaponSpec First = H.Weapons.ComposeSlot(H.Run, 0, 0);
	const FDTResolvedFlag* Reference = First.FindFlag(DTCat::EFlag::Split);
	if (!TestNotNull(TEXT("Split on the Pulse Rifle"), Reference))
	{
		return false;
	}
	for (int32 Slot = 1; Slot < 4; ++Slot)
	{
		const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, Slot);
		const FDTResolvedFlag* Other = Spec.FindFlag(DTCat::EFlag::Split);
		TestTrue(FString::Printf(TEXT("slot %d carries Split"), Slot), Other != nullptr);
		for (const FDTFlagParams::FNum& Num : Reference->Entry.Params.Nums)
		{
			TestTrue(FString::Printf(TEXT("slot %d %s identical"), Slot, *Num.Name.ToString()), Other && Other->Entry.Params.Get(Num.Name) == Num.Value);
		}
	}
	return true;
}

// ---- prerequisites, primary and second, leading, tiers -----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersPrerequisites, "Dusktracer.Keepers.Prerequisites", DTWeaponTestHarness::TestFlags) // T-KEE-04
bool FDTKeepersPrerequisites::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Storm);
	const FDTOfferPool Pool;
	auto Legal = [&H, &Pool](FName Id)
	{
		FDTOfferCard Card;
		Card.Kind = EDTCardKind::Blessing;
		Card.ContentId = Id;
		return DTOffers::IsLegal(H.P(), Pool, Card);
	};
	TestTrue(TEXT("the Key is legal"), Legal(Blessing(TEXT("Storm.Tempest"), TEXT("Key"))));
	TestFalse(TEXT("an Amp without the Key is not"), Legal(Blessing(TEXT("Storm.Tempest"), TEXT("Amp1"))));
	TestFalse(TEXT("the Trait without the Key is not"), Legal(Blessing(TEXT("Storm.Tempest"), TEXT("Trait"))));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	TestTrue(TEXT("an Amp with the Key is"), Legal(Blessing(TEXT("Storm.Tempest"), TEXT("Amp1"))));
	const bool bTraitAtOne = Legal(Blessing(TEXT("Storm.Tempest"), TEXT("Trait")));
	TestEqual(TEXT("the Trait needs C.KEEPER.TRAIT_PREREQ_CARDS cards"), bTraitAtOne, DTConst::KEEPER_TRAIT_PREREQ_CARDS <= 1);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Amp1")));
	TestEqual(TEXT("the Trait at two cards"), Legal(Blessing(TEXT("Storm.Tempest"), TEXT("Trait"))), DTConst::KEEPER_TRAIT_PREREQ_CARDS <= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersPrimarySecond, "Dusktracer.Keepers.PrimarySecond", DTWeaponTestHarness::TestFlags) // T-KEE-05
bool FDTKeepersPrimarySecond::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	const FDTOfferPool Pool;
	auto Legal = [&H, &Pool](FName Id)
	{
		FDTOfferCard Card;
		Card.Kind = EDTCardKind::Blessing;
		Card.ContentId = Id;
		return DTOffers::IsLegal(H.P(), Pool, Card);
	};
	FDTOfferCard PatronCard;
	PatronCard.Kind = EDTCardKind::Keeper;
	PatronCard.ContentId = DTOffers::KeeperId(EDTPath::Storm);
	const FDTPickOutcome Patron = DTOffers::ApplyPickState(H.P(), EDTHandKind::Patron, PatronCard);
	TestTrue(TEXT("the Patron pick fixes the primary"), Patron.bPatron && H.P().Keepers.Primary == EDTPath::Storm);
	TestTrue(TEXT("before the commitment: primary blessings only"), Legal(Blessing(TEXT("Storm.ArcGrid"), TEXT("Key"))) && !Legal(Blessing(TEXT("Forge.Hammer"), TEXT("Key"))));
	FDTOfferCard SecondCard;
	SecondCard.Kind = EDTCardKind::Keeper;
	SecondCard.ContentId = DTOffers::KeeperId(EDTPath::Forge);
	DTOffers::ApplyPickState(H.P(), EDTHandKind::Keeper, SecondCard);
	TestTrue(TEXT("the commitment fixes the second"), H.P().Keepers.Second == EDTPath::Forge && H.P().Keepers.bCommitDone);
	TestTrue(TEXT("the primary never changes"), H.P().Keepers.Primary == EDTPath::Storm);
	TestTrue(TEXT("after it: the second's blessings"), Legal(Blessing(TEXT("Forge.Furnace"), TEXT("Key"))));
	TestFalse(TEXT("the third Keeper never"), Legal(Blessing(TEXT("Tide.Frost"), TEXT("Key"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersLeadingArchetype, "Dusktracer.Keepers.LeadingArchetype", DTWeaponTestHarness::TestFlags) // T-KEE-06
bool FDTKeepersLeadingArchetype::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperRunState K;
	K.Primary = EDTPath::Storm;
	const int32 Storm = static_cast<int32>(EDTPath::Storm);
	TestTrue(TEXT("no Key: none"), K.Leading[Storm].IsNone());
	DTOffers::AddBlessing(K, Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	DTOffers::AddBlessing(K, Blessing(TEXT("Storm.Tempest"), TEXT("Amp1")));
	DTOffers::AddBlessing(K, Blessing(TEXT("Storm.ArcGrid"), TEXT("Key")));
	TestEqual(TEXT("most cards wins"), K.Leading[Storm], FName(TEXT("Archetype.Storm.Tempest")));
	DTOffers::AddBlessing(K, Blessing(TEXT("Storm.ArcGrid"), TEXT("Amp1")));
	TestEqual(TEXT("a tie goes to the latest card"), K.Leading[Storm], FName(TEXT("Archetype.Storm.ArcGrid")));
	// The Keeper layer reads the same leader: the Intercession variant follows it.
	TestEqual(TEXT("the variant is the leader's"), DTKeepers::ComposeIntercession(K).Variant, Row(TEXT("Storm.ArcGrid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersTiers, "Dusktracer.Keepers.Tiers", DTWeaponTestHarness::TestFlags) // T-KEE-07
bool FDTKeepersTiers::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Storm);
	const int32 Storm = static_cast<int32>(EDTPath::Storm);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Amp1")));
	TestEqual(TEXT("below C.TIER.FAVOR_T1: no tier"), H.P().Keepers.Tier[Storm], 0);
	const FDTFlagEntry* Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
	const float KeyChance = DTKeepers::PathParam(EDTPath::Storm, DTCat::EFlag::Split, DTFlagParam::Chance, 0.0f);
	TestTrue(TEXT("no tier line yet: the Key's chance"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), KeyChance));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Amp2")));
	TestEqual(TEXT("T1 at exactly C.TIER.FAVOR_T1"), H.P().Keepers.Tier[Storm], 1);
	Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
	TestTrue(TEXT("Tempest Tier1 changes the Split flag: chance +0.10"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), KeyChance + 0.10f, 1e-4f));
	// Arc Grid catches up and leads on the tie (latest card): the tier lines follow the leader, the tier stays.
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Amp1")));
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Amp2")));
	TestEqual(TEXT("T2 at C.TIER.FAVOR_T2"), H.P().Keepers.Tier[Storm], 2);
	TestEqual(TEXT("Arc Grid leads"), H.P().Keepers.Leading[Storm], FName(TEXT("Archetype.Storm.ArcGrid")));
	Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
	const FDTFlagEntry* Chain = H.Built(EDTPath::Storm, DTCat::EFlag::Chain);
	TestTrue(TEXT("Tempest's tier line left with the lead"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), KeyChance));
	TestTrue(TEXT("Arc Grid T1: jumps 2 + 1"), Chain && FMath::IsNearlyEqual(Chain->Params.Get(DTFlagParam::Jumps), 3.0f));
	TestTrue(TEXT("Arc Grid T2: range 350 + 90"), Chain && FMath::IsNearlyEqual(Chain->Params.Get(DTFlagParam::RangeUu), 440.0f));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Trait")));
	TestTrue(TEXT("tiers are never lost"), H.P().Keepers.Tier[Storm] >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersSecondCompat, "Dusktracer.Keepers.SecondCompat", DTWeaponTestHarness::TestFlags) // T-KEE-16
bool FDTKeepersSecondCompat::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Storm);
	H.Commit(EDTPath::Forge);
	const FDTOfferPool Pool;
	auto Legal = [&H, &Pool](FName Id)
	{
		FDTOfferCard Card;
		Card.Kind = EDTCardKind::Blessing;
		Card.ContentId = Id;
		return DTOffers::IsLegal(H.P(), Pool, Card);
	};
	TestTrue(TEXT("no leading archetype: nothing excluded"), Legal(Blessing(TEXT("Forge.Hammer"), TEXT("Key"))));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	TestFalse(TEXT("Tempest leads: Hammer (ConvertMelee) is never offered"), Legal(Blessing(TEXT("Forge.Hammer"), TEXT("Key"))));
	TestTrue(TEXT("Furnace stays legal"), Legal(Blessing(TEXT("Forge.Furnace"), TEXT("Key"))));
	H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Amp1")));
	TestEqual(TEXT("Arc Grid now leads"), H.P().Keepers.Leading[static_cast<int32>(EDTPath::Storm)], FName(TEXT("Archetype.Storm.ArcGrid")));
	TestTrue(TEXT("the owned Furnace Key stays (Field is Arc Grid's incompatibility)"), H.P().Keepers.Owned.Contains(Blessing(TEXT("Forge.Furnace"), TEXT("Key"))));
	TestTrue(TEXT("and still applies its flag"), H.Built(EDTPath::Forge, DTCat::EFlag::Field) != nullptr);
	return true;
}

// ---- enchantment and statuses ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersEnchantment, "Dusktracer.Keepers.Enchantment", DTWeaponTestHarness::TestFlags) // T-KEE-09
bool FDTKeepersEnchantment::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	// Trail tokens are the Keeper palette, never the core (R-PRE-01).
	TestTrue(TEXT("Storm trail is Pal.Keeper.Storm"), DTKeepers::TrailToken(EDTPath::Storm) == DTPal::Keeper_Storm);
	TestTrue(TEXT("Forge trail is Pal.Keeper.Forge"), DTKeepers::TrailToken(EDTPath::Forge) == DTPal::Keeper_Forge);
	TestTrue(TEXT("Tide trail is Pal.Keeper.Tide"), DTKeepers::TrailToken(EDTPath::Tide) == DTPal::Keeper_Tide);
	for (const EDTPath Path : { EDTPath::Storm, EDTPath::Forge, EDTPath::Tide })
	{
		TestFalse(TEXT("no trail is the core colour"), DTKeepers::TrailToken(Path) == DTPal::Player_Core);
	}

	auto Run = [](int32 Seed, TArray<TPair<uint8, uint16>>& OutProcs, FDTKeeperCounters& OutCounters, bool bCommit)
	{
		FDTKeeperHarness H(Seed);
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Storm, 0, false);
		H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
		if (bCommit)
		{
			H.Commit(EDTPath::Forge);
		}
		for (int32 Step = 0; Step < 100; ++Step)
		{
			H.CraftedStep(200, 10.0f);
			for (const FDTStatusApply& Apply : H.View.StatusApplies)
			{
				OutProcs.Emplace(static_cast<uint8>(Apply.Status), Apply.SourceId);
			}
			H.View.StatusApplies.Reset();
		}
		OutCounters = H.Keepers.GetCounters(0);
	};
	TArray<TPair<uint8, uint16>> A;
	TArray<TPair<uint8, uint16>> B;
	FDTKeeperCounters CA;
	FDTKeeperCounters CB;
	Run(11, A, CA, true);
	Run(11, B, CB, true);
	TestTrue(TEXT("same seed, same procs"), A == B && A.Num() > 0);
	const double Rolled = static_cast<double>(CA.HitsRolled);
	const double Primary = static_cast<double>(CA.PrimaryProcs[static_cast<int32>(EDTStatus::Shock)]) / Rolled;
	const double Second = static_cast<double>(CA.SecondProcs[static_cast<int32>(EDTStatus::Burn)]) / Rolled;
	TestTrue(FString::Printf(TEXT("primary Shock rate %.4f ~ C.ENCHANT.STATUS_CHANCE_PRIMARY"), Primary), FMath::Abs(Primary - DTConst::ENCHANT_STATUS_CHANCE_PRIMARY) < 0.01);
	TestTrue(FString::Printf(TEXT("second Burn rate %.4f ~ C.ENCHANT.STATUS_CHANCE_SECOND"), Second), FMath::Abs(Second - DTConst::ENCHANT_STATUS_CHANCE_SECOND) < 0.01);
	TArray<TPair<uint8, uint16>> C;
	FDTKeeperCounters CC;
	Run(11, C, CC, false);
	TestEqual(TEXT("no second status before the commitment"), CC.SecondProcs[static_cast<int32>(EDTStatus::Burn)], 0ll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersStatusProcs, "Dusktracer.Keepers.StatusProcs", DTWeaponTestHarness::TestFlags) // T-KEE-10
bool FDTKeepersStatusProcs::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.Patron(EDTPath::Storm, 0, false);
	H.CraftedStep(50, 10.0f);
	TestEqual(TEXT("no proc before the first blessing"), H.View.StatusApplies.Num(), 0);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	int32 Guard = 0;
	while (H.View.StatusApplies.Num() == 0 && ++Guard < 100)
	{
		H.CraftedStep(1, 10.0f);
	}
	if (!TestEqual(TEXT("one proc appends one application"), H.View.StatusApplies.Num(), 1))
	{
		return false;
	}
	const FDTStatusApply Apply = H.View.StatusApplies[0];
	TestEqual(TEXT("owner of the hit"), Apply.OwnerId, 0);
	TestEqual(TEXT("granting Keeper's source"), Apply.SourceId, DTKeepers::KeeperSource(EDTPath::Storm));
	TestTrue(TEXT("the primary status"), Apply.Status == EDTStatus::Shock);
	TestTrue(TEXT("not applied in Effects"), (H.Enemies.GetStatusMask(0) & (1u << static_cast<int32>(EDTStatus::Shock))) == 0);
	TestTrue(TEXT("applied by enemies next step with C.STATUS.* behaviour"), H.Enemies.ApplyStatus(Apply) &&
		(H.Enemies.GetStatusMask(0) & (1u << static_cast<int32>(EDTStatus::Shock))) != 0 &&
		FMath::IsNearlyEqual(H.Enemies.GetAux(0).ShockLeft, DTConst::STATUS_SHOCK_S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersBurnMagnitude, "Dusktracer.Keepers.BurnMagnitude", DTWeaponTestHarness::TestFlags) // T-KEE-11
bool FDTKeepersBurnMagnitude::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.Patron(EDTPath::Forge, 0, false);
	H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")));
	auto FirstBurn = [&H]()
	{
		for (int32 Guard = 0; Guard < 200; ++Guard)
		{
			H.CraftedStep(1, 40.0f);
			if (H.View.StatusApplies.Num() > 0)
			{
				const FDTStatusApply Apply = H.View.StatusApplies[0];
				H.View.StatusApplies.Reset();
				return Apply;
			}
		}
		return FDTStatusApply();
	};
	const FDTStatusApply Plain = FirstBurn();
	TestTrue(TEXT("Burn"), Plain.Status == EDTStatus::Burn);
	TestEqual(TEXT("magnitude = C.STATUS.BURN_DPS_RATIO x the hit's damage"), Plain.Magnitude, DTConst::STATUS_BURN_DPS_RATIO * 40.0f, 1e-4f);
	TestEqual(TEXT("no stack bonus without Furnace Amp2"), Plain.StackBonus, 0);
	H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Amp2")));
	const FDTStatusApply Banked = FirstBurn();
	TestEqual(TEXT("burn_stacks_max travels in StackBonus"), Banked.StackBonus, 1);
	H.Enemies.ApplyStatus(Banked);
	TestEqual(TEXT("the stack cap rises"), H.Enemies.GetAux(0).BurnCap, DTConst::STATUS_BURN_STACKS_MAX + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersFrostThreshold, "Dusktracer.Keepers.FrostThreshold", DTWeaponTestHarness::TestFlags) // T-KEE-12
bool FDTKeepersFrostThreshold::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	auto FirstChill = [](FDTKeeperHarness& H)
	{
		for (int32 Guard = 0; Guard < 200; ++Guard)
		{
			H.CraftedStep(1, 10.0f);
			if (H.View.StatusApplies.Num() > 0)
			{
				const FDTStatusApply Apply = H.View.StatusApplies[0];
				H.View.StatusApplies.Reset();
				return Apply;
			}
		}
		return FDTStatusApply();
	};
	{
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Tide, 0, false);
		H.Bless(Blessing(TEXT("Tide.Frost"), TEXT("Key")));
		const FDTStatusApply Chill = FirstChill(H);
		TestTrue(TEXT("Chill"), Chill.Status == EDTStatus::Chill);
		TestEqual(TEXT("Frost's freeze_stacks travels in FreezeAt"), Chill.FreezeAt, 2);
		H.Enemies.ApplyStatus(Chill);
		TestFalse(TEXT("one stack: not frozen"), H.Enemies.GetAux(0).IsFrozen());
		H.Enemies.ApplyStatus(Chill);
		TestTrue(TEXT("frozen at Frost's lower threshold"), H.Enemies.GetAux(0).IsFrozen());
	}
	{
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Tide, 0, false);
		H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
		const FDTStatusApply Chill = FirstChill(H);
		TestEqual(TEXT("without Frost: the default threshold"), Chill.FreezeAt, 0);
		TestEqual(TEXT("without Frost: the default length"), Chill.FreezeScale, 1.0f);
		for (int32 Stack = 1; Stack < DTConst::STATUS_FREEZE_STACKS; ++Stack)
		{
			H.Enemies.ApplyStatus(Chill);
		}
		TestFalse(TEXT("below C.STATUS.FREEZE_STACKS: not frozen"), H.Enemies.GetAux(0).IsFrozen());
		H.Enemies.ApplyStatus(Chill);
		TestTrue(TEXT("frozen at C.STATUS.FREEZE_STACKS"), H.Enemies.GetAux(0).IsFrozen());
	}
	return true;
}

// ---- sources ----------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersSources, "Dusktracer.Keepers.Sources", DTWeaponTestHarness::TestFlags) // T-KEE-13
bool FDTKeepersSources::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	const uint16 Storm = DTKeepers::KeeperSource(EDTPath::Storm);
	const uint16 Forge = DTKeepers::KeeperSource(EDTPath::Forge);
	const uint16 Rifle = FDTTelemetrySources::RegisterWeapon(PulseRifle);
	{
		// Cleaves (D-015), children, chains, echoes, field ticks: each carries Keeper.<Path>.
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Forge);
		H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
		H.StepMany(120);
		TestTrue(TEXT("a Keeper cleave is Keeper.Forge damage"), H.Damage(Forge) > 0.0);
		TestEqual(TEXT("no weapon-sourced cleave"), H.Damage(Rifle), 0.0);
	}
	for (const TCHAR* Archetype : { TEXT("Storm.Tempest"), TEXT("Storm.ArcGrid"), TEXT("Storm.Downpour") })
	{
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Storm);
		H.Bless(Blessing(Archetype, TEXT("Key")));
		H.StepMany(FireSteps);
		TestTrue(FString::Printf(TEXT("%s: Keeper hits carry Keeper.Storm"), Archetype), H.Damage(Storm) > 0.0);
	}
	{
		// Status damage: a Keeper Burn application names the Keeper, and enemies tick it under that source (R-ENM-22).
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Forge);
		H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")));
		H.StepMany(FireSteps);
		TestTrue(TEXT("Burn applications carry Keeper.Forge"), H.CountStatuses(EDTStatus::Burn, Forge) > 0);
		TestTrue(TEXT("field ticks carry Keeper.Forge"), H.Damage(Forge) > 0.0);
	}
	{
		// Intercession: the base's hits carry the primary, the layer's the second.
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Storm);
		H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
		H.Commit(EDTPath::Forge);
		H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Key"))); // no dash, no melee, no hit taken: the Key alone never slams here
		H.StepMany(30);
		TestEqual(TEXT("no Forge damage before the cast"), H.Damage(Forge), 0.0);
		H.Keepers.QueueIntercession(0);
		H.StepMany(2);
		TestEqual(TEXT("the layer is the second's leader"), H.Keepers.GetActivePlan(0).Layer, Row(TEXT("Forge.Slam")));
		TestTrue(TEXT("the layer's CastStart slam carries the second's Keeper.Forge"), H.Damage(Forge) > 0.0);
		H.StepMany(60);
		TestTrue(TEXT("the base's split children carry the primary's Keeper.Storm"), H.Damage(Storm) > 0.0);
	}
	return true;
}

// ---- Intercession -----------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersIntercessionCompose, "Dusktracer.Keepers.IntercessionCompose", DTWeaponTestHarness::TestFlags) // T-KEE-14
bool FDTKeepersIntercessionCompose::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	{
		FDTKeeperRunState K;
		K.Primary = EDTPath::Storm;
		FDTIntercessionPlan Plan = DTKeepers::ComposeIntercession(K);
		TestTrue(TEXT("no Key: base only"), Plan.Base == EDTPath::Storm && Plan.Variant == INDEX_NONE && Plan.Layer == INDEX_NONE);
		DTOffers::AddBlessing(K, Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
		K.Second = EDTPath::Forge;
		DTOffers::AddBlessing(K, Blessing(TEXT("Forge.Slam"), TEXT("Key")));
		Plan = DTKeepers::ComposeIntercession(K);
		TestEqual(TEXT("variant = primary's leader"), Plan.Variant, Row(TEXT("Storm.Tempest")));
		TestEqual(TEXT("no layer before the commitment"), Plan.Layer, INDEX_NONE);
		K.bCommitDone = true;
		Plan = DTKeepers::ComposeIntercession(K);
		TestEqual(TEXT("layer = second's leader from the commitment"), Plan.Layer, Row(TEXT("Forge.Slam")));
	}
	{
		// A layer naming the variant's flag keeps its status, never its shape: Frost's screen Freeze wins over Arc Grid's StatusApply.
		FDTKeeperRunState K;
		K.Primary = EDTPath::Tide;
		DTOffers::AddBlessing(K, Blessing(TEXT("Tide.Frost"), TEXT("Key")));
		K.Second = EDTPath::Storm;
		K.bCommitDone = true;
		DTOffers::AddBlessing(K, Blessing(TEXT("Storm.ArcGrid"), TEXT("Key")));
		const FDTIntercessionPlan Plan = DTKeepers::ComposeIntercession(K);
		const FDTSpecClause* Variant = Plan.VariantSpec.Clauses.FindByPredicate([](const FDTSpecClause& C) { return C.Flag == DTCat::EFlag::StatusApply; });
		const FDTSpecClause* Layer = Plan.LayerSpec.Clauses.FindByPredicate([](const FDTSpecClause& C) { return C.Flag == DTCat::EFlag::StatusApply; });
		TestTrue(TEXT("the variant keeps its shape (scope=Screen)"), Variant && Variant->GetWord(FName(TEXT("scope"))) == FName(TEXT("Screen")));
		TestTrue(TEXT("the layer keeps its status for the variant's hits"), Layer && Layer->GetWord(FName(TEXT("status"))) == FName(TEXT("Status.Shock")));
		for (const FDTSpecClause& C : Plan.LayerSpec.Clauses)
		{
			if (C.Flag == DTCat::EFlag::StatusApply)
			{
				for (const TPair<FName, FName>& Word : C.Words)
				{
					TestFalse(TEXT("no shape word left on a conflicting layer clause"), DTKeepers::IsShapeWord(Word.Key));
				}
			}
		}
	}
	{
		// Storm cast: every round splits (Tempest's variant shape), then the flags go back.
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Storm);
		H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
		H.StepOnce();
		H.Keepers.QueueIntercession(0);
		H.StepOnce();
		TestTrue(TEXT("casting"), H.Keepers.IsCasting(0));
		const FDTFlagEntry* Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
		TestTrue(TEXT("variant: children 5, every round"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Children), 5.0f) &&
			FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), 1.0f));
		TestTrue(TEXT("each child chains once"), H.Built(EDTPath::Storm, DTCat::EFlag::Chain) != nullptr);
		H.StepMany(FMath::CeilToInt(DTConst::INTERCESSION_STORM_DURATION_S / Dt) + 1);
		TestFalse(TEXT("the window ends"), H.Keepers.IsCasting(0));
		Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
		TestTrue(TEXT("the Key's own Split returns"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Children), 3.0f));
		TestTrue(TEXT("no chain after the window"), H.Built(EDTPath::Storm, DTCat::EFlag::Chain) == nullptr);
		TestEqual(TEXT("one cast recorded"), H.Keepers.TakeFinishedCasts().Num(), 1);
	}
	{
		// Forge cast: fire rings knock back and Burn.
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Forge);
		H.StepOnce();
		H.Keepers.QueueIntercession(0);
		H.StepMany(3);
		TestTrue(TEXT("Forge rings hit as Keeper.Forge"), H.Damage(DTKeepers::KeeperSource(EDTPath::Forge)) > 0.0);
		TestTrue(TEXT("Forge rings Burn"), H.CountStatuses(EDTStatus::Burn, DTKeepers::KeeperSource(EDTPath::Forge)) > 0);
		TestTrue(TEXT("Forge rings knock back"), H.MoveLog.Num() > 0);
	}
	{
		// Tide cast: everyone in range pulled into a line ahead and Frozen.
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(-600.0f, 300.0f));
		H.AddEnemy(FVector2f(0.0f, -900.0f));
		H.AddEnemy(FVector2f(1500.0f, 200.0f));
		H.Patron(EDTPath::Tide, 0, false);
		H.StepOnce();
		H.Keepers.QueueIntercession(0);
		H.StepMany(2);
		TestEqual(TEXT("three pulled"), H.MoveLog.Num(), 3);
		TestEqual(TEXT("three frozen"), H.FrozenEnemies(), 3);
		for (int32 Index = 0; Index < H.Enemies.Num(); ++Index)
		{
			const FVector2f P = H.Enemies.GetPositions()[Index];
			TestTrue(TEXT("ahead of the hero (aim +X)"), P.X > 0.0f);
		}
	}
	return true;
}

// ---- per player -----------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersPerPlayer, "Dusktracer.Keepers.PerPlayer", DTWeaponTestHarness::TestFlags) // T-KEE-19
bool FDTKeepersPerPlayer::RunTest(const FString& Parameters)
{
	using namespace DTKeeperTest;
	auto Play = [](int32 Seed, TArray<FDTStatusApply>& Out)
	{
		FDTKeeperHarness H(Seed, 2);
		H.AddBlock();
		H.Patron(EDTPath::Storm, 0);
		H.Patron(EDTPath::Forge, 1);
		H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")), 0);
		H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")), 1);
		H.StepMany(FireSteps);
		Out = H.StatusLog;
		return H.Weapons.GetPlayerFlags(1).ContainsByPredicate([](const FDTFlagEntry& E) { return E.Flag == DTCat::EFlag::Split; }) ||
			H.Weapons.GetPlayerFlags(0).ContainsByPredicate([](const FDTFlagEntry& E) { return E.Flag == DTCat::EFlag::Field; });
	};
	TArray<FDTStatusApply> A;
	TArray<FDTStatusApply> B;
	TestFalse(TEXT("one player's Keepers never reach the other's weapons"), Play(5, A));
	Play(5, B);
	bool bSame = A.Num() == B.Num() && A.Num() > 0;
	for (int32 Index = 0; bSame && Index < A.Num(); ++Index)
	{
		bSame = A[Index].Status == B[Index].Status && A[Index].OwnerId == B[Index].OwnerId && A[Index].Handle.Serial == B[Index].Handle.Serial;
	}
	TestTrue(TEXT("same seed and inputs, same procs"), bSame);
	int32 Wrong = 0;
	for (const FDTStatusApply& Apply : A)
	{
		Wrong += Apply.SourceId != DTKeepers::KeeperSource(Apply.OwnerId == 0 ? EDTPath::Storm : EDTPath::Forge) ? 1 : 0;
	}
	TestEqual(TEXT("every status names its owner's Keeper, never the other player's"), Wrong, 0);
	return true;
}

// ---- one behaviour test per archetype Key on a Pulse Rifle -----------------------------------------------------------------------

#define DT_KEEPER_KEY_TEST(Class, Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Dusktracer.Keepers.Key." Name, DTWeaponTestHarness::TestFlags) \
	bool Class::RunTest(const FString& Parameters)

DT_KEEPER_KEY_TEST(FDTKeepersKeyTempest, "Tempest")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness Base;
	Base.AddBlock();
	Base.Patron(EDTPath::Storm);
	Base.StepMany(FireSteps);
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Storm);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	H.StepMany(FireSteps);
	TestEqual(TEXT("without the Key no round splits"), Base.Weapons.GetMaxGeneration(), 0);
	TestTrue(TEXT("with it rounds split into children"), H.Weapons.GetMaxGeneration() >= 1 && H.Bullets.GetCounters().Spawned > Base.Bullets.GetCounters().Spawned);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Storm)), H.KeeperShare(EDTPath::Storm) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyArcGrid, "ArcGrid")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Storm);
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Key")));
	H.StepMany(FireSteps);
	// Behaviour: hits jump from the struck enemy to others (hits with no round of their own).
	int32 Jumps = 0;
	for (const FDTHit& Hit : H.HitLog)
	{
		Jumps += Hit.SourceId == DTKeepers::KeeperSource(EDTPath::Storm) && Hit.WeaponSlot == 0 ? 1 : 0;
	}
	TestTrue(TEXT("hits chain to further enemies"), Jumps > 0);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Storm)), H.KeeperShare(EDTPath::Storm) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyDownpour, "Downpour")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Storm);
	H.Bless(Blessing(TEXT("Storm.Downpour"), TEXT("Key")));
	H.StepMany(FireSteps);
	TestTrue(TEXT("strikes fall on impact points"), H.Damage(DTKeepers::KeeperSource(EDTPath::Storm)) > 0.0);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Storm)), H.KeeperShare(EDTPath::Storm) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyHammer, "Hammer")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
	H.StepMany(FireSteps);
	TestEqual(TEXT("the rifle's rounds become cleaves: none fly"), H.Bullets.GetCounters().Spawned, 0ll);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Forge)), H.KeeperShare(EDTPath::Forge) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyFurnace, "Furnace")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")));
	int32 MaxFields = 0;
	for (int32 Step = 0; Step < FireSteps; ++Step)
	{
		H.StepOnce();
		MaxFields = FMath::Max(MaxFields, H.Weapons.GetLiveFields());
	}
	TestTrue(TEXT("impacts leave burning ground"), MaxFields > 0);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Forge)), H.KeeperShare(EDTPath::Forge) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeySlam, "Slam")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Key")));
	for (int32 Step = 0; Step < FireSteps; ++Step)
	{
		H.View.HeroIntangible[0] = Step % 60 < 10; // a dash every second, landing on its tenth step
		H.StepOnce();
	}
	int32 Ring = 0;
	for (const FDTHit& Hit : H.HitLog)
	{
		Ring += Hit.SourceId == DTKeepers::KeeperSource(EDTPath::Forge) ? 1 : 0;
	}
	TestTrue(TEXT("dash landings slam the ground around the hero"), Ring > 0);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Forge)), H.KeeperShare(EDTPath::Forge) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyWhirlpool, "Whirlpool")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Tide);
	H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
	H.StepMany(FireSteps);
	TestTrue(TEXT("impacts pull enemies"), H.MoveLog.Num() > 0);
	TestTrue(TEXT("pulled enemies are Chilled by Keeper.Tide"), H.CountStatuses(EDTStatus::Chill, DTKeepers::KeeperSource(EDTPath::Tide)) > 0);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0 through crush_dmg"), H.KeeperShare(EDTPath::Tide)), H.KeeperShare(EDTPath::Tide) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyFrost, "Frost")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Tide);
	H.Bless(Blessing(TEXT("Tide.Frost"), TEXT("Key")));
	H.StepMany(FireSteps);
	TestTrue(TEXT("hits Chill until enemies Freeze"), H.FrozenEnemies() > 0);
	TestTrue(TEXT("Frozen enemies take frozen_vuln as Keeper damage"), H.Keepers.GetCounters(0).FrozenVulnHits > 0);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Tide)), H.KeeperShare(EDTPath::Tide) > 0.0);
	return true;
}

DT_KEEPER_KEY_TEST(FDTKeepersKeyStillness, "Stillness")
{
	using namespace DTKeeperTest;
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Tide);
	H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
	H.StepMany(FireSteps); // the hero never moves: it charges
	const float Plain = DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1)->Damage;
	float Strongest = 0.0f;
	for (const FDTHit& Hit : H.HitLog)
	{
		Strongest = Hit.SourceId == DTKeepers::KeeperSource(EDTPath::Tide) ? FMath::Max(Strongest, Hit.Damage) : Strongest;
	}
	const float Mult = DTKeepers::PathParam(EDTPath::Tide, DTCat::EFlag::Stillness, DTFlagParam::Mult, 0.0f);
	TestTrue(TEXT("standing still charges a volley: x mult damage"), Strongest >= Mult * Plain - KINDA_SMALL_NUMBER);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Tide)), H.KeeperShare(EDTPath::Tide) > 0.0);
	return true;
}

#undef DT_KEEPER_KEY_TEST

// ---- T-KEE-21: Keeper parameters the weapon and Keeper layers express (keepers.md R-KEE-13, weapons.md) ------------------------------------------

namespace DTKeeperParamTest
{
	using namespace DTKeeperTest;

	static const FName TestSource(TEXT("Test.Params"));

	/** One Keeper-sourced flag handed straight to the seam (the blessing path is covered by the Key tests). */
	static FDTFlagEntry& AddFlag(FDTWeaponFlags& Flags, DTCat::EFlag Flag, EDTPath Path)
	{
		return Flags.Add(Flag, DTKeepers::KeeperSource(Path), EDTFlagTier::Key);
	}

	static void Burn(FDTKeeperHarness& H, int32 Enemy)
	{
		FDTStatusApply Apply;
		Apply.Handle = H.Enemies.GetHandle(Enemy);
		Apply.Status = EDTStatus::Burn;
		Apply.Magnitude = 1.0f;
		Apply.OwnerId = 0;
		H.Enemies.ApplyStatus(Apply);
	}

	static void Shock(FDTKeeperHarness& H, int32 Enemy)
	{
		FDTStatusApply Apply;
		Apply.Handle = H.Enemies.GetHandle(Enemy);
		Apply.Status = EDTStatus::Shock;
		Apply.OwnerId = 0;
		H.Enemies.ApplyStatus(Apply);
	}

	static int32 HitsOn(const FDTKeeperHarness& H, uint32 Serial, uint16 Source)
	{
		int32 Count = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Count += Hit.EnemySerial == Serial && Hit.SourceId == Source ? 1 : 0;
		}
		return Count;
	}

	static float BiggestHit(const FDTKeeperHarness& H, uint16 Source)
	{
		float Best = 0.0f;
		for (const FDTHit& Hit : H.HitLog)
		{
			Best = Hit.SourceId == Source ? FMath::Max(Best, Hit.Damage) : Best;
		}
		return Best;
	}
}

#define DT_KEEPER_PARAM_TEST(Class, Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Dusktracer.Keepers.Param." Name, DTWeaponTestHarness::TestFlags) \
	bool Class::RunTest(const FString& Parameters)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponArcPlate, "Dusktracer.Weapons.ArcContactPoint", DTWeaponTestHarness::TestFlags) // R-ENM-10
bool FDTWeaponArcPlate::RunTest(const FString& Parameters)
{
	using namespace DTKeeperParamTest;
	FDTKeeperHarness H;
	const int32 Tank = H.Enemies.Spawn(EDTEnemyRole::Tank, FVector2f(250.0f, 0.0f));
	H.Enemies.SetHp(Tank, 1.0e5f); // small enough that float HP registers each cleave
	H.Enemies.SetFacing(Tank, FVector2f(-1.0f, 0.0f));
	H.Patron(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
	const float Before = H.Enemies.GetHp()[Tank];
	H.StepMany(120);
	if (TestTrue(TEXT("the cleave hits"), H.HitLog.Num() > 0))
	{
		const FDTHit& Hit = H.HitLog[0];
		const float Radius = H.Enemies.GetRadii()[Tank];
		TestEqual(TEXT("a sweep's hit sits on the rim facing the hero"), Hit.Position.X, 250.0f - Radius, 0.01f);
		double Taken = 0.0;
		for (const FDTHit& One : H.HitLog)
		{
			Taken += One.Damage * DTConst::ROLE_TANK_FRONT_DAMAGE_MULT;
		}
		TestEqual(TEXT("the plate halves a cleave to the Tank's face"), static_cast<double>(Before - H.Enemies.GetHp()[Tank]), Taken, 0.01 * Taken);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamCrush, "Crush")
{
	using namespace DTKeeperParamTest;
	const uint16 Tide = DTKeepers::KeeperSource(EDTPath::Tide);
	FDTKeeperHarness H;
	H.AddBlock();
	H.Patron(EDTPath::Tide);
	H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
	H.StepMany(FireSteps);
	TestTrue(TEXT("the Key's pulled enemies collide and take Keeper damage"), H.Damage(Tide) > 0.0);
	int32 WithSlot = 0;
	for (const FDTHit& Hit : H.HitLog)
	{
		WithSlot += Hit.SourceId == Tide && Hit.WeaponSlot != INDEX_NONE ? 1 : 0;
	}
	TestEqual(TEXT("crush is a Keeper-only hit: no weapon slot, no proc feeds on it"), WithSlot, 0);

	// Apart: a single pulled enemy touches nothing, so nothing is crushed.
	FDTKeeperHarness Lone;
	Lone.AddEnemy(FVector2f(300.0f, 0.0f));
	Lone.Patron(EDTPath::Tide);
	Lone.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
	Lone.StepMany(FireSteps);
	TestEqual(TEXT("no collision, no crush"), Lone.Damage(Tide), 0.0);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamPulledVuln, "PulledVuln")
{
	using namespace DTKeeperParamTest;
	const uint16 Tide = DTKeepers::KeeperSource(EDTPath::Tide);
	FDTKeeperHarness H;
	const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.Patron(EDTPath::Tide);
	FDTWeaponFlags Flags;
	FDTFlagEntry& Pull = AddFlag(Flags, DTCat::EFlag::Pull, EDTPath::Tide);
	Pull.Params.Set(DTFlagParam::DistanceUu, 1.0f);
	Pull.Params.Set(DTFlagParam::RadiusUu, 200.0f);
	Pull.Params.Set(DTFlagParam::PulledVuln, 0.5f);
	Pull.Params.Set(DTFlagParam::PulledVulnSeconds, 2.0f);
	H.Weapons.AddFlagSource(0, TestSource, Flags);
	H.StepMany(FireSteps);
	const uint32 Serial = H.Enemies.GetSerials()[Enemy];
	const int32 Extra = HitsOn(H, Serial, Tide);
	TestTrue(TEXT("a pulled enemy takes extra Keeper hits"), Extra > 0);
	const float Plain = DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1)->Damage;
	TestTrue(FString::Printf(TEXT("the extra is pulled_vuln of the hit (%.2f)"), BiggestHit(H, Tide)), FMath::IsNearlyEqual(BiggestHit(H, Tide), 0.5f * Plain, 0.01f * Plain));
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamArcs, "Arcs")
{
	using namespace DTKeeperParamTest;
	const uint16 Storm = DTKeepers::KeeperSource(EDTPath::Storm);
	const auto Run = [&Storm](float VsShocked, float Return, bool bShock, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		const int32 Second = H.AddEnemy(FVector2f(380.0f, 0.0f));
		H.Patron(EDTPath::Storm);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Chain = AddFlag(Flags, DTCat::EFlag::Chain, EDTPath::Storm);
		Chain.Params.Set(DTFlagParam::Jumps, 1.0f);
		Chain.Params.Set(DTFlagParam::RangeUu, 350.0f);
		Chain.Params.Set(DTFlagParam::Dmg, 1.0f);
		Chain.Params.Set(DTFlagParam::DmgVsShocked, VsShocked);
		Chain.Params.Set(DTFlagParam::ReturnSameTarget, Return);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		if (bShock)
		{
			Shock(H, Second);
		}
		H.StepMany(120);
	};
	FDTKeeperHarness Plain;
	Run(0.5f, 0.0f, false, Plain);
	FDTKeeperHarness Shocked;
	Run(0.5f, 0.0f, true, Shocked);
	TestTrue(TEXT("dmg_vs_shocked raises an arc on a Shocked enemy"), FMath::IsNearlyEqual(BiggestHit(Shocked, Storm), 1.5f * BiggestHit(Plain, Storm), 0.01f));
	TestEqual(TEXT("without return_same_target the first enemy takes no arc"), HitsOn(Plain, Plain.Enemies.GetSerials()[0], Storm), 0);
	FDTKeeperHarness Back;
	Run(0.0f, 1.0f, false, Back);
	TestTrue(TEXT("return_same_target: the arc leaps back onto the enemy it left"), HitsOn(Back, Back.Enemies.GetSerials()[0], Storm) > 0);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamForkOnKill, "ForkOnKill")
{
	using namespace DTKeeperParamTest;
	const uint16 Storm = DTKeepers::KeeperSource(EDTPath::Storm);
	const auto Run = [](float Forks, FDTKeeperHarness& H, TArray<uint32>& OutBystanders)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.AddEnemy(FVector2f(380.0f, 0.0f), 1.0f); // dies to the arc
		const int32 A = H.AddEnemy(FVector2f(480.0f, 40.0f)); // beyond a jump from the struck enemy, within one of the victim
		const int32 B = H.AddEnemy(FVector2f(480.0f, -40.0f));
		OutBystanders = { H.Enemies.GetSerials()[A], H.Enemies.GetSerials()[B] };
		H.Patron(EDTPath::Storm);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Chain = AddFlag(Flags, DTCat::EFlag::Chain, EDTPath::Storm);
		Chain.Params.Set(DTFlagParam::Jumps, 1.0f);
		Chain.Params.Set(DTFlagParam::RangeUu, 100.0f + 50.0f);
		Chain.Params.Set(DTFlagParam::Dmg, 1.0f);
		Chain.Params.Set(DTFlagParam::ForkOnKill, Forks);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		H.StepMany(120);
	};
	FDTKeeperHarness Plain;
	TArray<uint32> Bystanders;
	Run(0.0f, Plain, Bystanders);
	TestEqual(TEXT("no fork: the bystanders take no arc"), HitsOn(Plain, Bystanders[0], Storm) + HitsOn(Plain, Bystanders[1], Storm), 0);
	FDTKeeperHarness Forked;
	Run(3.0f, Forked, Bystanders); // the nearest fork lands on the first enemy, the next two on the bystanders
	TestTrue(TEXT("a killing arc forks to both bystanders"), HitsOn(Forked, Bystanders[0], Storm) > 0 && HitsOn(Forked, Bystanders[1], Storm) > 0);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamRetarget, "Retarget")
{
	using namespace DTKeeperParamTest;
	const uint16 Storm = DTKeepers::KeeperSource(EDTPath::Storm);
	const auto Run = [](bool bRetarget, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		FDTEnemySpawn Elite;
		Elite.Role = EDTEnemyRole::Swarm;
		Elite.Position = FVector2f(350.0f, -420.0f); // behind the fan of a straight-on hit
		Elite.bElite = true;
		const int32 Index = H.Enemies.Spawn(Elite, nullptr);
		H.Enemies.SetHp(Index, ToughHp);
		const uint32 Serial = H.Enemies.GetSerials()[Index];
		H.Patron(EDTPath::Storm);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Split = AddFlag(Flags, DTCat::EFlag::Split, EDTPath::Storm);
		Split.Params.Set(DTFlagParam::Children, 1.0f);
		Split.Params.Set(DTFlagParam::ChildDmg, 1.0f);
		Split.Params.Set(DTFlagParam::Gen, 1.0f);
		Split.Params.Set(DTFlagParam::FanDeg, 0.0f);
		if (bRetarget)
		{
			Split.Params.SetWord(DTFlagParam::Retarget, FName(TEXT("EliteOrBoss")));
		}
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		H.StepMany(FireSteps);
		return Serial;
	};
	FDTKeeperHarness Plain;
	const uint32 PlainElite = Run(false, Plain);
	TestEqual(TEXT("straight children miss the elite"), HitsOn(Plain, PlainElite, Storm), 0);
	FDTKeeperHarness Seek;
	const uint32 SeekElite = Run(true, Seek);
	TestTrue(TEXT("retarget=EliteOrBoss: children fly at the elite"), HitsOn(Seek, SeekElite, Storm) > 0);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamHitOnBurning, "HitOnBurning")
{
	using namespace DTKeeperParamTest;
	const auto Fields = [](bool bBurning)
	{
		FDTKeeperHarness H;
		const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Forge);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Field = AddFlag(Flags, DTCat::EFlag::Field, EDTPath::Forge);
		Field.Params.SetWord(DTFlagParam::Trigger, FName(TEXT("HitOnBurning")));
		Field.Params.Set(DTFlagParam::RadiusUu, 120.0f);
		Field.Params.Set(DTFlagParam::DurationS, 2.0f);
		Field.Params.Set(DTFlagParam::Dps, 0.3f);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		if (bBurning)
		{
			Burn(H, Enemy);
		}
		int32 Most = 0;
		for (int32 Step = 0; Step < 120; ++Step)
		{
			H.StepOnce();
			Most = FMath::Max(Most, H.Weapons.GetLiveFields());
		}
		return Most;
	};
	TestEqual(TEXT("hitting an enemy that does not Burn leaves nothing"), Fields(false), 0);
	TestTrue(TEXT("hitting a Burning enemy leaves a field"), Fields(true) > 0);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamBurningDeath, "BurningDeath")
{
	using namespace DTKeeperParamTest;
	const uint16 Forge = DTKeepers::KeeperSource(EDTPath::Forge);
	const auto Run = [](DTCat::EFlag Flag, bool bBurning, bool bExplode, bool bSpread, FDTKeeperHarness& H)
	{
		const int32 Victim = H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
		const int32 Near = H.AddEnemy(FVector2f(420.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Near];
		H.Patron(EDTPath::Forge);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Entry = AddFlag(Flags, Flag, EDTPath::Forge);
		if (bExplode)
		{
			Entry.Params.Set(DTFlagParam::BurningExplodeOnDeathUu, 200.0f);
			Entry.Params.Set(DTFlagParam::ExplodeDmg, 0.5f);
		}
		if (bSpread)
		{
			Entry.Params.Set(DTFlagParam::SpreadBurnOnKillUu, 200.0f);
			Entry.Params.Set(DTFlagParam::SpreadBurnIntervalS, 1.0f);
		}
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		if (bBurning)
		{
			Burn(H, Victim);
		}
		H.StepMany(60);
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Near = Run(DTCat::EFlag::Field, true, true, false, H);
		TestTrue(TEXT("a Burning enemy explodes on death (Furnace: any death)"), HitsOn(H, Near, Forge) > 0);
		TestTrue(TEXT("the explosion Burns"), H.CountStatuses(EDTStatus::Burn, Forge) > 0);
	}
	{
		FDTKeeperHarness H;
		const uint32 Near = Run(DTCat::EFlag::Field, false, true, false, H);
		TestEqual(TEXT("no Burn, no explosion"), HitsOn(H, Near, Forge), 0);
	}
	{
		// Slam's line asks for a slam's kill; a rifle kill does not count.
		FDTKeeperHarness H;
		const uint32 Near = Run(DTCat::EFlag::Slam, true, true, false, H);
		TestEqual(TEXT("a Slam entry explodes only on a slam's kill"), HitsOn(H, Near, Forge), 0);
	}
	{
		FDTKeeperHarness H;
		const uint32 Near = Run(DTCat::EFlag::Field, true, false, true, H);
		TestEqual(TEXT("spread deals no hit of its own"), HitsOn(H, Near, Forge), 0);
		TestTrue(TEXT("a Burning death spreads Burn"), H.CountStatuses(EDTStatus::Burn, Forge) > 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamDashReset, "DashCooldownReset")
{
	using namespace DTKeeperParamTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Key")));
	H.Keepers.QueueIntercession(0);
	H.StepOnce();
	TestTrue(TEXT("the cast runs"), H.Keepers.IsCasting(0));
	TestEqual(TEXT("no dash, no reset"), H.Keepers.TakeDashResets().Num(), 0);
	H.View.HeroIntangible[0] = true;
	H.StepOnce();
	H.View.HeroIntangible[0] = false;
	H.StepOnce();
	TestEqual(TEXT("a dash landing during the cast resets the cooldown"), H.Keepers.TakeDashResets().Num(), 1);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTKeepersKeywords, "Dusktracer.Keepers.Keywords", DTWeaponTestHarness::TestFlags) // T-KEE-22
bool FDTKeepersKeywords::RunTest(const FString& Parameters)
{
	FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(FName(TEXT("ST_Keywords")));
	if (!TestTrue(TEXT("ST_Keywords is registered"), Table.IsValid()))
	{
		return true;
	}
	TSet<FString> Ids = { TEXT("Status.Shock"), TEXT("Status.Burn"), TEXT("Status.Chill"), TEXT("Status.Freeze"), TEXT("Flag.Ricochet") };
	for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
	{
		Ids.Add(DTCat::FlagIds[static_cast<int32>(DTCat::Archetypes[Row].KeyFlag)]);
	}
	for (const FString& Id : Ids)
	{
		for (const TCHAR* Part : { TEXT(".Name"), TEXT(".Desc") })
		{
			FString Text;
			TestTrue(FString::Printf(TEXT("%s%s exists"), *Id, Part), Table->GetSourceString(Id + Part, Text) && !Text.IsEmpty());
		}
	}
	return true;
}

// ---- Tide's cards, each param against its absence (keepers.md §2, T-KEE-21) ------------------------------------------------------

namespace DTKeeperExpressTest
{
	using namespace DTKeeperParamTest;

	static const FName WordChill(TEXT("Status.Chill"));

	static void FreezeEnemy(FDTKeeperHarness& H, int32 Enemy, float Seconds = DTConst::STATUS_FREEZE_S)
	{
		FDTStatusApply Apply;
		Apply.Handle = H.Enemies.GetHandle(Enemy);
		Apply.Status = EDTStatus::Freeze;
		Apply.OwnerId = 0;
		Apply.SourceId = DTKeepers::KeeperSource(EDTPath::Tide);
		Apply.FreezeScale = Seconds / DTConst::STATUS_FREEZE_S;
		H.Enemies.ApplyStatus(Apply);
	}

	/** A Frost entry whose own proc never rolls, so only the param under test can act. */
	static FDTFlagEntry& FrostEntry(FDTWeaponFlags& Flags)
	{
		FDTFlagEntry& Entry = AddFlag(Flags, DTCat::EFlag::StatusApply, EDTPath::Tide);
		Entry.Params.SetWord(DTFlagParam::Status, WordChill);
		Entry.Params.Set(DTFlagParam::Chance, 0.0f);
		return Entry;
	}

	static int32 StatusesOn(const FDTKeeperHarness& H, uint32 Serial, EDTStatus Status)
	{
		int32 Count = 0;
		for (const FDTStatusApply& Apply : H.StatusLog)
		{
			Count += Apply.Handle.Serial == Serial && Apply.Status == Status ? 1 : 0;
		}
		return Count;
	}

	static uint32 SerialOf(const FDTKeeperHarness& H, int32 Enemy)
	{
		return H.Enemies.GetSerials()[Enemy];
	}

	static float PlainDamage()
	{
		return DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1)->Damage;
	}
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamFrostShatter, "FrostShatter")
{
	using namespace DTKeeperExpressTest;
	const uint16 Tide = DTKeepers::KeeperSource(EDTPath::Tide);
	const auto Run = [](bool bFrozen, FDTKeeperHarness& H)
	{
		const int32 Victim = H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
		const int32 Near = H.AddEnemy(FVector2f(400.0f, 0.0f));
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Frost = FrostEntry(Flags);
		Frost.Params.Set(DTFlagParam::ShatterOnFrozenKillUu, 200.0f);
		Frost.Params.Set(DTFlagParam::ShatterDmg, 0.5f);
		Frost.Params.SetWord(DTFlagParam::ShatterStatus, WordChill);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		if (bFrozen)
		{
			FreezeEnemy(H, Victim);
		}
		const uint32 Serial = H.Enemies.GetSerials()[Near];
		H.StepMany(30);
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Near = Run(true, H);
		TestEqual(TEXT("the Frozen death bursts once on its neighbour"), HitsOn(H, Near, Tide), 1);
		TestEqual(TEXT("for shatter_dmg of the strongest weapon"), BiggestHit(H, Tide), 0.5f * PlainDamage(), 0.01f);
		TestEqual(TEXT("and applies shatter_status"), StatusesOn(H, Near, EDTStatus::Chill), 1);
	}
	{
		FDTKeeperHarness H;
		const uint32 Near = Run(false, H);
		TestEqual(TEXT("a death that was not Frozen does not shatter"), HitsOn(H, Near, Tide), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamFrozenBlocks, "FrozenBlocksEnemies")
{
	using namespace DTKeeperExpressTest;
	const auto Run = [](bool bBlocks, FDTKeeperHarness& H)
	{
		const int32 Wall = H.AddEnemy(FVector2f(300.0f, 0.0f));
		const int32 Walker = H.AddEnemy(FVector2f(600.0f, 0.0f));
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FrostEntry(Flags).Params.Set(DTFlagParam::FrozenBlocksEnemies, bBlocks ? 1.0f : 0.0f);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		FreezeEnemy(H, Wall);
		for (int32 Step = 0; Step < 180; ++Step)
		{
			H.Enemies.Move(Dt, H.View.HeroPositions);
			H.StepOnce();
		}
		return H.Enemies.GetPositions()[Walker].X;
	};
	const float Rim = 300.0f + 2.0f * static_cast<float>(DTConst::ENM_SWARM_RADIUS_UU);
	const float OneStep = static_cast<float>(DTConst::ENM_SWARM_SPEED_UU_S) * Dt;
	{
		FDTKeeperHarness H;
		const float X = Run(true, H);
		TestTrue(FString::Printf(TEXT("the walker stops at the Frozen wall (x %.1f, rim %.1f)"), X, Rim), X >= Rim - 2.0f * OneStep);
	}
	{
		FDTKeeperHarness H;
		const float X = Run(false, H);
		TestTrue(FString::Printf(TEXT("without the card it walks through (x %.1f)"), X), X < Rim - 2.0f * OneStep);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamFreezeUntilHit, "FreezeUntilHit")
{
	using namespace DTKeeperExpressTest;
	// The harness never ticks statuses: a hold reads as the Freeze re-applied step after step.
	const auto Setup = [](FDTKeeperHarness& H, bool bHold)
	{
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Frost = FrostEntry(Flags);
		Frost.Params.Set(DTFlagParam::FreezeUntilHit, bHold ? 1.0f : 0.0f);
		Frost.Params.Set(DTFlagParam::HeldMax, 2.0f);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
	};
	{
		// Out of the rifle's range: nothing strikes them, so the hold runs, for at most held_max.
		FDTKeeperHarness H;
		TArray<uint32> Serials;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const int32 Enemy = H.AddEnemy(FVector2f(2000.0f, 200.0f * Index));
			Serials.Add(H.Enemies.GetSerials()[Enemy]);
			FreezeEnemy(H, Enemy, Dt);
		}
		Setup(H, true);
		H.StepMany(60);
		int32 Held = 0;
		for (const uint32 Serial : Serials)
		{
			Held += StatusesOn(H, Serial, EDTStatus::Freeze) > 30 ? 1 : 0;
		}
		TestEqual(TEXT("exactly held_max enemies are held"), Held, 2);
	}
	{
		// In range: the first hit ends the hold.
		FDTKeeperHarness H;
		const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Enemy];
		FreezeEnemy(H, Enemy, Dt);
		Setup(H, true);
		H.StepMany(60);
		const int32 Early = StatusesOn(H, Serial, EDTStatus::Freeze);
		H.StepMany(60);
		TestTrue(TEXT("a hit ends the hold"), Early > 0 && StatusesOn(H, Serial, EDTStatus::Freeze) == Early);
	}
	{
		FDTKeeperHarness H;
		const int32 Enemy = H.AddEnemy(FVector2f(2000.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Enemy];
		FreezeEnemy(H, Enemy, Dt);
		Setup(H, false);
		H.StepMany(60);
		TestEqual(TEXT("without the card nothing is held"), StatusesOn(H, Serial, EDTStatus::Freeze), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamFrozenHitChills, "FrozenHitChillsNeighbours")
{
	using namespace DTKeeperExpressTest;
	const auto Run = [](bool bFrozen, FDTKeeperHarness& H, uint32& OutNear, uint32& OutFar)
	{
		const int32 Struck = H.AddEnemy(FVector2f(300.0f, 0.0f));
		OutNear = SerialOf(H, H.AddEnemy(FVector2f(300.0f, 120.0f)));
		OutFar = SerialOf(H, H.AddEnemy(FVector2f(300.0f, 300.0f)));
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FrostEntry(Flags).Params.Set(DTFlagParam::FrozenHitChillsNeighboursUu, 150.0f);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		if (bFrozen)
		{
			FreezeEnemy(H, Struck);
		}
		H.StepMany(120);
	};
	uint32 Near = 0;
	uint32 Far = 0;
	{
		FDTKeeperHarness H;
		Run(true, H, Near, Far);
		TestTrue(TEXT("a neighbour in reach is Chilled"), StatusesOn(H, Near, EDTStatus::Chill) > 0);
		TestEqual(TEXT("one past the reach is not"), StatusesOn(H, Far, EDTStatus::Chill), 0);
	}
	{
		FDTKeeperHarness H;
		Run(false, H, Near, Far);
		TestEqual(TEXT("a hit on an enemy that is not Frozen Chills nothing"), StatusesOn(H, Near, EDTStatus::Chill), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamClusterFreeze, "ClusterFreeze")
{
	using namespace DTKeeperExpressTest;
	const uint16 Tide = DTKeepers::KeeperSource(EDTPath::Tide);
	const auto Bless = [](FDTKeeperHarness& H, int32 Min)
	{
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Pull = AddFlag(Flags, DTCat::EFlag::Pull, EDTPath::Tide);
		Pull.Params.Set(DTFlagParam::DistanceUu, 1.0f);
		Pull.Params.Set(DTFlagParam::RadiusUu, 200.0f);
		Pull.Params.Set(DTFlagParam::ClusterFreezeMin, static_cast<float>(Min));
		Pull.Params.Set(DTFlagParam::Shatter, 1.0f);
		Pull.Params.Set(DTFlagParam::ShatterUu, 200.0f);
		Pull.Params.Set(DTFlagParam::ShatterDmg, 0.5f);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
	};
	{
		FDTKeeperHarness H;
		H.AddBlock();
		const uint32 Serial = H.Enemies.GetSerials()[0];
		Bless(H, 5);
		H.StepMany(FireSteps);
		TestTrue(FString::Printf(TEXT("touching pulled enemies Freeze together (%d)"), H.FrozenEnemies()), H.FrozenEnemies() >= 5);
		TestEqual(TEXT("and shatter for shatter_dmg of the strongest weapon"), BiggestHit(H, Tide), 0.5f * PlainDamage(), 0.01f);
		TestTrue(FString::Printf(TEXT("a Frozen cluster does not burst on every impact (%d hits)"), HitsOn(H, Serial, Tide)), HitsOn(H, Serial, Tide) < 10);
	}
	{
		FDTKeeperHarness H;
		H.AddBlock();
		Bless(H, 1000);
		H.StepMany(FireSteps);
		TestEqual(TEXT("below the minimum nothing Freezes"), H.FrozenEnemies(), 0);
		TestEqual(TEXT("and nothing shatters"), H.Damage(Tide), 0.0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamChillOnPull, "ChillStacksOnPull")
{
	using namespace DTKeeperExpressTest;
	const auto MostStacks = [](const FDTKeeperHarness& H)
	{
		float Most = 0.0f;
		for (const FDTStatusApply& Apply : H.StatusLog)
		{
			Most = Apply.Status == EDTStatus::Chill ? FMath::Max(Most, Apply.Magnitude) : Most;
		}
		return Most;
	};
	{
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
		H.StepMany(FireSteps);
		TestEqual(TEXT("the Key's pull Chills one stack"), MostStacks(H), 1.0f);
	}
	{
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
		H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Amp1")));
		H.StepMany(FireSteps);
		TestEqual(TEXT("Cold Draw: chill_stacks_on_pull stacks"), MostStacks(H), 2.0f);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamChargedStatus, "StillnessChargedStatus")
{
	using namespace DTKeeperExpressTest;
	const auto Run = [](bool bWalk, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
		for (int32 Step = 0; Step < FireSteps; ++Step)
		{
			H.View.HeroPositions[0] = FVector2f(0.0f, bWalk ? 80.0f * (Step % 2) : 0.0f);
			H.StepOnce();
		}
	};
	{
		FDTKeeperHarness H;
		Run(false, H);
		TestTrue(TEXT("a charged volley Freezes what it hits"), H.CountStatuses(EDTStatus::Freeze, DTKeepers::KeeperSource(EDTPath::Tide)) > 0);
	}
	{
		FDTKeeperHarness H;
		Run(true, H);
		TestEqual(TEXT("an uncharged volley does not"), H.CountStatuses(EDTStatus::Freeze, DTKeepers::KeeperSource(EDTPath::Tide)), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamChargedPull, "StillnessChargedPull")
{
	using namespace DTKeeperExpressTest;
	{
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Amp1")));
		H.StepMany(FireSteps);
		float Longest = 0.0f;
		for (const FDTDisplacement& Move : H.MoveLog)
		{
			Longest = FMath::Max(Longest, Move.Delta.Size());
		}
		TestTrue(TEXT("a charged impact pulls"), H.MoveLog.Num() > 0);
		TestTrue(FString::Printf(TEXT("up to distance_uu (%.1f)"), Longest), Longest <= 150.0f + 0.01f);
	}
	{
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
		H.StepMany(FireSteps);
		TestEqual(TEXT("the Key alone pulls nothing"), H.MoveLog.Num(), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamChargedAura, "StillnessChargedAura")
{
	using namespace DTKeeperExpressTest;
	const auto Run = [](bool bAura, FDTKeeperHarness& H, uint32& OutNear, uint32& OutFar)
	{
		H.AddEnemy(FVector2f(250.0f, 0.0f));
		OutNear = SerialOf(H, H.AddEnemy(FVector2f(0.0f, 280.0f)));
		OutFar = SerialOf(H, H.AddEnemy(FVector2f(0.0f, 420.0f)));
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
		if (bAura)
		{
			H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Trait")));
		}
		H.StepMany(FireSteps);
	};
	uint32 Near = 0;
	uint32 Far = 0;
	{
		FDTKeeperHarness H;
		Run(true, H, Near, Far);
		TestTrue(TEXT("while charged the hero Chills what is in reach"), StatusesOn(H, Near, EDTStatus::Chill) > 0);
		TestEqual(TEXT("and nothing past it"), StatusesOn(H, Far, EDTStatus::Chill), 0);
		TestEqual(TEXT("the card's Field half leaves no zone on impacts"), H.Weapons.GetLiveFields(), 0);
	}
	{
		FDTKeeperHarness H;
		Run(false, H, Near, Far);
		TestEqual(TEXT("without the Trait no aura"), StatusesOn(H, Near, EDTStatus::Chill), 0);
	}
	return true;
}

// T-PRE-11: each control cue appears exactly when the sim is doing what it shows, in a cool token that holds its contrast.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPreControlCues, "Dusktracer.Pre.ControlCues", DTWeaponTestHarness::TestFlags)
bool FDTPreControlCues::RunTest(const FString& Parameters)
{
	using namespace DTKeeperExpressTest;
	const auto Count = [](FDTKeeperHarness& H, FDTWeaponVisual::EKind Kind, FDTWeaponVisual* OutLast = nullptr)
	{
		TArray<FDTWeaponVisual> Visuals;
		H.Weapons.GatherVisuals(H.Run, H.View, Visuals);
		int32 Found = 0;
		for (const FDTWeaponVisual& Visual : Visuals)
		{
			if (Visual.Kind == Kind)
			{
				++Found;
				if (OutLast)
				{
					*OutLast = Visual;
				}
			}
		}
		return Found;
	};
	const auto SetupFrost = [](FDTKeeperHarness& H, bool bBlocks, bool bHold)
	{
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Frost = FrostEntry(Flags);
		Frost.Params.Set(DTFlagParam::FrozenBlocksEnemies, bBlocks ? 1.0f : 0.0f);
		Frost.Params.Set(DTFlagParam::FreezeUntilHit, bHold ? 1.0f : 0.0f);
		Frost.Params.Set(DTFlagParam::HeldMax, 2.0f);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
	};

	// The wall: every enemy this player Froze, only with the card.
	for (const bool bBlocks : { true, false })
	{
		FDTKeeperHarness H;
		const FVector2f At(2000.0f, 0.0f);
		const int32 Frozen = H.AddEnemy(At);
		H.AddEnemy(FVector2f(2000.0f, 300.0f));
		FreezeEnemy(H, Frozen);
		SetupFrost(H, bBlocks, false);
		H.StepMany(2);
		FDTWeaponVisual Wall;
		TestEqual(TEXT("one plate per Frozen enemy, with the card"), Count(H, FDTWeaponVisual::EKind::FrozenWall, &Wall), bBlocks ? 1 : 0);
		if (bBlocks)
		{
			TestEqual(TEXT("under the enemy"), Wall.Position, At);
			TestEqual(TEXT("sized from its radius"), Wall.Radius, static_cast<float>(DTConst::ENM_SWARM_RADIUS_UU), 0.01f);
			TestEqual(TEXT("in the wall bucket"), DTWeaponPalette::BucketOf(Wall), DTWeaponPalette::WallBucket());
			TestTrue(TEXT("the wall bucket is Pal.Keeper.Tide"), DTWeaponPalette::BucketToken(DTWeaponPalette::WallBucket()) == DTPal::Keeper_Tide);
		}
		TestEqual(TEXT("no frame while nothing is held"), Count(H, FDTWeaponVisual::EKind::FrozenHold), 0);
		FDTWeaponVisual Plain;
		TestEqual(TEXT("the Frozen enemy without a card shows the plain cue, and only it"), Count(H, FDTWeaponVisual::EKind::FrozenPlain, &Plain), bBlocks ? 0 : 1);
		if (!bBlocks)
		{
			TestEqual(TEXT("on the enemy"), Plain.Position, At);
			TestTrue(TEXT("a plain Frozen is a cue"), DTWeaponCues::IsCue(Plain));
			TestEqual(TEXT("in its own bucket"), DTWeaponCues::BucketOf(Plain), static_cast<int32>(DTWeaponCues::Frozen));
			TArray<FDTTelegraph> Bars;
			DTWeaponCues::Append(Plain, Bars);
			TestEqual(TEXT("a diamond of four bars"), Bars.Num(), 4);
			const float Half = Plain.Radius * DTConst::PRE_FROZEN_CUE_RATIO;
			for (const FDTTelegraph& Bar : Bars)
			{
				const FVector2f Start = Bar.Origin - Plain.Position;
				TestEqual(TEXT("each starts on a diamond corner"), FMath::Abs(Start.X) + FMath::Abs(Start.Y), Half, 0.01f);
				TestTrue(TEXT("and runs on the diagonal, never like the hold's square"), FMath::IsNearlyEqual(FMath::Abs(Bar.Dir.X), FMath::Abs(Bar.Dir.Y), 0.01f));
			}
		}
	}

	// The hold: exactly the held enemies, a frame of four bars around each.
	{
		FDTKeeperHarness H;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FreezeEnemy(H, H.AddEnemy(FVector2f(2000.0f, 200.0f * Index)), Dt);
		}
		SetupFrost(H, false, true);
		H.StepMany(60);
		FDTWeaponVisual Hold;
		TestEqual(TEXT("one frame per held enemy, held_max at most"), Count(H, FDTWeaponVisual::EKind::FrozenHold, &Hold), 2);
		TestEqual(TEXT("no plate without the wall card"), Count(H, FDTWeaponVisual::EKind::FrozenWall), 0);
		TestEqual(TEXT("the unheld Frozen enemy shows the plain cue"), Count(H, FDTWeaponVisual::EKind::FrozenPlain), 1);
		TestTrue(TEXT("a frame is a cue"), DTWeaponCues::IsCue(Hold));
		TArray<FDTTelegraph> Bars;
		DTWeaponCues::Append(Hold, Bars);
		TestEqual(TEXT("four bars"), Bars.Num(), 4);
		const float Half = Hold.Radius * DTConst::PRE_HOLD_FRAME_RATIO;
		for (const FDTTelegraph& Bar : Bars)
		{
			TestEqual(TEXT("a bar spans the frame"), Bar.Length, 2.0f * Half, 0.01f);
			const FVector2f End = Bar.Origin + Bar.Dir * Bar.Length - Hold.Position;
			TestEqual(TEXT("and ends on a corner"), FMath::Max(FMath::Abs(End.X), FMath::Abs(End.Y)), Half, 0.01f);
		}
		TestEqual(TEXT("the frame token"), DTWeaponCues::BucketOf(Hold), static_cast<int32>(DTWeaponCues::Hold));
	}

	// The aura: only with the Trait, only while a charge is held, reaching exactly charged_aura_uu.
	for (const bool bTrait : { true, false })
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
		if (bTrait)
		{
			H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Trait")));
		}
		TestEqual(TEXT("no ring before the charge"), Count(H, FDTWeaponVisual::EKind::StillAura), 0);
		TestEqual(TEXT("no dial before a step"), Count(H, FDTWeaponVisual::EKind::StillCharge), 0);
		H.StepMany(3);
		FDTWeaponVisual Dial;
		TestEqual(TEXT("the dial shows the charge gathering, Trait or not"), Count(H, FDTWeaponVisual::EKind::StillCharge, &Dial), 1);
		TestEqual(TEXT("nothing held yet"), Dial.Charges, 0);
		TestTrue(TEXT("part way to a charge"), Dial.Charge01 > 0.0f && Dial.Charge01 < 1.0f);
		H.StepMany(FireSteps);
		TestEqual(TEXT("the dial stays while a charge is held"), Count(H, FDTWeaponVisual::EKind::StillCharge, &Dial), 1);
		TestEqual(TEXT("one charge at charges_max 1"), Dial.Charges, 1);
		TestEqual(TEXT("and nothing more to gather"), Dial.Charge01, 0.0f);
		TestEqual(TEXT("on the ground bucket"), DTWeaponCues::BucketOf(Dial), static_cast<int32>(DTWeaponCues::Ground));
		{
			TArray<FDTTelegraph> Closed;
			DTWeaponCues::Append(Dial, Closed);
			TestEqual(TEXT("a held charge is one closed ring"), Closed.Num(), 1);
			TestTrue(TEXT("a ring shape"), Closed.Num() == 1 && Closed[0].Shape == EDTTelegraphShape::Ring);
			FDTWeaponVisual Next = Dial;
			Next.Charge01 = 0.5f;
			TArray<FDTTelegraph> Gathering;
			DTWeaponCues::Append(Next, Gathering);
			TestEqual(TEXT("then ticks for the next, half the dial at one half"), Gathering.Num(), 1 + DTConst::PRE_CHARGE_TICKS / 2);
			FDTWeaponVisual Two = Dial;
			Two.Charges = 2;
			TArray<FDTTelegraph> Pair;
			DTWeaponCues::Append(Two, Pair);
			TestEqual(TEXT("two charges, two rings"), Pair.Num(), 2);
			TestTrue(TEXT("each its own radius"), Pair.Num() == 2 && Pair[1].Length > Pair[0].Length + Pair[0].Width);
		}
		FDTWeaponVisual Aura;
		TestEqual(TEXT("a ring while charged, with the Trait"), Count(H, FDTWeaponVisual::EKind::StillAura, &Aura), bTrait ? 1 : 0);
		if (bTrait)
		{
			TArray<FDTTelegraph> Ring;
			DTWeaponCues::Append(Aura, Ring);
			TestEqual(TEXT("one ring"), Ring.Num(), 1);
			TestTrue(TEXT("a ring shape"), Ring[0].Shape == EDTTelegraphShape::Ring);
			TestEqual(TEXT("its outer edge is the reach"), Ring[0].Length, Aura.Radius, 0.01f);
			FDTWeaponVisual Pulsing = Aura;
			Pulsing.Pulse01 = 1.0f;
			TArray<FDTTelegraph> Wide;
			DTWeaponCues::Append(Pulsing, Wide);
			TestEqual(TEXT("a pulse thickens it inward"), Wide[0].Width, static_cast<float>(DTConst::PRE_AURA_RING_UU + DTConst::PRE_AURA_PULSE_UU), 0.01f);
			TestEqual(TEXT("and never past the reach"), Wide[0].Length, Aura.Radius, 0.01f);
		}
	}

	// Cool tokens at the drawn emissive, clear of both ground references (R-PRE-05) and of the enemy hues.
	const auto Luminance = [](const FColor& Token, float Emissive)
	{
		const FLinearColor C = DTPal::Linear(Token);
		return FMath::Min(1.0f, (0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B) * Emissive);
	};
	for (int32 Bucket = 0; Bucket < DTWeaponCues::NumBuckets; ++Bucket)
	{
		const FColor Token = DTWeaponCues::Token(Bucket);
		for (const FColor& Ground : { DTPal::Ground_Shadow, DTPal::Ground_Bright })
		{
			const float Contrast = (Luminance(Token, DTWeaponCues::Emissive(Bucket)) + 0.05f) / (Luminance(Ground, 1.0f) + 0.05f);
			TestTrue(FString::Printf(TEXT("cue contrast %.2f"), Contrast), Contrast >= DTConst::PRE_CONTRAST_MIN);
		}
		const float Hue = FLinearColor(Token.ReinterpretAsLinear()).LinearRGBToHSV().R;
		TestTrue(FString::Printf(TEXT("cue hue %.0f stays cool, clear of red and magenta"), Hue), Hue > 120.0f && Hue < 260.0f);
	}
	for (int32 Bucket = 0; Bucket < DTWeaponCues::NumBuckets; ++Bucket)
	{
		for (const FColor& Forbidden : { DTPal::Telegraph, DTPal::Enemy_Bullet, DTPal::Enemy_Optic, DTPal::Elite_Light })
		{
			TestTrue(TEXT("no cue reads an enemy or telegraph token"), DTWeaponCues::Token(Bucket) != Forbidden);
		}
	}

	// Draw order: zones lie on the ground under the ground cues, those under telegraphs; hull cues sit at the hull's own height.
	const float ZoneTop = static_cast<float>(DTConst::PRE_ZONE_Z_UU + DTConst::PRE_ZONE_HALF_UU);
	TestTrue(TEXT("zones are above the floor"), DTConst::PRE_ZONE_Z_UU > DTConst::PRE_ZONE_HALF_UU);
	TestTrue(TEXT("a zone never covers a ground cue"), ZoneTop < DTWeaponCues::DrawZ(DTWeaponCues::Ground));
	TestTrue(TEXT("a ground cue never covers a telegraph"), DTWeaponCues::DrawZ(DTWeaponCues::Ground) < static_cast<float>(DTConst::ENEMIES_TELEGRAPH_Z_UU));
	TestEqual(TEXT("the hold frame sits at hull height"), DTWeaponCues::DrawZ(DTWeaponCues::Hold), static_cast<float>(DTConst::SIM_PLANE_Z_UU));
	TestEqual(TEXT("the plain Frozen diamond too"), DTWeaponCues::DrawZ(DTWeaponCues::Frozen), static_cast<float>(DTConst::SIM_PLANE_Z_UU));
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamChargedFreezeRing, "StillnessChargedFreezeRing")
{
	using namespace DTKeeperExpressTest;
	const auto Run = [](float Ring, FDTKeeperHarness& H, uint32& OutNear, uint32& OutFar)
	{
		H.AddEnemy(FVector2f(250.0f, 0.0f));
		OutNear = SerialOf(H, H.AddEnemy(FVector2f(0.0f, 280.0f)));
		OutFar = SerialOf(H, H.AddEnemy(FVector2f(0.0f, 420.0f)));
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Still = AddFlag(Flags, DTCat::EFlag::Stillness, EDTPath::Tide);
		Still.Params.Set(DTFlagParam::StillMoveUu, 50.0f);
		Still.Params.Set(DTFlagParam::ChargeS, 0.6f);
		Still.Params.Set(DTFlagParam::Mult, 2.0f);
		Still.Params.Set(DTFlagParam::ChargedFreezeRingUu, Ring);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		H.StepMany(FireSteps);
	};
	uint32 Near = 0;
	uint32 Far = 0;
	{
		FDTKeeperHarness H;
		Run(300.0f, H, Near, Far);
		TestTrue(TEXT("a charged volley Freezes the ring around the hero"), StatusesOn(H, Near, EDTStatus::Freeze) > 0);
		TestEqual(TEXT("not past it"), StatusesOn(H, Far, EDTStatus::Freeze), 0);
	}
	{
		FDTKeeperHarness H;
		Run(0.0f, H, Near, Far);
		TestEqual(TEXT("without the tier no ring"), StatusesOn(H, Near, EDTStatus::Freeze), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamAllVolleysCharged, "StillnessAllVolleysCharged")
{
	using namespace DTKeeperExpressTest;
	const uint16 Tide = DTKeepers::KeeperSource(EDTPath::Tide);
	const auto Run = [](bool bCast, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Stillness"), TEXT("Key")));
		if (bCast)
		{
			H.Keepers.QueueIntercession(0);
		}
		for (int32 Step = 0; Step < 300; ++Step)
		{
			H.View.HeroPositions[0] = FVector2f(0.0f, 80.0f * (Step % 2)); // walks: past still_move_uu, inside pool_radius_uu
			H.StepOnce();
		}
	};
	{
		FDTKeeperHarness H;
		Run(true, H);
		TestTrue(TEXT("the cast runs"), H.Keepers.IsCasting(0));
		TestTrue(FString::Printf(TEXT("every volley of the cast is charged (%d Keeper hits)"), H.CountHits(Tide)), H.CountHits(Tide) >= 16);
	}
	{
		FDTKeeperHarness H;
		Run(false, H);
		TestEqual(TEXT("walking without the cast charges nothing"), H.CountHits(Tide), 0);
	}
	return true;
}

// ---- the other Keepers' cards ----------------------------------------------------------------------------------------------------

DT_KEEPER_PARAM_TEST(FDTKeeperParamAfterHit, "AfterHitTakenMult")
{
	using namespace DTKeeperExpressTest;
	const uint16 Forge = DTKeepers::KeeperSource(EDTPath::Forge);
	const auto Run = [](bool bAmp, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(200.0f, 0.0f));
		H.Patron(EDTPath::Forge);
		H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Key")));
		if (bAmp)
		{
			H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Amp1")));
		}
		FDTHeroHit& Taken = H.View.HeroHits.AddDefaulted_GetRef();
		Taken.PlayerId = 0;
		H.StepOnce();
		H.View.HeroHits.Reset();
	};
	const float Ratio = DTKeepers::PathParam(EDTPath::Forge, DTCat::EFlag::Slam, DTFlagParam::DmgRatio, 0.0f);
	{
		FDTKeeperHarness H;
		Run(true, H);
		TestEqual(TEXT("the slam after a hit deals x after_hit_taken_mult"), BiggestHit(H, Forge), 2.0f * Ratio * PlainDamage(), 0.01f);
		H.HitLog.Reset();
		H.View.HeroIntangible[0] = true;
		H.StepOnce();
		H.View.HeroIntangible[0] = false;
		H.StepOnce();
		TestEqual(TEXT("only that slam: the next one is plain"), BiggestHit(H, Forge), Ratio * PlainDamage(), 0.01f);
	}
	{
		FDTKeeperHarness H;
		Run(false, H);
		TestEqual(TEXT("without the card the slam is plain"), BiggestHit(H, Forge), Ratio * PlainDamage(), 0.01f);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamEmbers, "Embers")
{
	using namespace DTKeeperExpressTest;
	const uint16 Forge = DTKeepers::KeeperSource(EDTPath::Forge);
	const auto Run = [](bool bAmp, FDTKeeperHarness& H)
	{
		// Just past the slam's reach, on the first ember's line.
		const int32 Enemy = H.AddEnemy(FVector2f(440.0f, 0.0f));
		H.Patron(EDTPath::Forge, 0, false);
		H.Weapons.Give(H.Run, 0, ArcBlade, 1); // reaches nothing here, so only an ember can touch the enemy
		H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Key")));
		if (bAmp)
		{
			H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Amp2")));
		}
		H.View.HeroIntangible[0] = true;
		H.StepOnce();
		H.View.HeroIntangible[0] = false;
		const uint32 Serial = H.Enemies.GetSerials()[Enemy];
		H.StepMany(60);
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(true, H);
		TestEqual(TEXT("an ember strikes past the ring"), HitsOn(H, Serial, Forge), 1);
		TestEqual(TEXT("and Burns what it hits"), StatusesOn(H, Serial, EDTStatus::Burn), 1);
	}
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(false, H);
		TestEqual(TEXT("without the card nothing passes the ring"), HitsOn(H, Serial, Forge), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamDetonate, "DetonateBurning")
{
	using namespace DTKeeperExpressTest;
	const uint16 Forge = DTKeepers::KeeperSource(EDTPath::Forge);
	const auto Run = [](bool bBurning, bool bTrait, FDTKeeperHarness& H)
	{
		const int32 Enemy = H.AddEnemy(FVector2f(200.0f, 0.0f));
		H.Patron(EDTPath::Forge);
		H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Key")));
		if (bTrait)
		{
			H.Bless(Blessing(TEXT("Forge.Slam"), TEXT("Trait")));
		}
		if (bBurning)
		{
			Burn(H, Enemy);
		}
		const uint32 Serial = H.Enemies.GetSerials()[Enemy];
		H.View.HeroIntangible[0] = true;
		H.StepOnce();
		H.View.HeroIntangible[0] = false;
		H.StepOnce();
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(true, true, H);
		TestEqual(TEXT("the slam and the detonation"), HitsOn(H, Serial, Forge), 2);
		double Smallest = 1.0e12;
		for (const FDTHit& Hit : H.HitLog)
		{
			Smallest = Hit.SourceId == Forge ? FMath::Min(Smallest, static_cast<double>(Hit.Damage)) : Smallest;
		}
		TestEqual(TEXT("the detonation is what the Burn still owes"), Smallest, static_cast<double>(DTConst::STATUS_BURN_S), 0.01);
	}
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(false, true, H);
		TestEqual(TEXT("nothing Burning, nothing to detonate"), HitsOn(H, Serial, Forge), 1);
	}
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(true, false, H);
		TestEqual(TEXT("without the card a slam only strikes"), HitsOn(H, Serial, Forge), 1);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamBurnTickRate, "BurnTickRate")
{
	using namespace DTKeeperExpressTest;
	const auto Strongest = [](bool bAmp)
	{
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Patron(EDTPath::Forge);
		H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")));
		if (bAmp)
		{
			H.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Amp1")));
		}
		H.StepMany(FireSteps);
		float Most = 0.0f;
		for (const FDTStatusApply& Apply : H.StatusLog)
		{
			Most = Apply.Status == EDTStatus::Burn ? FMath::Max(Most, Apply.Magnitude) : Most;
		}
		return Most;
	};
	const float Plain = Strongest(false);
	TestEqual(TEXT("a Burn is the ratio of its hit"), Plain, DTConst::STATUS_BURN_DPS_RATIO * PlainDamage(), 0.01f);
	TestEqual(TEXT("burn_tick_rate raises it"), Strongest(true), 1.25f * Plain, 0.01f);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamCleaveModifiers, "ApplyModifierOnHit")
{
	using namespace DTKeeperExpressTest;
	const auto Hits = [](const TCHAR* Mod, bool bTrait)
	{
		FDTKeeperHarness H;
		H.AddBlock();
		H.Patron(EDTPath::Forge, 0, false);
		H.Weapons.Give(H.Run, 0, PulseRifle, 3); // three rounds: two fold into the cleave, one still flies, so the modifier stays live
		H.Weapons.Socket(H.Run, 0, 0, FName(Mod));
		H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
		if (bTrait)
		{
			H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Trait")));
		}
		H.StepMany(120);
		return H.CountHits(DTWeapons::ModifierSource(DTWeapons::FindModifier(FName(Mod))));
	};
	const TCHAR* Mod = TEXT("Mod.Ricochet");
	const int32 Plain = Hits(Mod, false);
	const int32 Carried = Hits(Mod, true);
	TestTrue(FString::Printf(TEXT("a cleave carries the modifier (%d > %d)"), Carried, Plain), Carried > Plain);
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamEveryN, "EveryNSkyStrike")
{
	using namespace DTKeeperExpressTest;
	const uint16 Storm = DTKeepers::KeeperSource(EDTPath::Storm);
	const auto Run = [](bool bTrait, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		const int32 Far = H.AddEnemy(FVector2f(-1200.0f, 0.0f)); // on screen, past the rifle's reach
		H.Patron(EDTPath::Storm);
		H.Bless(Blessing(TEXT("Storm.Downpour"), TEXT("Key")));
		if (bTrait)
		{
			H.Bless(Blessing(TEXT("Storm.Downpour"), TEXT("Trait")));
		}
		const uint32 Serial = H.Enemies.GetSerials()[Far];
		H.StepMany(FireSteps);
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(true, H);
		const int32 Echoes = H.CountHits(Storm) - HitsOn(H, Serial, Storm);
		const int32 Screen = HitsOn(H, Serial, Storm);
		TestTrue(FString::Printf(TEXT("every Nth strike hits the screen (%d of %d)"), Screen, Echoes), Screen > 0 && Screen >= Echoes / 8 - 1 && Screen <= Echoes / 8 + 1);
	}
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(false, H);
		TestEqual(TEXT("without the card a strike stays at its impact"), HitsOn(H, Serial, Storm), 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamKillShocks, "KillShocksNeighbours")
{
	using namespace DTKeeperExpressTest;
	const auto Run = [](float Reach, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.AddEnemy(FVector2f(300.0f, 150.0f), 1.0f); // the arc's victim
		const int32 Near = H.AddEnemy(FVector2f(300.0f, 480.0f));
		H.Patron(EDTPath::Storm);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Chain = AddFlag(Flags, DTCat::EFlag::Chain, EDTPath::Storm);
		Chain.Params.Set(DTFlagParam::Jumps, 1.0f);
		Chain.Params.Set(DTFlagParam::RangeUu, 160.0f);
		Chain.Params.Set(DTFlagParam::Dmg, 1.0f);
		Chain.Params.Set(DTFlagParam::KillShocksNeighboursUu, Reach);
		H.Weapons.AddFlagSource(0, TestSource, Flags);
		const uint32 Serial = H.Enemies.GetSerials()[Near];
		H.StepMany(120);
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(400.0f, H);
		TestEqual(TEXT("an arc's kill Shocks what is in reach of the death"), StatusesOn(H, Serial, EDTStatus::Shock), 1);
	}
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(0.0f, H);
		TestEqual(TEXT("without the tier it does not"), StatusesOn(H, Serial, EDTStatus::Shock), 0);
	}
	return true;
}

// ---- Pull interval and field status (D-016) ---------------------------------------------------------------------------------------

namespace DTKeeperPullTest
{
	using namespace DTKeeperTest;

	static int32 MovesOn(const FDTKeeperHarness& H, uint32 Serial)
	{
		int32 Count = 0;
		for (const FDTDisplacement& Move : H.MoveLog)
		{
			Count += Move.EnemySerial == Serial ? 1 : 0;
		}
		return Count;
	}

	static float LongestMove(const FDTKeeperHarness& H, uint32 Serial)
	{
		float Best = 0.0f;
		for (const FDTDisplacement& Move : H.MoveLog)
		{
			Best = Move.EnemySerial == Serial ? FMath::Max(Best, Move.Delta.Size()) : Best;
		}
		return Best;
	}
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamPullInterval, "PullInterval") // R-KEE-21
{
	using namespace DTKeeperParamTest;
	using namespace DTKeeperExpressTest;
	using namespace DTKeeperPullTest;
	const int32 IntervalSteps = FMath::RoundToInt(DTConst::KEEPER_PULL_INTERVAL_S / Dt);
	const int32 Intervals = 3;
	const int32 Allowed = Intervals + 1; // the first pull, then one per elapsed interval
	const auto Tide = [](FDTKeeperHarness& H, bool bBackwash)
	{
		H.Patron(EDTPath::Tide);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Pull = AddFlag(Flags, DTCat::EFlag::Pull, EDTPath::Tide);
		Pull.Params.Set(DTFlagParam::DistanceUu, 1.0f);
		Pull.Params.Set(DTFlagParam::RadiusUu, 200.0f);
		if (bBackwash)
		{
			Pull.Params.SetWord(DTFlagParam::Target, FName(TEXT("Origin")));
		}
		else
		{
			FDTFlagEntry& Chill = AddFlag(Flags, DTCat::EFlag::StatusApply, EDTPath::Tide);
			Chill.Params.SetWord(DTFlagParam::Status, FName(TEXT("Status.Chill")));
			Chill.Params.SetWord(DTFlagParam::On, FName(TEXT("Pulled")));
			Chill.Params.Set(DTFlagParam::Chance, 1.0f);
		}
		H.Weapons.AddFlagSource(0, TestSource, Flags);
	};
	{
		FDTKeeperHarness H;
		const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Enemy];
		Tide(H, false);
		H.StepMany(IntervalSteps * Intervals);
		const int32 Moves = MovesOn(H, Serial);
		TestTrue(FString::Printf(TEXT("at most one pull per interval (%d in %d intervals)"), Moves, Intervals), Moves <= Allowed);
		TestTrue(FString::Printf(TEXT("and it pulls again after each interval (%d)"), Moves), Moves >= Intervals);
		TestEqual(TEXT("a Pulled status rides on the pull, not on every impact"), StatusesOn(H, Serial, EDTStatus::Chill), Moves);
	}
	{
		// Backwash is the same flag: the struck enemy alone, toward the round's origin.
		FDTKeeperHarness H;
		const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Enemy];
		Tide(H, true);
		H.StepMany(IntervalSteps * Intervals);
		TestTrue(FString::Printf(TEXT("a hit's own pull is held to the interval too (%d)"), MovesOn(H, Serial)), MovesOn(H, Serial) <= Allowed);
	}
	{
		// Per enemy: holding one enemy does not hold another that enters reach later.
		FDTKeeperHarness H;
		const int32 First = H.AddEnemy(FVector2f(300.0f, 0.0f));
		Tide(H, false);
		H.StepMany(IntervalSteps / 2);
		const int32 Second = H.AddEnemy(FVector2f(300.0f, 100.0f));
		H.StepMany(IntervalSteps / 4);
		TestTrue(TEXT("the first was pulled"), MovesOn(H, H.Enemies.GetSerials()[First]) >= 1);
		TestTrue(TEXT("the second is pulled inside the first's interval"), MovesOn(H, H.Enemies.GetSerials()[Second]) >= 1);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamFieldStatus, "FieldStatus") // R-KEE-22
{
	using namespace DTKeeperParamTest;
	using namespace DTKeeperExpressTest;
	const int32 PulseSteps = FMath::RoundToInt(DTConst::STATUS_BURN_TICK_S / Dt);
	const auto Field = [](FDTKeeperHarness& H, const TCHAR* Status)
	{
		H.Patron(EDTPath::Forge);
		FDTWeaponFlags Flags;
		FDTFlagEntry& Entry = AddFlag(Flags, DTCat::EFlag::Field, EDTPath::Forge);
		Entry.Params.Set(DTFlagParam::RadiusUu, 120.0f);
		Entry.Params.Set(DTFlagParam::DurationS, 2.0f);
		if (Status)
		{
			Entry.Params.SetWord(DTFlagParam::Status, FName(Status));
		}
		H.Weapons.AddFlagSource(0, TestSource, Flags);
	};
	const uint16 Forge = DTKeepers::KeeperSource(EDTPath::Forge);
	{
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f)); // takes the rounds
		const int32 Inside = H.AddEnemy(FVector2f(350.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Inside];
		Field(H, TEXT("Status.Burn"));
		const int32 Pulses = 8;
		H.StepMany(PulseSteps * Pulses);
		int32 Burns = 0;
		float Dps = 0.0f;
		for (const FDTStatusApply& Apply : H.StatusLog)
		{
			if (Apply.Handle.Serial == Serial && Apply.Status == EDTStatus::Burn && Apply.SourceId == Forge)
			{
				++Burns;
				Dps = FMath::Max(Dps, Apply.Magnitude);
			}
		}
		TestTrue(FString::Printf(TEXT("a field Burns what stands in it (%d)"), Burns), Burns > 0);
		// Many live fields overlap here; a stack replaced before its first tick would never deal damage.
		TestTrue(FString::Printf(TEXT("once per pulse however many fields overlap (%d in %d pulses)"), Burns, Pulses), Burns <= Pulses + 1);
		const float Plain = DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1)->Damage;
		TestEqual(TEXT("the Burn scales with the emission's damage (R-KEE-11)"), Dps, DTConst::STATUS_BURN_DPS_RATIO * Plain, 0.01f * Plain);
	}
	{
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		const int32 Watched = H.AddEnemy(FVector2f(350.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Watched];
		Field(H, nullptr);
		H.StepMany(PulseSteps * 8);
		TestEqual(TEXT("a field that names no status applies none"), StatusesOn(H, Serial, EDTStatus::Burn), 0);
	}
	{
		// The card: Downpour's Amp1 leaves a field that Shocks. The rifle stops on the first enemy, so the second is the field's.
		FDTKeeperHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		const int32 Watched = H.AddEnemy(FVector2f(350.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Watched];
		H.Patron(EDTPath::Storm);
		H.Bless(Blessing(TEXT("Storm.Downpour"), TEXT("Key")));
		H.Bless(Blessing(TEXT("Storm.Downpour"), TEXT("Amp1")));
		H.StepMany(120);
		TestTrue(TEXT("Downpour's field Shocks"), StatusesOn(H, Serial, EDTStatus::Shock) > 0);
	}
	return true;
}

DT_KEEPER_PARAM_TEST(FDTKeeperParamWhirlpoolLayer, "WhirlpoolLayer") // keepers.md Pull, expression.csv SecondKeeperLayer
{
	using namespace DTKeeperParamTest;
	using namespace DTKeeperExpressTest;
	using namespace DTKeeperPullTest;
	const auto Run = [](bool bCast, FDTKeeperHarness& H)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f)); // takes the rounds
		const int32 Watched = H.AddEnemy(FVector2f(300.0f, 180.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Watched];
		H.Patron(EDTPath::Storm);
		H.Commit(EDTPath::Tide);
		H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
		if (bCast)
		{
			H.Keepers.QueueIntercession(0);
		}
		H.StepMany(30);
		return Serial;
	};
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(true, H);
		TestEqual(TEXT("during the cast impacts pull by the layer's distance_uu"), LongestMove(H, Serial), 150.0f, 1.0f);
	}
	{
		FDTKeeperHarness H;
		const uint32 Serial = Run(false, H);
		TestEqual(TEXT("without the cast by the Key's"), LongestMove(H, Serial), 120.0f, 1.0f);
	}
	return true;
}

#undef DT_KEEPER_PARAM_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
