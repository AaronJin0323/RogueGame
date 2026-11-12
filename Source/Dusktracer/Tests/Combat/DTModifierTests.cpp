// Modifier tests (02_Systems/modifiers.md T-MOD-01..10).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Modifiers; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Combat/DTReforge.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersRoundsTable, "Dusktracer.Modifiers.RoundsTable", DTWeaponTestHarness::TestFlags) // T-OFF-13
bool FDTModifiersRoundsTable::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const FDTOfferCard HammerKey = BlessingCard(TEXT("Blessing.Forge.Hammer.Key"));
	const FName Rifle(TEXT("Weapon.PulseRifle"));
	const FName Ring(TEXT("Weapon.RingEmitter"));
	const int32 Folds = 2; // by hand from expression.csv: the Key's rounds=2
	double CardMult = 0.0; // {cleave_mult} of the Key's rules row, from the same two numbers the text formats
	const FDTBlessingInfo KeyInfo = DTOffers::FindBlessing(HammerKey.ContentId);
	for (const TPair<FString, double>& Param : DTOffers::SpecParams(DTCat::Archetypes[KeyInfo.Archetype].Effects[KeyInfo.Column]))
	{
		if (Param.Key == TEXT("dmg_mult"))
		{
			CardMult = Param.Value * DTConst::WEAPON_CONVERT_MELEE_DAMAGE_RATIO;
		}
	}
	TestTrue(TEXT("the Key names a multiplier"), CardMult > 0.0);

	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		if (!IsS1Weapon(Weapon))
		{
			continue;
		}
		const FName WeaponName = DTWeapons::WeaponId(Weapon);
		for (int32 Level = 1; Level <= DTWeapons::GetMaxLevel(Weapon); ++Level)
		{
			const FString Cell = FString::Printf(TEXT("%s L%d"), DTCat::Weapons[Weapon].Id, Level);
			FDTHarness H;
			H.Weapons.Give(H.Run, 0, WeaponName, Level);
			H.StepOnce();
			FDTPlayerRunState& Player = H.Run.GetPlayer(0);
			const FRandomStream Stream = Player.Offer.Rng;
			const TArray<FDTRoundsLeft> Left = DTOffers::RoundsAfterPick(Player, HammerKey);
			TestEqual(Cell + TEXT(": reading the card takes nothing from the run"), Player.Keepers.Owned.Num(), 0);
			TestEqual(Cell + TEXT(": reading the card draws no number"), Player.Offer.Rng.GetCurrentSeed(), Stream.GetCurrentSeed());

			// Only what leads with Projectile converts: on S1 the Rifle and the Ring, with the S2 arsenal every weapon whose matrix row
			// takes ConvertMelee natively. The others lose no round, so the card names none.
			const bool bConverts = DTWeapons::ResolveFlag(DTWeapons::DeclaredCaps(Weapon), DTCat::EFlag::ConvertMelee).Compat == DTCat::ECompat::Native;
			TestTrue(Cell + TEXT(": the Rifle and the Ring convert"), bConverts || (WeaponName != Rifle && WeaponName != Ring));
			if (!TestEqual(Cell + TEXT(": the card reads the table"), Left.Num(), bConverts ? 1 : 0) || !bConverts)
			{
				continue;
			}
			const int32 Count = DTWeapons::GetLevelRow(Weapon, Level)->Count;
			TestEqual(Cell + TEXT(": the weapon's own name"), Left[0].Weapon, WeaponName);
			TestEqual(Cell + TEXT(": the weapon's rounds at this level"), Left[0].Count, Count);
			TestEqual(Cell + TEXT(": what the Key leaves"), Left[0].Kept, Count - FMath::Min(Folds, Count));

			// The sim fires what the card said.
			TestTrue(Cell + TEXT(": the blessing is taken"), TakeForgeBlessing(H, HammerKey));
			FDTWeaponInstance Held = H.Run.GetPlayer(0).Weapons[0];
			const FDTWeaponSpec Spec = DTWeaponCompose::Compose(Held, DTKeepers::BuildKeeperFlags(H.Run.GetPlayer(0).Keepers, EDTPath::Forge).Entries, FDTWeaponHeroStats(), 0);
			TestEqual(Cell + TEXT(": the sim leaves what the card named"), Spec.Count - Spec.ConvertRounds, Left[0].Kept);
			if (DTWeapons::DeclaredCaps(Weapon).Has(DTCat::ECap::Projectile)) // a Lob carrier never had Projectile to lose
			{
				TestEqual(Cell + TEXT(": no carrier left when none fly"), Spec.EffectiveCaps.Has(DTCat::ECap::Projectile), Left[0].Kept > 0);
			}
			TestEqual(Cell + TEXT(": the multiplier on the card is the one the sim deals"), static_cast<double>(Spec.ConvertDamageFactor), CardMult, 1e-4);

			// Taken once it says nothing more, and the rest of Hammer never names rounds.
			TestEqual(Cell + TEXT(": held already: no warning"), DTOffers::RoundsAfterPick(H.Run.GetPlayer(0), HammerKey).Num(), 0);
			TestEqual(Cell + TEXT(": an amplifier converts no more"), DTOffers::RoundsAfterPick(H.Run.GetPlayer(0), BlessingCard(TEXT("Blessing.Forge.Hammer.Amp2"))).Num(), 0);
		}
	}

	// Cards that convert nothing say nothing, and the words follow the numbers: both rows carry the weapon, Some its numbers.
	FDTHarness Plain;
	Plain.Weapons.Give(Plain.Run, 0, Rifle, 1);
	Plain.StepOnce();
	TestEqual(TEXT("a blessing with no conversion"), DTOffers::RoundsAfterPick(Plain.Run.GetPlayer(0), BlessingCard(TEXT("Blessing.Forge.Furnace.Key"))).Num(), 0);
	FDTOfferCard Upgrade = HammerKey;
	Upgrade.Kind = EDTCardKind::WeaponUpgrade;
	Upgrade.ContentId = Rifle;
	Upgrade.TargetWeapon = Rifle;
	TestEqual(TEXT("an upgrade"), DTOffers::RoundsAfterPick(Plain.Run.GetPlayer(0), Upgrade).Num(), 0);

	FDTOfferCard WeaponCard;
	WeaponCard.Kind = EDTCardKind::Weapon;
	WeaponCard.ContentId = Rifle;
	const FString Name = DTOfferText::Name(WeaponCard).ToString();
	const FString None = DTOfferText::RoundsLeft(FDTRoundsLeft{ Rifle, 0, 2 }).ToString();
	const FString Some = DTOfferText::RoundsLeft(FDTRoundsLeft{ Rifle, 1, 3 }).ToString();
	TestTrue(TEXT("none flying names the weapon"), None.Contains(Name));
	TestTrue(TEXT("some flying names the weapon"), Some.Contains(Name));
	TestTrue(TEXT("some flying says how many are kept"), Some.Contains(TEXT("1")));
	TestTrue(TEXT("some flying says of how many"), Some.Contains(TEXT("3")));
	TestNotEqual(TEXT("the two cases are two sentences"), None, Some);
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
	// By hand from R-WPN-10 and the catalogs: a whole conversion loses the carrier cap, so these go.
	const TSet<FName> NeedRounds = { Split, TEXT("Mod.ScatterShell"), TEXT("Mod.Pierce"), TEXT("Mod.Ricochet"), TEXT("Mod.Homing"), TEXT("Mod.OrbitLock"), TEXT("Mod.Boomerang") };
	// ConvertMelee is native on these; on Orbital Drones, Arc Blade and Gravity Mine it resolves as a fallback and changes no caps.
	const TSet<FName> Converts = { Rifle, Ring, TEXT("Weapon.ScatterCannon"), TEXT("Weapon.RailLance"), TEXT("Weapon.FlakMortar"), TEXT("Weapon.SeekerSwarm"), TEXT("Weapon.SpiralProjector") };

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
				// The Key folds two rounds, the whole of a count of 2 or less.
				const bool bConverts = Converts.Contains(WeaponName);
				const bool bWhole = bConverts && DTWeapons::GetLevelRow(Weapon, Level)->Count <= 2;
				const bool bSilenced = bWhole ? NeedRounds.Contains(ModName) : bConverts && ModName == Split;

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

