// Keeper tests (02_Systems/keepers.md T-KEE-01..20) plus one behaviour test per archetype Key on a Pulse Rifle.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Keepers; Quit" -unattended -nullrhi
// T-KEE-15 IntercessionBand and T-KEE-18 Budget are bot probes (Tools/power_split.py ablations): PLANNED until bot v1.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTPaletteGen.h"
#include "Keepers/DTKeeperSim.h"
#include "Keepers/DTKeeperSpec.h"
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
	TestTrue(TEXT("no tier line yet: chance 0.25"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), 0.25f));
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Amp2")));
	TestEqual(TEXT("T1 at exactly C.TIER.FAVOR_T1"), H.P().Keepers.Tier[Storm], 1);
	Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
	TestTrue(TEXT("Tempest Tier1 changes the Split flag: chance +0.10"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), 0.35f, 1e-4f));
	// Arc Grid catches up and leads on the tie (latest card): the tier lines follow the leader, the tier stays.
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Amp1")));
	H.Bless(Blessing(TEXT("Storm.ArcGrid"), TEXT("Amp2")));
	TestEqual(TEXT("T2 at C.TIER.FAVOR_T2"), H.P().Keepers.Tier[Storm], 2);
	TestEqual(TEXT("Arc Grid leads"), H.P().Keepers.Leading[Storm], FName(TEXT("Archetype.Storm.ArcGrid")));
	Split = H.Built(EDTPath::Storm, DTCat::EFlag::Split);
	const FDTFlagEntry* Chain = H.Built(EDTPath::Storm, DTCat::EFlag::Chain);
	TestTrue(TEXT("Tempest's tier line left with the lead"), Split && FMath::IsNearlyEqual(Split->Params.Get(DTFlagParam::Chance), 0.25f));
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
	// The Whirlpool Key has no damage term in expression.csv (pull + Chill; crush_dmg arrives with Amp2), so its
	// Keeper share is its control: the share of hits made on enemies the Keeper moved.
	AddInfo(FString::Printf(TEXT("Whirlpool Key damage share %.3f (no damage term by data)"), H.KeeperShare(EDTPath::Tide)));
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
	TestTrue(TEXT("standing still charges a volley: x mult damage"), Strongest >= 2.0f * Plain - KINDA_SMALL_NUMBER);
	TestTrue(FString::Printf(TEXT("Keeper share %.3f > 0"), H.KeeperShare(EDTPath::Tide)), H.KeeperShare(EDTPath::Tide) > 0.0);
	return true;
}

#undef DT_KEEPER_KEY_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
