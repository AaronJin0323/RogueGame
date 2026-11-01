// Modifier tests (02_Systems/modifiers.md T-MOD-01..10).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Modifiers; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponCompose.h"
#include "Core/DTCatalogGen.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Keepers/DTKeeperSpec.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferStyle.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTModifierTest
{
	using namespace DTWeaponTestHarness;

	static bool IsS1Weapon(int32 Weapon) { return DTCat::Weapons[Weapon].NumLevels > 0; }

	/** Every cap list a weapon can have in S1: declared, and under a full or partial ConvertMelee / ConvertOrbit. */
	static TArray<FDTCapList> CapVariants(int32 Weapon)
	{
		TArray<FDTCapList> Out;
		Out.Add(DTWeapons::DeclaredCaps(Weapon));
		for (const DTCat::EFlag Conv : { DTCat::EFlag::ConvertMelee, DTCat::EFlag::ConvertOrbit })
		{
			Out.Add(DTWeapons::EffectiveCaps(Weapon, Conv, true));
			Out.Add(DTWeapons::EffectiveCaps(Weapon, Conv, false));
		}
		return Out;
	}

	static FDTWeaponFlags KeeperFlag(DTCat::EFlag Flag, EDTPath Path, TFunctionRef<void(FDTFlagParams&)> Fill)
	{
		FDTWeaponFlags Flags;
		Fill(Flags.Add(Flag, DTWeapons::KeeperSource(Path), EDTFlagTier::Key).Params);
		return Flags;
	}

	static FDTOfferCard BlessingCard(const TCHAR* Id)
	{
		FDTOfferCard Card;
		Card.Kind = EDTCardKind::Blessing;
		Card.ContentId = Id;
		return Card;
	}

	/** A blessing pick as the run makes it: the Keeper state, then the flags the sim composes from that state. */
	static bool TakeForgeBlessing(FDTHarness& H, const FDTOfferCard& Card)
	{
		FDTPlayerRunState& Player = H.Run.GetPlayer(0);
		const bool bTaken = DTOffers::AddBlessing(Player.Keepers, Card.ContentId);
		H.Weapons.AddFlagSource(0, TEXT("Test.Hammer"), DTKeepers::BuildKeeperFlags(Player.Keepers, EDTPath::Forge));
		H.StepOnce();
		return bTaken;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersNeverIncompatible, "Dusktracer.Modifiers.NeverIncompatible", DTWeaponTestHarness::TestFlags) // T-MOD-01
bool FDTModifiersNeverIncompatible::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	// Full catalog cross product, over every cap list a weapon can reach: compatible means the weapon can express it.
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		for (const FDTCapList& Caps : CapVariants(Weapon))
		{
			for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
			{
				const DTCat::FModifierRow& Row = DTCat::Modifiers[Mod];
				if (!DTModCompat::IsCompatible(Caps, Mod))
				{
					continue;
				}
				const FString Pair = FString::Printf(TEXT("%s x %s"), DTCat::Weapons[Weapon].Id, Row.Id);
				TestTrue(Pair + TEXT(": a required cap present"), Row.RequiredCaps == 0 || (Caps.Mask & Row.RequiredCaps) != 0);
				TestTrue(Pair + TEXT(": no blocked cap"), (Caps.Mask & Row.BlockedCaps) == 0);
				if (Row.Flag != DTCat::EFlag::None)
				{
					TestEqual(Pair + TEXT(": its flag resolves native"), DTWeapons::ResolveFlag(Caps, Row.Flag).Compat, DTCat::ECompat::Native);
				}
			}
		}
	}
	// The socket and the composition agree with IsCompatible for every S1 weapon x modifier.
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		if (!IsS1Weapon(Weapon))
		{
			continue;
		}
		for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
		{
			const FString Pair = FString::Printf(TEXT("%s x %s"), DTCat::Weapons[Weapon].Id, DTCat::Modifiers[Mod].Id);
			const bool bCompatible = DTModCompat::IsCompatible(Weapon, Mod);
			FDTHarness H;
			H.Weapons.Give(H.Run, 0, DTWeapons::WeaponId(Weapon), 1);
			const EDTWeaponResult Socketed = H.Weapons.Socket(H.Run, 0, 0, DTWeapons::ModifierId(Mod));
			TestEqual(Pair + TEXT(": socket agrees"), Socketed == EDTWeaponResult::Ok, bCompatible);
			// Force an incompatible one in: the composition never applies it (dormant, no behaviour, no flag entry).
			FDTWeaponInstance Forced = H.Run.GetPlayer(0).Weapons[0];
			Forced.Sockets.Reset();
			Forced.Sockets.Add(FDTWeaponInstance::FSocket{ DTWeapons::ModifierId(Mod), false });
			const FDTWeaponSpec Spec = DTWeaponCompose::Compose(Forced, {}, FDTWeaponHeroStats(), 0);
			TestEqual(Pair + TEXT(": composition applies it only when compatible"), !Forced.Sockets[0].bDormant, bCompatible);
			const uint16 Source = DTWeapons::ModifierSource(Mod);
			const bool bApplied = Spec.Flags.ContainsByPredicate([Source](const FDTResolvedFlag& F) { return F.Entry.SourceId == Source; }) ||
				Spec.ModSources.Contains(Source);
			if (!bCompatible)
			{
				TestFalse(Pair + TEXT(": incompatible never applied"), bApplied);
			}
			for (const FDTResolvedFlag& Flag : Spec.Flags)
			{
				if (Flag.Entry.SourceId == Source)
				{
					TestTrue(Pair + TEXT(": applied flag is native"), Flag.IsNative());
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersMatrixGenerated, "Dusktracer.Modifiers.MatrixGenerated", DTWeaponTestHarness::TestFlags) // T-MOD-02
bool FDTModifiersMatrixGenerated::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	// The expected S1 table, built by hand from the stated rule (R-MOD-01) and the catalogs.
	struct FCell { const TCHAR* Weapon; const TCHAR* Mods; };
	const FCell Expected[] =
	{
		{ TEXT("Weapon.PulseRifle"), TEXT("SplitCore ScatterShell Pierce Mirror ChainTrigger LingeringField Backwash Conduit Fork Ricochet Homing OrbitLock Boomerang SpreadWave Spillover Refit") },
		{ TEXT("Weapon.ScatterCannon"), TEXT("SplitCore ScatterShell Pierce Mirror ChainTrigger LingeringField Backwash Conduit Fork Ricochet Homing OrbitLock Boomerang SpreadWave Spillover Refit") },
		{ TEXT("Weapon.RailLance"), TEXT("SplitCore ScatterShell Pierce Mirror ChainTrigger LingeringField Backwash Conduit Fork Ricochet Boomerang SpreadWave Spillover Refit") },
		{ TEXT("Weapon.FlakMortar"), TEXT("ScatterShell Mirror ChainTrigger LingeringField Backwash Conduit Fork Spillover Refit") },
		{ TEXT("Weapon.SeekerSwarm"), TEXT("SplitCore Pierce ChainTrigger LingeringField Backwash Conduit Fork Ricochet Spillover Refit") },
		{ TEXT("Weapon.SpiralProjector"), TEXT("SplitCore ScatterShell Pierce ChainTrigger LingeringField Backwash Conduit Ricochet Homing OrbitLock Boomerang SpreadWave Spillover Refit") },
		{ TEXT("Weapon.RingEmitter"), TEXT("SplitCore ScatterShell Pierce ChainTrigger LingeringField Backwash Conduit Ricochet Homing OrbitLock Boomerang SpreadWave Spillover Refit") },
		{ TEXT("Weapon.OrbitalDrones"), TEXT("Pierce ChainTrigger LingeringField Backwash Conduit Fork SpreadWave Spillover Refit") },
		{ TEXT("Weapon.ArcBlade"), TEXT("Mirror ChainTrigger LingeringField Backwash Conduit Spillover Refit") },
		{ TEXT("Weapon.GravityMine"), TEXT("ScatterShell ChainTrigger LingeringField Conduit Fork Spillover Refit") },
	};
	for (const FCell& Cell : Expected)
	{
		TArray<FString> Yes;
		FString(Cell.Mods).ParseIntoArrayWS(Yes);
		for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
		{
			const bool bWant = Yes.Contains(DTWeapons::ShortName(DTCat::Modifiers[Mod].Id));
			TestEqual(FString::Printf(TEXT("%s x %s"), Cell.Weapon, DTCat::Modifiers[Mod].Id),
				DTModCompat::IsCompatible(FName(Cell.Weapon), DTWeapons::ModifierId(Mod)), bWant);
		}
	}
	// The id overload, the index overload and the cap-list overload are one function.
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
		{
			const bool A = DTModCompat::IsCompatible(Weapon, Mod);
			TestEqual(TEXT("ids agree"), DTModCompat::IsCompatible(DTWeapons::WeaponId(Weapon), DTWeapons::ModifierId(Mod)), A);
			TestEqual(TEXT("caps agree"), DTModCompat::IsCompatible(DTWeapons::DeclaredCaps(Weapon), Mod), A);
		}
	}
	TestFalse(TEXT("unknown ids are never compatible"), DTModCompat::IsCompatible(FName(TEXT("Weapon.Nope")), FName(TEXT("Mod.Pierce"))));

	// The generated matrix.csv (Tools/gen_matrix.py) says what IsCompatible says, cell by cell.
	TArray<FString> Lines;
	const FString MatrixPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/09_Catalog/matrix.csv"));
	TestTrue(TEXT("matrix.csv exists"), FFileHelper::LoadFileToStringArray(Lines, *MatrixPath));
	TArray<FString> Weapons;
	int32 Cells = 0;
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(","), false);
		if (Weapons.Num() == 0)
		{
			Weapons = Parts;
			continue;
		}
		for (int32 Col = 1; Col < Parts.Num() && Col < Weapons.Num(); ++Col)
		{
			TestEqual(FString::Printf(TEXT("matrix %s x %s"), *Parts[0], *Weapons[Col]),
				DTModCompat::IsCompatible(FName(*Weapons[Col]), FName(*Parts[0])), Parts[Col] == TEXT("yes"));
			++Cells;
		}
	}
	TestEqual(TEXT("every cell present"), Cells, DTWeapons::NumWeapons() * DTWeapons::NumModifiers());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersSpecialisers, "Dusktracer.Modifiers.Specialisers", DTWeaponTestHarness::TestFlags) // T-MOD-03