namespace DTChainTriggerTest
{
	using namespace DTWeaponTestHarness;

	static const TCHAR* const ChainWeapons[] = { TEXT("Weapon.PulseRifle"), TEXT("Weapon.ScatterCannon"), TEXT("Weapon.OrbitalDrones"), TEXT("Weapon.RailLance"),
		TEXT("Weapon.FlakMortar"), TEXT("Weapon.ArcBlade"), TEXT("Weapon.SeekerSwarm"), TEXT("Weapon.RingEmitter"), TEXT("Weapon.SpiralProjector"), TEXT("Weapon.GravityMine") };

	struct FChainReading
	{
		EDTWeaponResult Socketed = EDTWeaponResult::Ok;
		bool bDormant = false;
		int32 ChainHits = 0;
		float ChainDamage = 0.0f;
		float Total = 0.0f;
		int64 Kills = 0;
		int64 Breaches = 0;
	};

	/** The weapon at level 8, evolved or not, with Chain Trigger socketed or not. */
	static void GiveChained(FDTHarness& H, const TCHAR* Weapon, bool bEvolved, bool bChain, FChainReading& Out)
	{
		H.Weapons.Give(H.Run, 0, FName(Weapon), 8);
		FDTWeaponInstance& Owned = H.Run.GetPlayer(0).Weapons[0];
		Owned.Evolve = bEvolved ? DTReforge::FindEvolve(Owned.Def) : Owned.Evolve;
		if (bChain)
		{
			Out.Socketed = H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.ChainTrigger"));
		}
	}

