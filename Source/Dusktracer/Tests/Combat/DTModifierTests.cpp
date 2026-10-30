// Modifier tests (02_Systems/modifiers.md T-MOD-01..09).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Modifiers; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponCompose.h"
#include "Core/DTCatalogGen.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	TestEqual(TEXT("kept in its socket"), Pulse.Sockets.Num(), 1);
	TestTrue(TEXT("dormant"), Pulse.Sockets.Num() == 1 && Pulse.Sockets[0].bDormant);
	TestEqual(TEXT("inert"), H.Weapons.GetLastSpec(0, 0).Pierce, 1);
	H.Weapons.RemoveFlagSource(0, TEXT("Test.Hammer"));
	H.StepOnce();
	TestFalse(TEXT("wakes when the conversion is gone"), H.Run.GetPlayer(0).Weapons[0].Sockets[0].bDormant);
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

#endif // WITH_DEV_AUTOMATION_TESTS