bool FDTModifiersSpecialisers::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const TCHAR* Banned[] = { TEXT("damage_mult"), TEXT("dmg_mult"), TEXT("fire_rate"), TEXT("cooldown"), TEXT("size"), TEXT("crit"), TEXT("scale") };
	const TCHAR* NonFlagBehaviours[] = { TEXT("Mod.Pierce"), TEXT("Mod.Mirror"), TEXT("Mod.ChainTrigger"), TEXT("Mod.Fork"), TEXT("Mod.Homing"), TEXT("Mod.OrbitLock"),
		TEXT("Mod.Boomerang"), TEXT("Mod.SpreadWave"), TEXT("Mod.Spillover"), TEXT("Mod.Refit") };
	for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
	{
		const DTCat::FModifierRow& Row = DTCat::Modifiers[Mod];
		for (int32 Index = 0; Index < Row.NumParams; ++Index)
		{
			for (const TCHAR* Word : Banned)
			{
				TestFalse(FString::Printf(TEXT("%s param %s is not a multiplier"), Row.Id, Row.Params[Index].Name), FString(Row.Params[Index].Name).Contains(Word));
			}
		}
		bool bNamed = Row.Flag != DTCat::EFlag::None;
		for (const TCHAR* Id : NonFlagBehaviours)
		{
			bNamed |= FCString::Strcmp(Id, Row.Id) == 0;
		}
		TestTrue(FString::Printf(TEXT("%s names a behaviour"), Row.Id), bNamed);
	}
	// Behaviourally: socketing any compatible modifier leaves damage, cooldown, size and crit of the weapon unchanged.
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		if (!IsS1Weapon(Weapon))
		{
			continue;
		}
		FDTWeaponInstance Bare;
		Bare.Def = Weapon;
		const FDTWeaponSpec Base = DTWeaponCompose::Compose(Bare, {}, FDTWeaponHeroStats(), 0);
		for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
		{
			if (!DTModCompat::IsCompatible(Weapon, Mod))
			{
				continue;
			}
			FDTWeaponInstance With = Bare;
			With.Sockets.Add(FDTWeaponInstance::FSocket{ DTWeapons::ModifierId(Mod), false });
			const FDTWeaponSpec Spec = DTWeaponCompose::Compose(With, {}, FDTWeaponHeroStats(), 0);
			const FString Pair = FString::Printf(TEXT("%s x %s"), DTCat::Weapons[Weapon].Id, DTCat::Modifiers[Mod].Id);
			TestTrue(Pair + TEXT(": no number multiplier"), Spec.Damage == Base.Damage && Spec.Cooldown == Base.Cooldown &&
				Spec.Radius == Base.Radius && Spec.AreaRadius == Base.AreaRadius && Spec.CritChance == Base.CritChance && Spec.Count == Base.Count);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersSockets, "Dusktracer.Modifiers.Sockets", DTWeaponTestHarness::TestFlags) // T-MOD-04