	/** Rings of enemies where every pattern reaches one: a sweep's reach, the drone orbit, a ring's band. */
	static void AddCrowd(FDTHarness& H, float Hp)
	{
		for (const float Radius : { 120.0f, static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU), 300.0f, 560.0f })
		{
			for (int32 Index = 0; Index < 24; ++Index)
			{
				H.AddEnemy(FVector2f(Radius, 0.0f).GetRotated(15.0f * static_cast<float>(Index)), Hp);
			}
		}
	}

	static void Read(const FDTHarness& H, FChainReading& Out)
	{
		const uint16 Chain = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.ChainTrigger"));
		for (const FDTHit& Hit : H.HitLog)
		{
			Out.ChainHits += Hit.SourceId == Chain ? 1 : 0;
			Out.ChainDamage += Hit.SourceId == Chain ? Hit.Damage : 0.0f;
			Out.Total += Hit.Damage;
		}
		Out.Kills = H.Enemies.GetTotalKills();
	}

	/**
	 * One kill by the weapon, handed in a second after its first volley, so a ring, a beam or a mine of its own lives as it does in play.
	 * Nothing dies and the weapon's own cadence runs on. bMove walks the hero back and forth, which Cyclone needs to turn at all.
	 */
	static FChainReading OneKill(const TCHAR* Weapon, bool bEvolved, bool bKill, bool bMove)
	{
		FChainReading Out;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, Weapon, bEvolved, true, Out);
		AddCrowd(H, ToughHp);
		const int32 KillStep = DTConst::SIM_FIXED_HZ;
		for (int32 Step = 0; Step < KillStep + 6 * DTConst::SIM_FIXED_HZ; ++Step)
		{
			if (bMove)
			{
				H.View.HeroPositions[0] += FVector2f(Step % 2 == 0 ? 4.0f : -4.0f, 0.0f);
			}
			if (bKill && Step == KillStep)
			{
				FDTKill Kill;
				Kill.OwnerId = 0;
				Kill.WeaponSlot = 0;
				H.PendingKills.Add(Kill);
			}
			H.StepOnce();
		}
		const FDTWeaponInstance& Owned = H.Run.GetPlayer(0).Weapons[0];
		Out.bDormant = Owned.Sockets.Num() > 0 && Owned.Sockets[0].bDormant;
		Read(H, Out);
		return Out;
	}

	/**
	 * Thirty seconds of a crowd that walks in and dies: six enemies a second from 750 uu at 100 uu/s, kills resolved by the pool, pushes applied,
	 * so the modifier triggers as it does in play. Breaches are enemy-steps spent within 150 uu of the hero.
	 */
	static FChainReading March(const TCHAR* Weapon, bool bEvolved, bool bChain, bool bMove, float Hp)
	{
		FChainReading Out;
		FDTHarness H;
		GiveChained(H, Weapon, bEvolved, bChain, Out);
		int32 Spawned = 0;
		for (int32 Step = 0; Step < 30 * DTConst::SIM_FIXED_HZ; ++Step)
		{
			if (Step % (DTConst::SIM_FIXED_HZ / 6) == 0)
			{
				H.AddEnemy(FVector2f(750.0f, 0.0f).GetRotated(137.5f * static_cast<float>(Spawned++)), Hp);
			}
			if (bMove)
			{
				H.View.HeroPositions[0] += FVector2f(Step % 2 == 0 ? 4.0f : -4.0f, 0.0f);
			}
			H.StepOnce();
			for (const FDTDisplacement& Move : H.View.Displacements)
			{
				if (H.Enemies.IsHandleValid(FDTEnemyHandle{ Move.EnemyIndex, Move.EnemySerial }))
				{
					H.Enemies.SetPosition(Move.EnemyIndex, H.Enemies.GetPositions()[Move.EnemyIndex] + Move.Delta);
				}
			}
			H.View.Displacements.Reset();
			for (int32 Enemy = 0; Enemy < H.Enemies.Num(); ++Enemy)
			{
				const FVector2f At = H.Enemies.GetPositions()[Enemy];
				const float Dist = At.Size();
				Out.Breaches += Dist < 150.0f ? 1 : 0;
				if (Dist > 40.0f)
				{
					H.Enemies.SetPosition(Enemy, At - At / Dist * 100.0f * Dt);
				}
			}
		}
		Read(H, Out);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerEveryWeapon, "Dusktracer.Modifiers.ChainTrigger.EveryWeapon", DTWeaponTestHarness::TestFlags) // T-MOD-16