bool FDTModifiersSockets::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.RingEmitter"), 1);
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.ArcBlade"), 1);
	TestEqual(TEXT("socket"), H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Pierce")), EDTWeaponResult::Ok);
	TestEqual(TEXT("a second copy is refused"), H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Pierce")), EDTWeaponResult::Duplicate);
	const TCHAR* More[] = { TEXT("Mod.Mirror"), TEXT("Mod.Fork"), TEXT("Mod.Backwash"), TEXT("Mod.Conduit") };
	int32 Ok = 1;
	for (const TCHAR* Mod : More)
	{
		const EDTWeaponResult Result = H.Weapons.Socket(H.Run, 0, 0, FName(Mod));
		if (Ok < DTConst::MOD_SOCKETS_PER_WEAPON)
		{
			TestEqual(FString::Printf(TEXT("%s fits"), Mod), Result, EDTWeaponResult::Ok);
			++Ok;
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s beyond the cap is refused"), Mod), Result, EDTWeaponResult::SocketsFull);
		}
	}
	TestEqual(TEXT("sockets at the cap"), H.Run.GetPlayer(0).Weapons[0].Sockets.Num(), DTConst::MOD_SOCKETS_PER_WEAPON);
	TestEqual(TEXT("incompatible refused"), H.Weapons.Socket(H.Run, 0, 2, TEXT("Mod.SplitCore")), EDTWeaponResult::Incompatible);
	TestEqual(TEXT("unknown refused"), H.Weapons.Socket(H.Run, 0, 2, TEXT("Mod.Nope")), EDTWeaponResult::UnknownId);
	// A modifier affects only its own weapon.
	const FDTWeaponSpec Ring = H.Weapons.ComposeSlot(H.Run, 0, 1);
	TestEqual(TEXT("ring pierce untouched by pulse's Pierce"), Ring.Pierce, DTWeapons::GetLevelRow(DTWeapons::FindWeapon(TEXT("Weapon.RingEmitter")), 1)->Pierce);
	TestEqual(TEXT("ring has no mirror"), Ring.MirrorSource, static_cast<uint16>(0));
	TestTrue(TEXT("pulse pierce raised"), H.Weapons.ComposeSlot(H.Run, 0, 0).Pierce > 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersStackCaps, "Dusktracer.Modifiers.StackCaps", DTWeaponTestHarness::TestFlags) // T-MOD-05
bool FDTModifiersStackCaps::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	// SplitCore + ScatterShell + Flag.Split (asking for 5 generations) in a dense field: never past the cap.
	FDTHarness H;
	for (int32 X = 0; X < 12; ++X)
	{
		for (int32 Y = -3; Y <= 3; ++Y)
		{
			H.AddEnemy(FVector2f(150.0f + 70.0f * X, 70.0f * Y));
		}
	}
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.SplitCore"));
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.ScatterShell"));
	H.Weapons.AddFlagSource(0, TEXT("Test.Tempest"), KeeperFlag(DTCat::EFlag::Split, EDTPath::Storm, [](FDTFlagParams& P)
	{
		P.Set(DTFlagParam::Chance, 1.0f);
		P.Set(DTFlagParam::Children, 2.0f);
		P.Set(DTFlagParam::Gen, 5.0f);
	}));
	const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 0);
	const FDTResolvedFlag* Keeper = Spec.Flags.FindByPredicate([](const FDTResolvedFlag& F)
	{
		return F.Entry.Flag == DTCat::EFlag::Split && F.Entry.SourceId == DTWeapons::KeeperSource(EDTPath::Storm);
	});
	TestTrue(TEXT("flag generations clamp at the cap"), Keeper && Keeper->Entry.Params.Get(DTFlagParam::Gen) == static_cast<float>(DTConst::MOD_SPLIT_GENERATIONS_MAX));
	H.StepMany(120);
	TestTrue(TEXT("splits happened"), H.Weapons.GetMaxGeneration() >= 1);
	TestTrue(FString::Printf(TEXT("max generation %d <= cap"), H.Weapons.GetMaxGeneration()), H.Weapons.GetMaxGeneration() <= DTConst::MOD_SPLIT_GENERATIONS_MAX);

	// Crit chance from all sources clamps at the cap.
	FDTWeaponHeroStats Stats;
	Stats.CritChance = 0.99f;
	H.Weapons.SetHeroStats(0, Stats);
	TestEqual(TEXT("crit clamps"), H.Weapons.ComposeSlot(H.Run, 0, 0).CritChance, DTConst::MOD_CRIT_CHANCE_CAP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersNoFitBonus, "Dusktracer.Modifiers.NoFitBonus", DTWeaponTestHarness::TestFlags) // T-MOD-06
bool FDTModifiersNoFitBonus::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
	{
		for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
		{
			if (!IsS1Weapon(Weapon) || !DTModCompat::IsCompatible(Weapon, Mod))
			{
				continue;
			}
			auto ComposeWith = [Weapon, Mod](EDTPath Patron, bool bKeeperFlags)
			{
				FDTHarness H;
				H.Run.GetPlayer(0).Keepers.Primary = Patron;
				H.Weapons.Give(H.Run, 0, DTWeapons::WeaponId(Weapon), 1);
				H.Weapons.Socket(H.Run, 0, 0, DTWeapons::ModifierId(Mod));
				if (bKeeperFlags)
				{
					// A Keeper flag that converts nothing (Chain) from the modifier's fit Path.
					H.Weapons.AddFlagSource(0, TEXT("Test.Fit"), KeeperFlag(DTCat::EFlag::Chain, Patron, [](FDTFlagParams& P) { P.Set(DTFlagParam::Jumps, 2.0f); }));
				}
				return H.Weapons.ComposeSlot(H.Run, 0, 0);
			};
			const FDTWeaponSpec Without = ComposeWith(EDTPath::None, false);
			for (int32 Path = 0; Path < 3; ++Path)
			{
				if ((DTCat::Modifiers[Mod].Fits & DTCat::PathBit(static_cast<EDTPath>(Path))) == 0)
				{
					continue;
				}
				const FDTWeaponSpec With = ComposeWith(static_cast<EDTPath>(Path), true);
				const FString Pair = FString::Printf(TEXT("%s on %s with its fit Keeper"), DTCat::Modifiers[Mod].Id, DTCat::Weapons[Weapon].Id);
				TestTrue(Pair + TEXT(": same numbers"), With.Damage == Without.Damage && With.Pierce == Without.Pierce && With.Count == Without.Count &&
					With.MirrorDamageRatio == Without.MirrorDamageRatio && With.ForkRounds == Without.ForkRounds && With.ChainTriggerIntervalS == Without.ChainTriggerIntervalS);
				const uint16 Source = DTWeapons::ModifierSource(Mod);
				const FDTResolvedFlag* A = Without.Flags.FindByPredicate([Source](const FDTResolvedFlag& F) { return F.Entry.SourceId == Source; });
				const FDTResolvedFlag* B = With.Flags.FindByPredicate([Source](const FDTResolvedFlag& F) { return F.Entry.SourceId == Source; });
				TestEqual(Pair + TEXT(": same flag presence"), A != nullptr, B != nullptr);
				if (A && B)
				{
					bool bSame = A->Entry.Params.Nums.Num() == B->Entry.Params.Nums.Num();
					for (int32 Index = 0; bSame && Index < A->Entry.Params.Nums.Num(); ++Index)
					{
						bSame = A->Entry.Params.Nums[Index].Name == B->Entry.Params.Nums[Index].Name && A->Entry.Params.Nums[Index].Value == B->Entry.Params.Nums[Index].Value;
					}
					TestTrue(Pair + TEXT(": same parameters"), bSame);
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersCoverage, "Dusktracer.Modifiers.Coverage", DTWeaponTestHarness::TestFlags) // T-MOD-07
bool FDTModifiersCoverage::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		if (!IsS1Weapon(Weapon))
		{
			continue;
		}
		int32 Starter = 0, All = 0;
		for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
		{
			if (!DTModCompat::IsCompatible(Weapon, Mod))
			{
				continue;
			}
			++All;
			Starter += DTCat::Modifiers[Mod].bStarter ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("%s: %d starter modifiers"), DTCat::Weapons[Weapon].Id, Starter), Starter >= DTConst::MOD_MIN_COMPATIBLE);
		TestTrue(FString::Printf(TEXT("%s: %d S1 modifiers"), DTCat::Weapons[Weapon].Id, All), All >= DTConst::MOD_MIN_COMPATIBLE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersDormant, "Dusktracer.Modifiers.Dormant", DTWeaponTestHarness::TestFlags) // T-MOD-08
bool FDTModifiersDormant::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	TestEqual(TEXT("pierce socketed"), H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Pierce")), EDTWeaponResult::Ok);
	const int32 Raised = H.Weapons.ComposeSlot(H.Run, 0, 0).Pierce;
	TestTrue(TEXT("awake: pierce raised"), Raised > 1);
	// A ConvertMelee that converts every round removes Projectile: Pierce goes dormant, kept and inert.
	H.Weapons.AddFlagSource(0, TEXT("Test.Hammer"), KeeperFlag(DTCat::EFlag::ConvertMelee, EDTPath::Forge, [](FDTFlagParams& P)
	{
		P.Set(DTFlagParam::Rounds, 2.0f);
	}));
	H.StepOnce();
	const FDTWeaponInstance& Pulse = H.Run.GetPlayer(0).Weapons[0];
	const TArray<FDTWeaponDormancy> Slept = H.Weapons.TakeDormancyChanges();
	TestTrue(TEXT("the change is handed to telemetry"), Slept.Num() == 1 && Slept[0].OwnerId == 0 && Slept[0].Weapon == FName(TEXT("Weapon.PulseRifle"))
		&& Slept[0].Change.ModId == FName(TEXT("Mod.Pierce")) && Slept[0].Change.bDormant);
	H.StepOnce();
	TestEqual(TEXT("no change, no record"), H.Weapons.TakeDormancyChanges().Num(), 0);
	TestEqual(TEXT("kept in its socket"), Pulse.Sockets.Num(), 1);
	TestTrue(TEXT("dormant"), Pulse.Sockets.Num() == 1 && Pulse.Sockets[0].bDormant);
	TestEqual(TEXT("inert"), H.Weapons.GetLastSpec(0, 0).Pierce, 1);
	H.Weapons.RemoveFlagSource(0, TEXT("Test.Hammer"));
	H.StepOnce();
	TestFalse(TEXT("wakes when the conversion is gone"), H.Run.GetPlayer(0).Weapons[0].Sockets[0].bDormant);
	const TArray<FDTWeaponDormancy> Woke = H.Weapons.TakeDormancyChanges();
	TestTrue(TEXT("and that change too"), Woke.Num() == 1 && !Woke[0].Change.bDormant);
	TestEqual(TEXT("active again"), H.Weapons.GetLastSpec(0, 0).Pierce, Raised);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersSource, "Dusktracer.Modifiers.Source", DTWeaponTestHarness::TestFlags) // T-MOD-09
bool FDTModifiersSource::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const uint16 Weapon = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.PulseRifle"));
	const uint16 Split = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.SplitCore"));
	const uint16 Field = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.LingeringField"));
	FDTHarness H;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		H.AddEnemy(FVector2f(200.0f + 60.0f * Index, (Index % 2) * 50.0f - 25.0f));
	}
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.SplitCore"));
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.LingeringField"));
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Pierce"));
	H.StepMany(60);
	TestTrue(TEXT("weapon rounds keep Weapon.PulseRifle"), H.CountHitsFrom(Weapon) > 0);
	TestTrue(TEXT("split children carry Modifier.SplitCore"), H.CountHitsFrom(Split) > 0);
	TestTrue(TEXT("field ticks carry Modifier.LingeringField"), H.CountHitsFrom(Field) > 0);
	bool bPiercedWeapon = false;
	for (const FDTHit& Hit : H.HitLog)
	{
		bPiercedWeapon |= Hit.SourceId == Weapon && Hit.bPierce;
		TestTrue(TEXT("no Pierce source exists: pierce only reshapes"), FDTTelemetrySources::GetName(Hit.SourceId) != TEXT("Modifier.Pierce"));
	}
	TestTrue(TEXT("pierced hits keep the weapon source"), bPiercedWeapon);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersFreeVolleySource, "Dusktracer.Modifiers.FreeVolleySource", DTWeaponTestHarness::TestFlags) // T-MOD-09
bool FDTModifiersFreeVolleySource::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const uint16 Forge = DTWeapons::KeeperSource(EDTPath::Forge);
	const uint16 Chain = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.ChainTrigger"));
	FDTHarness H;
	H.bResolve = false;
	H.AddEnemy(FVector2f(200.0f, 0.0f));
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.ChainTrigger"));
	H.Weapons.AddFlagSource(0, TEXT("Test.Hammer"), KeeperFlag(DTCat::EFlag::ConvertMelee, EDTPath::Forge, [](FDTFlagParams& P)
	{
		P.Set(DTFlagParam::Rounds, 2.0f);
		P.Set(DTFlagParam::ArcDeg, 140.0f);
		P.Set(DTFlagParam::RadiusUu, 380.0f);
	}));
	H.StepOnce();
	const int32 OwnVolley = H.CountHitsFrom(Forge);
	TestTrue(TEXT("the weapon's own volley is the Keeper's cleave"), OwnVolley > 0);
	TestEqual(TEXT("nothing is the modifier's before a kill"), H.CountHitsFrom(Chain), 0);

	// A kill by the weapon fires the free volley; the conversion folds it into a cleave, and the cleave stays the modifier's.
	FDTKill Kill;
	Kill.OwnerId = 0;
	Kill.WeaponSlot = 0;
	H.PendingKills.Add(Kill);
	H.StepOnce();
	H.StepOnce();
	TestEqual(TEXT("one free cleave, one hit"), H.CountHitsFrom(Chain), 1);
	TestEqual(TEXT("the Keeper gains nothing from it"), H.CountHitsFrom(Forge), OwnVolley);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersDormantShown, "Dusktracer.Modifiers.DormantShown", DTWeaponTestHarness::TestFlags) // T-MOD-08
bool FDTModifiersDormantShown::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const FName Rifle(TEXT("Weapon.PulseRifle"));
	const FName Ring(TEXT("Weapon.RingEmitter"));
	const FName Shell(TEXT("Mod.ScatterShell"));
	const FName Split(TEXT("Mod.SplitCore"));
	const FDTOfferCard HammerKey = BlessingCard(TEXT("Blessing.Forge.Hammer.Key"));

	FDTHarness H;
	H.Weapons.Give(H.Run, 0, Rifle, 1); // count 2: Hammer's Key converts every round
	H.Weapons.Socket(H.Run, 0, 0, Shell);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.ChainTrigger"));
	H.StepOnce();
	FDTPlayerRunState& Player = H.Run.GetPlayer(0);
	TestEqual(TEXT("nothing to list while every socket is live"), DTWeaponCompose::DormantSockets(Player).Num(), 0);

	// The owner's run: the card names the socket the pick silences and leaves the one that still works.
	const FRandomStream Stream = Player.Offer.Rng;
	const TArray<FDTDormantSocket> Warned = DTOffers::DormantAfterPick(Player, HammerKey);
	TestEqual(TEXT("one socket would sleep"), Warned.Num(), 1);
	TestTrue(TEXT("Scatter Shell on the Pulse Rifle"), Warned.Num() == 1 && Warned[0].Mod == Shell && Warned[0].Weapon == Rifle);
	TestEqual(TEXT("reading the card takes nothing from the run"), Player.Keepers.Owned.Num(), 0);
	TestFalse(TEXT("reading the card leaves the sockets live"), Player.Weapons[0].Sockets[0].bDormant);
	TestEqual(TEXT("reading the card draws no number"), Player.Offer.Rng.GetCurrentSeed(), Stream.GetCurrentSeed());

	// Cards that cannot silence say nothing.
	FDTOfferCard Other = HammerKey;
	Other.ContentId = TEXT("Blessing.Forge.Furnace.Key");
	TestEqual(TEXT("a blessing with no conversion"), DTOffers::DormantAfterPick(Player, Other).Num(), 0);
	Other.Kind = EDTCardKind::WeaponUpgrade;
	Other.ContentId = Rifle;
	Other.TargetWeapon = Rifle;
	TestEqual(TEXT("an upgrade"), DTOffers::DormantAfterPick(Player, Other).Num(), 0);

	// A conversion that leaves a round flying still silences what blocks its cap: Split Core sleeps with a round in the air, and
	// Scatter Shell beside it keeps working. The answer is the composition's, not the round count's and not the card's flag.
	FDTHarness Partial;
	Partial.Weapons.Give(Partial.Run, 0, Rifle, 3); // count 3: Hammer's Key leaves one round
	Partial.Weapons.Socket(Partial.Run, 0, 0, Shell);
	Partial.Weapons.Socket(Partial.Run, 0, 0, Split);
	Partial.StepOnce();
	const TArray<FDTDormantSocket> PartialWarned = DTOffers::DormantAfterPick(Partial.Run.GetPlayer(0), HammerKey);
	TestEqual(TEXT("one round still flies: one socket would sleep"), PartialWarned.Num(), 1);
	TestTrue(TEXT("Split Core, which blocks the Melee the conversion adds"), PartialWarned.Num() == 1 && PartialWarned[0].Mod == Split);
	TestTrue(TEXT("the blessing is taken on the partial conversion"), TakeForgeBlessing(Partial, HammerKey));
	const TArray<FDTDormantSocket> PartialListed = DTWeaponCompose::DormantSockets(Partial.Run.GetPlayer(0));
	TestEqual(TEXT("the sim silences that one socket"), PartialListed.Num(), 1);
	TestTrue(TEXT("the same socket on the partial conversion"), PartialListed.Num() == 1 && PartialListed[0].Mod == Split);
	const FDTKeeperRunState& PartialKeepers = Partial.Run.GetPlayer(0).Keepers;
	FDTWeaponInstance Flying = Partial.Run.GetPlayer(0).Weapons[0];
	const FDTWeaponSpec FlyingSpec = DTWeaponCompose::Compose(Flying, DTKeepers::BuildKeeperFlags(PartialKeepers, EDTPath::Forge).Entries, FDTWeaponHeroStats(), 0);
	TestTrue(TEXT("a round does still fly"), FlyingSpec.ConvertRounds < FlyingSpec.Count && FlyingSpec.EffectiveCaps.Has(DTCat::ECap::Projectile));

	// An Intercession passes and moves no socket: Hammer's variant names no rounds and folds onto the Key's, so a cast changes no caps.
	FDTEffectSpec Cast;
	TestTrue(TEXT("Hammer's variant parses"), DTKeepers::ParseSpec(DTCat::Archetypes[DTOffers::FindBlessing(HammerKey.ContentId).Archetype].IntercessionVariant, Cast));
	FDTWeaponInstance During = Partial.Run.GetPlayer(0).Weapons[0];
	const FDTWeaponSpec CastSpec = DTWeaponCompose::Compose(During, DTKeepers::BuildKeeperFlags(PartialKeepers, EDTPath::Forge, Cast.Clauses).Entries, FDTWeaponHeroStats(), 0);
	TestEqual(TEXT("a cast folds as many rounds as the Key"), CastSpec.ConvertRounds, FlyingSpec.ConvertRounds);
	TestEqual(TEXT("a cast changes no caps"), CastSpec.EffectiveCaps.Mask, FlyingSpec.EffectiveCaps.Mask);
	TestEqual(TEXT("a cast wakes and silences nothing"), During.Sockets[0].bDormant, Flying.Sockets[0].bDormant);
	TestEqual(TEXT("a cast wakes and silences nothing either way"), During.Sockets[1].bDormant, Flying.Sockets[1].bDormant);

	// The worst legal card: a whole conversion takes three sockets of the Rifle, and Split Core sleeps on the Ring Emitter's twelve rounds.
	FDTHarness Worst;
	Worst.Weapons.Give(Worst.Run, 0, Rifle, 2);
	for (const FName Mod : { Shell, Split, FName(TEXT("Mod.Pierce")) })
	{
		Worst.Weapons.Socket(Worst.Run, 0, 0, Mod);
	}
	Worst.Weapons.Give(Worst.Run, 0, Ring, 1);
	Worst.Weapons.Socket(Worst.Run, 0, 1, Split);
	Worst.StepOnce();
	const TArray<FDTDormantSocket> WorstWarned = DTOffers::DormantAfterPick(Worst.Run.GetPlayer(0), HammerKey);
	TestEqual(TEXT("four sockets would sleep"), WorstWarned.Num(), 4);
	TestTrue(TEXT("the last is on the Ring Emitter"), WorstWarned.Num() == 4 && WorstWarned[3].Mod == Split && WorstWarned[3].Weapon == Ring);
	TestTrue(TEXT("the blessing is taken on the worst card"), TakeForgeBlessing(Worst, HammerKey));
	TestEqual(TEXT("the sim silences all four"), DTWeaponCompose::DormantSockets(Worst.Run.GetPlayer(0)).Num(), 4);

	// The pick itself: the sim agrees with the card, the HUD list names it, and a socket already asleep is not warned of again.
	TestTrue(TEXT("the blessing is taken"), TakeForgeBlessing(H, HammerKey));
	const TArray<FDTDormantSocket> Listed = DTWeaponCompose::DormantSockets(H.Run.GetPlayer(0));
	TestEqual(TEXT("the HUD lists what the card named"), Listed.Num(), 1);
	TestTrue(TEXT("the same socket"), Listed.Num() == 1 && Listed[0].Mod == Shell && Listed[0].Weapon == Rifle);
	FDTOfferCard Amp = HammerKey;
	Amp.ContentId = TEXT("Blessing.Forge.Hammer.Amp1");
	TestEqual(TEXT("already asleep: no warning"), DTOffers::DormantAfterPick(H.Run.GetPlayer(0), Amp).Num(), 0);

	// Both lines carry both names, in the running culture.
	FDTOfferCard WeaponCard;
	WeaponCard.Kind = EDTCardKind::Weapon;
	WeaponCard.ContentId = Rifle;
	FDTOfferCard ModCard;
	ModCard.Kind = EDTCardKind::Mod;
	ModCard.ContentId = Shell;
	for (const TCHAR* Key : { TEXT("Dormant.Hud"), TEXT("Dormant.Card") })
	{
		const FString Line = DTOfferText::Dormant(Key, FDTDormantSocket{ Rifle, Shell }).ToString();
		TestTrue(FString(Key) + TEXT(" names the weapon"), Line.Contains(DTOfferText::Name(WeaponCard).ToString()));
		TestTrue(FString(Key) + TEXT(" names the modifier"), Line.Contains(DTOfferText::Name(ModCard).ToString()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersDormantTable, "Dusktracer.Modifiers.DormantTable", DTWeaponTestHarness::TestFlags) // T-MOD-08
bool FDTModifiersDormantTable::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const FDTOfferCard HammerKey = BlessingCard(TEXT("Blessing.Forge.Hammer.Key"));
	const FName Rifle(TEXT("Weapon.PulseRifle"));
	const FName Ring(TEXT("Weapon.RingEmitter"));
	const FName Split(TEXT("Mod.SplitCore"));
	// By hand from R-WPN-10 and the catalogs: a whole conversion loses Projectile, so these four go.
	const TSet<FName> NeedRounds = { Split, TEXT("Mod.ScatterShell"), TEXT("Mod.Pierce"), TEXT("Mod.Ricochet") };

	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		if (!IsS1Weapon(Weapon))
		{
			continue;
		}
		const FName WeaponName = DTWeapons::WeaponId(Weapon);
		for (int32 Level = 1; Level <= DTWeapons::GetMaxLevel(Weapon); ++Level)
		{
			for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
			{
				if (!DTModCompat::IsCompatible(Weapon, Mod))
				{
					continue;
				}
				const FName ModName = DTWeapons::ModifierId(Mod);
				const FString Cell = FString::Printf(TEXT("%s L%d x %s"), DTCat::Weapons[Weapon].Id, Level, DTCat::Modifiers[Mod].Id);
				// Only what leads with Projectile converts: the Rifle and the Ring. The Key folds two rounds, the whole of a count of 2.
				const bool bWhole = WeaponName == Rifle && DTWeapons::GetLevelRow(Weapon, Level)->Count <= 2;
				const bool bSilenced = bWhole ? NeedRounds.Contains(ModName) : (WeaponName == Rifle || WeaponName == Ring) && ModName == Split;

				FDTHarness H;
				H.Weapons.Give(H.Run, 0, WeaponName, Level);
				H.Weapons.Socket(H.Run, 0, 0, ModName);
				H.StepOnce();
				const int32 Warned = DTOffers::DormantAfterPick(H.Run.GetPlayer(0), HammerKey).Num();
				TakeForgeBlessing(H, HammerKey);
				const int32 Listed = DTWeaponCompose::DormantSockets(H.Run.GetPlayer(0)).Num();
				TestEqual(Cell + TEXT(": the card reads the table"), Warned, bSilenced ? 1 : 0);
				TestEqual(Cell + TEXT(": the sim silences what the card named"), Listed, Warned);

				// Only a blessing silences: a level can give a weapon Projectile back, and no modifier is blocked by it.
				if (Listed == 0 && Level < DTWeapons::GetMaxLevel(Weapon))
				{
					TestEqual(Cell + TEXT(": the level is taken"), H.Weapons.LevelUp(H.Run, 0, 0, Level + 1), EDTWeaponResult::Ok);
					H.StepOnce();
					TestEqual(Cell + TEXT(": a level silences nothing"), DTWeaponCompose::DormantSockets(H.Run.GetPlayer(0)).Num(), 0);
				}
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