bool FDTModifiersChainTriggerEveryWeapon::RunTest(const FString& Parameters)
{
	// The card says a kill by the weapon fires one free volley. On every weapon, plain and evolved, that kill must add damage.
	using namespace DTChainTriggerTest;
	for (const TCHAR* Name : ChainWeapons)
	{
		for (int32 Evolved = 0; Evolved < 2; ++Evolved)
		{
			const bool bMove = Evolved == 1 && FCString::Strcmp(Name, TEXT("Weapon.ArcBlade")) == 0;
			const FChainReading Without = OneKill(Name, Evolved == 1, false, bMove);
			const FChainReading With = OneKill(Name, Evolved == 1, true, bMove);
			const FString What = FString::Printf(TEXT("%s%s"), Name, Evolved ? TEXT(" evolved") : TEXT(""));
			AddInfo(FString::Printf(TEXT("READ one kill | %s | own %.1f | with %.1f | chain hits %d | chain damage %.1f"), *What, Without.Total, With.Total, With.ChainHits, With.ChainDamage));
			TestEqual(*FString::Printf(TEXT("%s: the socket takes Chain Trigger"), *What), With.Socketed, EDTWeaponResult::Ok);
			TestFalse(*FString::Printf(TEXT("%s: the socket is not dormant"), *What), With.bDormant);
			TestTrue(*FString::Printf(TEXT("%s: a kill's free volley adds damage"), *What), With.Total > Without.Total);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerMarch, "Dusktracer.Modifiers.ChainTrigger.March", DTWeaponTestHarness::TestFlags) // T-MOD-16
bool FDTModifiersChainTriggerMarch::RunTest(const FString& Parameters)
{
	// Against a crowd that walks in and dies, the modifier never makes its weapon kill less.
	using namespace DTChainTriggerTest;
	for (const TCHAR* Name : ChainWeapons)
	{
		for (int32 Evolved = 0; Evolved < 2; ++Evolved)
		{
			const bool bMove = Evolved == 1 && FCString::Strcmp(Name, TEXT("Weapon.ArcBlade")) == 0;
			for (const float Hp : { 60.0f, 240.0f })
			{
				const FChainReading Without = March(Name, Evolved == 1, false, bMove, Hp);
				const FChainReading With = March(Name, Evolved == 1, true, bMove, Hp);
				const FString What = FString::Printf(TEXT("%s%s hp %.0f"), Name, Evolved ? TEXT(" evolved") : TEXT(""), Hp);
				AddInfo(FString::Printf(TEXT("READ march | %s | kills %lld -> %lld | damage %.0f -> %.0f | breaches %lld -> %lld | chain hits %d | chain damage %.0f"), *What,
					Without.Kills, With.Kills, Without.Total, With.Total, Without.Breaches, With.Breaches, With.ChainHits, With.ChainDamage));
				TestTrue(*FString::Printf(TEXT("%s: no fewer kills with Chain Trigger"), *What), With.Kills >= Without.Kills);
			}
		}
	}
	return true;
}

namespace DTChainTriggerTest
{
	static void AddKill(FDTHarness& H)
	{
		FDTKill Kill;
		Kill.OwnerId = 0;
		Kill.WeaponSlot = 0;
		H.PendingKills.Add(Kill);
	}

	static int32 HitsOn(const FDTHarness& H, uint32 Serial, uint16 Source, int32 From = 0)
	{
		int32 Count = 0;
		for (int32 Index = From; Index < H.HitLog.Num(); ++Index)
		{
			Count += H.HitLog[Index].EnemySerial == Serial && H.HitLog[Index].SourceId == Source ? 1 : 0;
		}
		return Count;
	}

	/** A tough enemy, by serial. */
	static uint32 AddSerial(FDTHarness& H, const FVector2f& At)
	{
		const int32 Enemy = H.AddEnemy(At);
		return H.Enemies.GetSerials()[Enemy];
	}

	static uint16 ChainSource() { return FDTTelemetrySources::FindOrAdd(TEXT("Modifier.ChainTrigger")); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerBladeHalo, "Dusktracer.Modifiers.ChainTrigger.BladeHalo", DTWeaponTestHarness::TestFlags) // T-MOD-16, D-039
bool FDTModifiersChainTriggerBladeHalo::RunTest(const FString& Parameters)
{
	// Blade Halo keeps its bolts (R-RFG-07, D-079): Chain Trigger's free volley fires the bolts and every blade strikes once where it is, as the modifier's.
	using namespace DTChainTriggerTest;
	const uint16 Chain = ChainSource();
	const float Orbit = static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU);
	int32 OwnHits[2] = { 0, 0 };
	for (int32 Killed = 0; Killed < 2; ++Killed)
	{
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.OrbitalDrones"), true, true, Unused);
		// A full ring on the orbit, so every blade touches some wherever the turn has carried it.
		for (int32 Index = 0; Index < 36; ++Index)
		{
			H.AddEnemy(FVector2f(Orbit, 0.0f).GetRotated(10.0f * static_cast<float>(Index)));
		}
		const uint16 Own = H.Run.GetPlayer(0).Weapons[0].SourceId;
		int32 MostPerTouch = 0; // the most enemies the weapon's own strikes of one step reach on this ring (every blade on a tick, R-RFG-07)
		for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ + DTConst::SIM_FIXED_HZ / 2; ++Step)
		{
			if (Killed == 1 && Step == DTConst::SIM_FIXED_HZ / 2)
			{
				AddKill(H);
			}
			const int32 From = H.HitLog.Num();
			H.StepOnce();
			int32 Touch = 0;
			for (int32 Index = From; Index < H.HitLog.Num(); ++Index)
			{
				Touch += H.HitLog[Index].SourceId == Own ? 1 : 0;
			}
			MostPerTouch = FMath::Max(MostPerTouch, Touch);
		}
		const FDTWeaponSpec& Spec = H.Weapons.GetLastSpec(0, 0);
		const int32 Blades = FMath::RoundToInt(DTReforge::Param(Spec.EvolveRow, TEXT("blades")));
		const float Blade = Spec.Damage * DTReforge::Param(Spec.EvolveRow, TEXT("dmg_ratio"));
		int32 ChainHits = 0;
		int32 ChainStrikes = 0;
		int32 ChainBolts = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			OwnHits[Killed] += Hit.SourceId == Own ? 1 : 0;
			ChainHits += Hit.SourceId == Chain ? 1 : 0;
			if (Hit.SourceId == Chain)
			{
				const bool bStrike = FMath::IsNearlyEqual(Hit.Damage, Blade, 0.01f);
				const bool bBolt = FMath::IsNearlyEqual(Hit.Damage, Spec.Damage, 0.01f);
				TestTrue(TEXT("each hit is a blade's strike or a bolt, with no multiplier"), bStrike || bBolt);
				ChainStrikes += bStrike ? 1 : 0;
				ChainBolts += bBolt ? 1 : 0;
			}
		}
		if (Killed == 1)
		{
			TestTrue(TEXT("every blade strikes once as Modifier.ChainTrigger"), ChainStrikes >= Blades);
			TestTrue(TEXT("once each: no more than one touch of every blade"), ChainStrikes <= Blades * MostPerTouch);
			TestTrue(TEXT("and its bolts fire as the modifier's"), ChainBolts > 0);
		}
		else
		{
			TestEqual(TEXT("without a kill nothing is the modifier's"), ChainHits, 0);
		}
	}
	TestEqual(TEXT("the free volley moves no contact tick: the weapon's own strikes are unchanged"), OwnHits[1], OwnHits[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerPulseWall, "Dusktracer.Modifiers.ChainTrigger.PulseWall", DTWeaponTestHarness::TestFlags) // T-MOD-16, D-039
bool FDTModifiersChainTriggerPulseWall::RunTest(const FString& Parameters)
{
	// One ring lives per weapon (R-RFG-12). Chain Trigger's free volley opens no second ring and does not start the live one again:
	// the ring strikes what stands inside it once more where it stands, as the modifier's (D-070).
	using namespace DTChainTriggerTest;
	const uint16 Chain = ChainSource();
	int32 OwnRim[2] = { 0, 0 };
	for (int32 Killed = 0; Killed < 2; ++Killed)
	{
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.RingEmitter"), true, true, Unused);
		const int32 Row = H.Run.GetPlayer(0).Weapons[0].Evolve;
		const float Radius = DTReforge::Param(Row, TEXT("radius_uu"));
		const uint32 Rim = AddSerial(H, FVector2f(Radius - 40.0f, 0.0f));
		const uint32 Inner = AddSerial(H, FVector2f(0.0f, 0.4f * Radius));
		H.StepMany(DTConst::SIM_FIXED_HZ); // the ring has grown and holds
		const uint16 Own = H.Run.GetPlayer(0).Weapons[0].SourceId;
		TestEqual(TEXT("the weapon's ring lives"), H.Weapons.GetLiveFields(), 1);
		TestTrue(TEXT("and strikes the rim as the weapon"), HitsOn(H, Rim, Own) > 0);
		const int32 Before = H.HitLog.Num();
		if (Killed == 1)
		{
			AddKill(H);
		}
		H.StepMany(2);
		if (Killed == 1)
		{
			TestEqual(TEXT("the rim is struck once more, as Modifier.ChainTrigger"), HitsOn(H, Rim, Chain, Before), 1);
			const float Tick = DTReforge::Param(Row, TEXT("dps")) * H.Weapons.GetLastSpec(0, 0).Damage * DTReforge::Param(Row, TEXT("tick_s"));
			for (int32 Index = Before; Index < H.HitLog.Num(); ++Index)
			{
				if (H.HitLog[Index].SourceId == Chain)
				{
					TestNearlyEqual(TEXT("for one tick of the row, with no multiplier"), H.HitLog[Index].Damage, Tick, 0.01f);
				}
			}
		}
		H.StepMany(DTConst::SIM_FIXED_HZ);
		TestEqual(TEXT("one ring, never two"), H.Weapons.GetLiveFields(), 1);
		TestEqual(TEXT("one ring was ever opened"), H.Weapons.GetFieldsOpened(), 1);
		TestEqual(TEXT("one kill is one strike"), HitsOn(H, Rim, Chain, Before), Killed);
		// D-070: the whole disc strikes, so the free volley's one more strike reaches the inside too, once per kill.
		OwnRim[Killed] = HitsOn(H, Rim, Own, Before);
	}
	TestTrue(TEXT("the wall holds and ticks on"), OwnRim[0] > 0);
	TestEqual(TEXT("the ring's own ticks are not spent"), OwnRim[1], OwnRim[0]);
	{
		// With no ring live the free volley opens one, and that ring is the modifier's.
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.RingEmitter"), true, true, Unused);
		const int32 Row = H.Run.GetPlayer(0).Weapons[0].Evolve;
		const uint32 Rim = AddSerial(H, FVector2f(DTReforge::Param(Row, TEXT("radius_uu")) - 40.0f, 0.0f));
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 60.0f;
		H.StepOnce();
		TestEqual(TEXT("no ring yet"), H.Weapons.GetLiveFields(), 0);
		AddKill(H);
		H.StepMany(2 * DTConst::SIM_FIXED_HZ);
		TestEqual(TEXT("with none live, the free volley opens one"), H.Weapons.GetLiveFields(), 1);
		TestTrue(TEXT("and it strikes as Modifier.ChainTrigger"), HitsOn(H, Rim, Chain) > 0);
		TestEqual(TEXT("the weapon has none of it"), HitsOn(H, Rim, H.Run.GetPlayer(0).Weapons[0].SourceId), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerSweepBeam, "Dusktracer.Modifiers.ChainTrigger.SweepBeam", DTWeaponTestHarness::TestFlags) // T-MOD-16, D-039
bool FDTModifiersChainTriggerSweepBeam::RunTest(const FString& Parameters)
{
	// One beam lives per weapon (R-RFG-08). Chain Trigger's free volley does not start it again: it is one tick of the beam along its own aim.
	using namespace DTChainTriggerTest;
	const uint16 Chain = ChainSource();
	int32 OwnHits[2] = { 0, 0 };
	int32 FirstHitStep = INDEX_NONE;
	for (int32 Killed = 0; Killed < 2; ++Killed)
	{
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.RailLance"), true, true, Unused);
		const int32 Row = H.Run.GetPlayer(0).Weapons[0].Evolve;
		const uint32 Centre = AddSerial(H, FVector2f(500.0f, 0.0f));
		const uint16 Own = H.Run.GetPlayer(0).Weapons[0].SourceId;
		const int32 Steps = FMath::RoundToInt(DTReforge::Param(Row, TEXT("beam_s")) / Dt) + 30;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			if (Killed == 1 && Step == FirstHitStep / 2)
			{
				AddKill(H); // the kill lands while the live beam is still short of the enemy
			}
			H.StepOnce();
			H.Run.GetPlayer(0).Weapons[0].Cooldown = 60.0f; // one beam of the weapon's own
			if (Killed == 0 && FirstHitStep == INDEX_NONE && HitsOn(H, Centre, Own) > 0)
			{
				FirstHitStep = Step;
			}
		}
		OwnHits[Killed] = HitsOn(H, Centre, Own);
		if (Killed == 0)
		{
			TestTrue(TEXT("the weapon's beam crosses the enemy"), FirstHitStep > 0 && OwnHits[0] > 0);
			TestEqual(TEXT("without a kill nothing is the modifier's"), HitsOn(H, Centre, Chain), 0);
		}
		else
		{
			TestEqual(TEXT("one tick along the densest line, as Modifier.ChainTrigger"), HitsOn(H, Centre, Chain), 1);
			for (const FDTHit& Hit : H.HitLog)
			{
				if (Hit.SourceId == Chain)
				{
					TestEqual(TEXT("for one tick of the beam, with no multiplier"), Hit.Damage, H.Weapons.GetLastSpec(0, 0).Damage);
				}
			}
		}
	}
	TestEqual(TEXT("the beam did not start again: the weapon's sweep runs on to its end"), OwnHits[1], OwnHits[0]);
	{
		// With no beam live the free volley starts one, and that beam is the modifier's (R-RFG-08).
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.RailLance"), true, true, Unused);
		const int32 Row = H.Run.GetPlayer(0).Weapons[0].Evolve;
		const uint32 Centre = AddSerial(H, FVector2f(500.0f, 0.0f));
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 60.0f;
		H.StepOnce();
		TestEqual(TEXT("no beam yet"), H.HitLog.Num(), 0);
		AddKill(H);
		H.StepMany(FMath::RoundToInt(DTReforge::Param(Row, TEXT("beam_s")) / Dt) + 30);
		TestTrue(TEXT("with none live, the free volley starts a beam as Modifier.ChainTrigger"), HitsOn(H, Centre, Chain) > 0);
		TestEqual(TEXT("the weapon has none of it"), HitsOn(H, Centre, H.Run.GetPlayer(0).Weapons[0].SourceId), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerCyclone, "Dusktracer.Modifiers.ChainTrigger.Cyclone", DTWeaponTestHarness::TestFlags) // T-MOD-16, D-039
bool FDTModifiersChainTriggerCyclone::RunTest(const FString& Parameters)
{
	// Cyclone keeps its rule (R-RFG-10): standing still it stops, and a kill's free volley stops with it. Moving, the free volley sweeps.
	using namespace DTChainTriggerTest;
	const uint16 Chain = ChainSource();
	FChainReading Unused;
	FDTHarness H;
	H.bResolve = false;
	GiveChained(H, TEXT("Weapon.ArcBlade"), true, true, Unused);
	const uint32 Serial = AddSerial(H, FVector2f(100.0f, 0.0f));
	H.StepMany(DTConst::SIM_FIXED_HZ);
	TestEqual(TEXT("standing still it does not turn"), H.HitLog.Num(), 0);
	AddKill(H);
	H.StepMany(DTConst::SIM_FIXED_HZ / 2);
	TestEqual(TEXT("at rest a kill's free volley does not turn it either"), H.HitLog.Num(), 0);
	for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ / 2; ++Step)
	{
		H.View.HeroPositions[0] += FVector2f(Step % 2 == 0 ? 4.0f : -4.0f, 0.0f);
		if (Step == 4)
		{
			AddKill(H);
		}
		H.StepOnce();
	}
	TestEqual(TEXT("moving, a kill's free volley is one sweep as Modifier.ChainTrigger"), HitsOn(H, Serial, Chain), 1);
	for (const FDTHit& Hit : H.HitLog)
	{
		if (Hit.SourceId == Chain)
		{
			TestEqual(TEXT("at the weapon's damage"), Hit.Damage, H.Weapons.GetLastSpec(0, 0).Damage);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersChainTriggerSingularity, "Dusktracer.Modifiers.ChainTrigger.Singularity", DTWeaponTestHarness::TestFlags) // T-MOD-16, D-039
bool FDTModifiersChainTriggerSingularity::RunTest(const FString& Parameters)
{
	// Singularity keeps its rule (R-RFG-14): the free volley's mines join the live one, damage kept, and the mine stays its creator's.
	using namespace DTChainTriggerTest;
	const uint16 Chain = ChainSource();
	float Most[2] = { 0.0f, 0.0f };
	for (int32 Killed = 0; Killed < 2; ++Killed)
	{
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.GravityMine"), true, true, Unused);
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepOnce();
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 60.0f;
		TestEqual(TEXT("the weapon laid one mine"), H.Weapons.GetLiveMines(0, 0), 1);
		if (Killed == 1)
		{
			AddKill(H);
		}
		H.StepMany(2);
		TestEqual(TEXT("a free volley lays no second mine"), H.Weapons.GetLiveMines(0, 0), 1);
		H.StepMany(4 * DTConst::SIM_FIXED_HZ);
		int32 ChainHits = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Most[Killed] = FMath::Max(Most[Killed], Hit.Damage);
			ChainHits += Hit.SourceId == Chain ? 1 : 0;
		}
		TestEqual(TEXT("the merged mine stays the weapon's"), ChainHits, 0);
	}
	TestTrue(TEXT("the mine blasts"), Most[0] > 0.0f);
	TestNearlyEqual(TEXT("the free volley's damage joins the live mine: one volley more"), Most[1], 2.0f * Most[0], 0.01f * Most[0]);
	{
		// The other order: the free volley lays the mine, the weapon's own volley joins it, and the mine is the modifier's.
		FChainReading Unused;
		FDTHarness H;
		H.bResolve = false;
		GiveChained(H, TEXT("Weapon.GravityMine"), true, true, Unused);
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 60.0f;
		H.StepOnce();
		TestEqual(TEXT("no mine yet"), H.Weapons.GetLiveMines(0, 0), 0);
		AddKill(H);
		H.StepMany(2);
		TestEqual(TEXT("the free volley lays the mine"), H.Weapons.GetLiveMines(0, 0), 1);
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 0.0f;
		H.StepOnce();
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 60.0f;
		TestEqual(TEXT("the weapon's volley joins it"), H.Weapons.GetLiveMines(0, 0), 1);
		H.StepMany(4 * DTConst::SIM_FIXED_HZ);
		const uint16 Own = H.Run.GetPlayer(0).Weapons[0].SourceId;
		float Blast = 0.0f;
		int32 OwnHits = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Blast = Hit.SourceId == Chain ? FMath::Max(Blast, Hit.Damage) : Blast;
			OwnHits += Hit.SourceId == Own ? 1 : 0;
		}
		TestNearlyEqual(TEXT("a mine the free volley laid is the modifier's, whatever joins it"), Blast, 2.0f * Most[0], 0.01f * Most[0]);
		TestEqual(TEXT("and the weapon has none of it"), OwnHits, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTModifiersScatterOneGeneration, "Dusktracer.Modifiers.ScatterOneGeneration", DTWeaponTestHarness::TestFlags) // T-MOD-05 (D-078)
bool FDTModifiersScatterOneGeneration::RunTest(const FString& Parameters)
{
	using namespace DTModifierTest;
	using namespace DTWeaponTestHarness;
	const FName Ring(TEXT("Weapon.RingEmitter"));
	const FName Shell(TEXT("Mod.ScatterShell"));
	const FName Split(TEXT("Mod.SplitCore"));
	const int32 Steps = 10 * DTConst::SIM_FIXED_HZ;

	// A level 8 Ring Emitter with Scatter Shell on an empty field: every round's flight ends and bursts one ring of six; a ring's
	// child never bursts again (R-MOD-05), so the weapon asks for seven emissions a round and stays inside its budget (R-WPN-09).
	FDTHarness Plain;
	Plain.Weapons.Give(Plain.Run, 0, Ring, 8);
	Plain.StepMany(Steps);
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, Ring, 8);
	TestEqual(TEXT("Scatter Shell socketed"), H.Weapons.Socket(H.Run, 0, 0, Shell), EDTWeaponResult::Ok);
	H.StepMany(Steps);
	const FDTWeaponCounters& With = H.Weapons.GetCounters(0, 0);
	const int64 Rounds = Plain.Weapons.GetCounters(0, 0).Spawned;
	const int64 Children = With.Spawned - Rounds;
	AddInfo(FString::Printf(TEXT("rounds %lld, emitted with Scatter Shell %lld, refused %lld, max generation %d"), Rounds, With.Spawned, With.Throttled,
		H.Weapons.GetMaxGeneration()));
	TestEqual(TEXT("the budget refuses nothing"), With.Throttled, static_cast<int64>(0));
	TestEqual(TEXT("no ring's child bursts: one generation"), H.Weapons.GetMaxGeneration(), 1);
	TestTrue(TEXT("rounds burst into rings"), Children > 0);
	TestEqual(TEXT("each burst is one ring of six"), Children % 6, static_cast<int64>(0));
	TestTrue(TEXT("at most seven emissions a round"), With.Spawned <= 7 * Rounds);

	// A mine's detonation still bursts one ring, and that ring's children do not burst again (a few enemies to lay mines at).
	FDTHarness Mine;
	FDTHarness MinePlain;
	for (FDTHarness* Field : { &Mine, &MinePlain })
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Field->AddEnemy(FVector2f(300.0f + 40.0f * Index, 0.0f));
		}
	}
	Mine.Weapons.Give(Mine.Run, 0, TEXT("Weapon.GravityMine"), 1);
	TestEqual(TEXT("Scatter Shell on the mine"), Mine.Weapons.Socket(Mine.Run, 0, 0, Shell), EDTWeaponResult::Ok);
	MinePlain.Weapons.Give(MinePlain.Run, 0, TEXT("Weapon.GravityMine"), 1);
	Mine.StepMany(Steps);
	MinePlain.StepMany(Steps);
	const int64 MineChildren = Mine.Weapons.GetCounters(0, 0).Spawned - MinePlain.Weapons.GetCounters(0, 0).Spawned;
	TestTrue(TEXT("a detonation bursts a ring"), MineChildren > 0);
	TestEqual(TEXT("a detonation's ring is six"), MineChildren % 6, static_cast<int64>(0));
	TestEqual(TEXT("the mine's ring's children do not burst"), Mine.Weapons.GetMaxGeneration(), 1);

	// Split Core still splits a Scatter Shell child that hits: enemies stand past the rounds' reach, where only ring children fly,
	// so a second generation can only be Split Core's fan on a ring's child (the ring's child itself never bursts again).
	FDTHarness Both;
	for (int32 Index = 0; Index < 48; ++Index)
	{
		const float Angle = 2.0f * PI * static_cast<float>(Index) / 48.0f;
		Both.AddEnemy(FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 1600.0f);
	}
	Both.Weapons.Give(Both.Run, 0, Ring, 8);
	Both.Weapons.Socket(Both.Run, 0, 0, Shell);
	Both.Weapons.Socket(Both.Run, 0, 0, Split);
	Both.StepMany(Steps);
	TestTrue(TEXT("ring children reach the enemies"), Both.HitLog.Num() > 0);
	TestEqual(TEXT("a ring's child that hits is split by Split Core, up to the generation cap"), Both.Weapons.GetMaxGeneration(), DTConst::MOD_SPLIT_GENERATIONS_MAX);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
