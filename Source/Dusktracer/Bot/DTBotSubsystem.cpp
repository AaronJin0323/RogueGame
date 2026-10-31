#include "Bot/DTBotSubsystem.h"

#include "Combat/DTReforge.h"
#include "Boss/DTBossSubsystem.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DTGameMode.h"
#include "Keepers/DTKeeperSubsystem.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Core/DTConstantsGen.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"
#include "Progression/DTGemSubsystem.h"
#include "Telemetry/DTTelemetryRecords.h"
#include "Telemetry/DTTelemetrySubsystem.h"

namespace DTBotSubsystemLocal
{
	static TAutoConsoleVariable<FString> CVarBotPolicy(TEXT("dt.Bot.Policy"), TEXT("Off"),
		TEXT("Bot offer policy for player 0: Off (no bot), None (no-pick arm), Random, Steered. Read at world begin play (-DTBot=)."), ECVF_Default);
	static TAutoConsoleVariable<FString> CVarBotPath(TEXT("dt.Bot.Path"), TEXT("Storm"),
		TEXT("Bot Steered: Patron Path (Storm, Forge, Tide)."), ECVF_Default);
	static TAutoConsoleVariable<FString> CVarBotArchetype(TEXT("dt.Bot.Archetype"), TEXT("Tempest"),
		TEXT("Bot Steered: preferred archetype name of the Path (Tempest, ArcGrid, Downpour, Hammer, Furnace, Slam, Whirlpool, Frost, Stillness)."), ECVF_Default);
	static TAutoConsoleVariable<FString> CVarBotSecond(TEXT("dt.Bot.Second"), TEXT(""),
		TEXT("Bot Steered: second Keeper Path (empty = the fixed default for the Patron)."), ECVF_Default);
	static TAutoConsoleVariable<int32> CVarBotVersion(TEXT("dt.Bot.Version"), 1,
		TEXT("Bot version: 1 = the S1 bot, frozen (bot.md R-BOT-01); 2 = bot v2. Read at world begin play (-DTBotVersion=)."), ECVF_Default);
	static TAutoConsoleVariable<float> CVarBotSkill(TEXT("dt.Bot.Skill"), 1.0f,
		TEXT("Bot skill 0..1. 1 = no reaction delay, no error. Below 1: v1 adds delay and noise, v2 is the profile of bot.md R-BOT-04..11."), ECVF_Default);
	static TAutoConsoleVariable<int32> CVarBotFast(TEXT("dt.Bot.Fast"), 0,
		TEXT("Bot: fixed steps run per frame (accelerated time, headless runs); 0 = real time."), ECVF_Default);
	static TAutoConsoleVariable<float> CVarBotMaxSeconds(TEXT("dt.Bot.MaxSeconds"), 0.0f,
		TEXT("Bot: end the run (telemetry reason bot_timeout) at this sim time; 0 = no cap."), ECVF_Default);
	static TAutoConsoleVariable<int32> CVarBotQuit(TEXT("dt.Bot.Quit"), 0,
		TEXT("Bot: 1 = quit the process once the run is over and its telemetry is written."), ECVF_Default);

	/** Default steps per frame for a bare -DTBotFast (ten sim seconds a frame at 60 Hz). */
	constexpr int32 DefaultFastSteps = 600;
	/** Frames to wait for the game mode / boss to close the telemetry session before the bot closes it. */
	constexpr int32 EndGraceFrames = 120;
	constexpr int32 HandGuard = 64;
	/** The level probe of the bot record ("level at 630 s", Docs/Validation/S1_bot_baseline.md). */
	constexpr double LevelProbeS = 630.0;
	/** The bot record's hits_wall: a hit this close to an arena edge counts as taken at the wall. */
	constexpr float HitWallBandUU = 100.0f;

	static void SetCVar(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(*Value, ECVF_SetByCommandline);
		}
	}

	static void Mix(uint32& Hash, const void* Data, int32 Bytes)
	{
		Hash = FCrc::MemCrc32(Data, Bytes, Hash);
	}
}

bool UDTBotSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTBotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Heroes = Collection.InitializeDependency<UDTHeroSubsystem>();
	Offers = Collection.InitializeDependency<UDTOfferSubsystem>();
}

void UDTBotSubsystem::Deinitialize()
{
	Deactivate();
	Super::Deinitialize();
}

TStatId UDTBotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDTBotSubsystem, STATGROUP_Tickables);
}

FDTBotConfig UDTBotSubsystem::ReadConfig()
{
	using namespace DTBotSubsystemLocal;
	const TCHAR* Cmd = FCommandLine::Get();
	FString Text;
	if (FParse::Value(Cmd, TEXT("DTBot="), Text)) { SetCVar(TEXT("dt.Bot.Policy"), Text); }
	if (FParse::Value(Cmd, TEXT("DTBotPath="), Text)) { SetCVar(TEXT("dt.Bot.Path"), Text); }
	if (FParse::Value(Cmd, TEXT("DTBotArchetype="), Text)) { SetCVar(TEXT("dt.Bot.Archetype"), Text); }
	if (FParse::Value(Cmd, TEXT("DTBotSecond="), Text)) { SetCVar(TEXT("dt.Bot.Second"), Text); }
	if (FParse::Value(Cmd, TEXT("DTBotVersion="), Text)) { SetCVar(TEXT("dt.Bot.Version"), Text); }
	if (FParse::Value(Cmd, TEXT("DTBotSkill="), Text)) { SetCVar(TEXT("dt.Bot.Skill"), Text); }
	if (FParse::Value(Cmd, TEXT("DTBotFast="), Text)) { SetCVar(TEXT("dt.Bot.Fast"), Text); }
	else if (FParse::Param(Cmd, TEXT("DTBotFast"))) { SetCVar(TEXT("dt.Bot.Fast"), FString::FromInt(DefaultFastSteps)); }
	if (FParse::Value(Cmd, TEXT("DTBotMaxSeconds="), Text)) { SetCVar(TEXT("dt.Bot.MaxSeconds"), Text); }
	if (FParse::Param(Cmd, TEXT("DTBotQuit"))) { SetCVar(TEXT("dt.Bot.Quit"), TEXT("1")); }

	FDTBotConfig Config;
	Config.Policy = FDTBotConfig::ParsePolicy(CVarBotPolicy.GetValueOnGameThread());
	const EDTPath Path = FDTBotConfig::ParsePath(CVarBotPath.GetValueOnGameThread());
	Config.Path = Path == EDTPath::None ? EDTPath::Storm : Path;
	Config.Archetype = FName(*CVarBotArchetype.GetValueOnGameThread());
	Config.SecondPath = FDTBotConfig::ParsePath(CVarBotSecond.GetValueOnGameThread());
	Config.Version = FDTBotConfig::KnownVersion(CVarBotVersion.GetValueOnGameThread());
	Config.Skill = CVarBotSkill.GetValueOnGameThread();
	Config.bPrimaryFirst = FParse::Param(FCommandLine::Get(), TEXT("DTBotPrimaryFirst"));
	if (FParse::Value(Cmd, TEXT("DTBotCarry="), Text))
	{
		Config.CarryWeapon = FName(*Text);
		Config.bCarryHold = FParse::Param(Cmd, TEXT("DTBotCarryHold"));
	}
	Config.Seed = ADTGameMode::GetCommandLineSeed();
	return Config;
}

void UDTBotSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FDTBotConfig Config = ReadConfig();
	Config.Seed = ADTGameMode::GetRunSeed(&InWorld); // a restarted run's seed is the map's (settings.md R-SET-24)
	if (Config.Policy != EDTBotPolicy::Off)
	{
		StepsPerFrame = FMath::Max(0, DTBotSubsystemLocal::CVarBotFast.GetValueOnGameThread());
		MaxSeconds = DTBotSubsystemLocal::CVarBotMaxSeconds.GetValueOnGameThread();
		bQuit = DTBotSubsystemLocal::CVarBotQuit.GetValueOnGameThread() != 0;
		FParse::Value(FCommandLine::Get(), TEXT("DTBotOut="), OutPath);
		FString ProbePath;
		if (FParse::Value(FCommandLine::Get(), TEXT("DTProbe="), ProbePath))
		{
			FString Text;
			FString Error;
			FDTProbeScript Script;
			if (!FFileHelper::LoadFileToString(Text, *ProbePath) || !Script.Parse(Text, Error))
			{
				UE_LOG(LogDT, Error, TEXT("Bot: probe %s not read: %s"), *ProbePath, *Error);
				return;
			}
			StartProbe(Script, Config.Seed);
			return;
		}
		Activate(Config);
	}
}

void UDTBotSubsystem::StartProbe(const FDTProbeScript& Script, int32 Seed)
{
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::Steered;
	Config.Path = Script.Patron;
	Config.Seed = Seed;
	Config.bProbeRoute = true;
	Config.CastStep = Script.CastAtS < 0.0f ? FDTBotConfig::CastNever : FMath::RoundToInt64(Script.CastAtS * DTConst::SIM_FIXED_HZ);
	Config.CastDashSteps = FMath::RoundToInt32(Script.CastDashS * DTConst::SIM_FIXED_HZ);
	Activate(Config);
	Probe = Script;
	bProbe = bActive;
	bProbeApplied = false;
	MaxSeconds = Script.WarmupS + Script.WindowS;
}

void UDTBotSubsystem::ApplyProbe()
{
	bProbeApplied = true;
	UWorld* World = GetWorld();
	UDTSimSubsystem* SimSys = Sim.Get();
	UDTKeeperSubsystem* Keepers = World ? World->GetSubsystem<UDTKeeperSubsystem>() : nullptr;
	UDTWeaponSubsystem* Weapons = World ? World->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr;
	if (!SimSys || !Keepers || !Weapons || !Enemies || !Heroes.IsValid())
	{
		UE_LOG(LogDT, Error, TEXT("Bot: probe needs the sim, Keeper, weapon, enemy and hero systems"));
		return;
	}
	FDTPlayerRunState& Player = SimSys->GetRun().GetPlayer(0);
	if (Probe.Second != EDTPath::None)
	{
		Player.Offer.SecondPath = Probe.Second;
		Player.Keepers.Second = Probe.Second;
		Player.Keepers.bCommitDone = true;
	}
	for (const FName& Id : Probe.Blessings)
	{
		if (!Keepers->Bless(0, Id))
		{
			UE_LOG(LogDT, Error, TEXT("Bot: probe blessing %s refused"), *Id.ToString());
		}
	}
	Keepers->Rebuild(0);
	for (const FDTProbeWeapon& Weapon : Probe.Weapons)
	{
		EDTWeaponResult Result = Weapons->Give(0, Weapon.Id, Weapon.Level);
		if (Result == EDTWeaponResult::AlreadyOwned)
		{
			Result = Weapons->LevelUp(0, Weapon.Id, Weapon.Level);
		}
		if (Result != EDTWeaponResult::Ok)
		{
			UE_LOG(LogDT, Error, TEXT("Bot: probe weapon %s L%d refused (%d)"), *Weapon.Id.ToString(), Weapon.Level, static_cast<int32>(Result));
		}
		for (const FName& Mod : Weapon.Mods)
		{
			const EDTWeaponResult Socketed = Weapons->Socket(0, Weapon.Id, Mod);
			if (Socketed != EDTWeaponResult::Ok)
			{
				UE_LOG(LogDT, Error, TEXT("Bot: probe modifier %s on %s refused (%d)"), *Mod.ToString(), *Weapon.Id.ToString(), static_cast<int32>(Socketed));
			}
		}
	}
	Heroes->SetNoDamage(true); // the probe reads damage dealt; a hero that dies ends the window
	if (const FDTHeroState* State = Heroes->GetState(0))
	{
		FDTHeroStats Stats = State->Stats;
		Stats.IntercessionCooldownMul = 0.0f; // the meter is full from the start; only the script presses it
		Heroes->SetStats(0, Stats);
	}
	Enemies->SetKeepHpScale(Probe.HpScale);
	Enemies->SetKeepPopulation(Probe.Field);
	UE_LOG(LogDT, Log, TEXT("Bot: probe applied: %d blessing(s), %d weapon(s), field %d at HP scale %.2f, window %.1f + %.1f s, cast at %.1f s"),
		Probe.Blessings.Num(), Probe.Weapons.Num(), Probe.Field, Probe.HpScale, Probe.WarmupS, Probe.WindowS, Probe.CastAtS);
}

void UDTBotSubsystem::ApplyCarry()
{
	const FName Carry = Brain.GetConfig().CarryWeapon;
	UDTSimSubsystem* SimSys = Sim.Get();
	UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	if (Carry.IsNone() || !SimSys || !Weapons)
	{
		return;
	}
	const FDTPlayerRunState& Player = SimSys->GetRun().GetPlayer(0);
	if (!bCarryGiven && Player.Keepers.Primary != EDTPath::None)
	{
		bCarryGiven = true;
		if (!Player.Weapons.ContainsByPredicate([Carry](const FDTWeaponInstance& W) { return W.Id == Carry; }))
		{
			const EDTWeaponResult Given = Weapons->Give(0, Carry, 1);
			UE_CLOG(Given != EDTWeaponResult::Ok, LogDT, Error, TEXT("Bot: carry %s refused (%d)"), *Carry.ToString(), static_cast<int32>(Given));
		}
	}
	const int32 Target = FDTBotBrain::CarryGiftLevel(Player, Carry);
	if (Target > 0)
	{
		const FDTWeaponInstance* Carried = Player.Weapons.FindByPredicate([Carry](const FDTWeaponInstance& W) { return W.Id == Carry; });
		CarryGift += Target - Carried->Level;
		const EDTWeaponResult Raised = Weapons->LevelUp(0, Carry, Target);
		UE_CLOG(Raised != EDTWeaponResult::Ok, LogDT, Error, TEXT("Bot: carry %s to L%d refused (%d)"), *Carry.ToString(), Target, static_cast<int32>(Raised));
	}
}

void UDTBotSubsystem::WriteBuildSnapshot()
{
	bSnapshotDone = true;
	UDTSimSubsystem* SimSys = Sim.Get();
	if (SimSys && !OutPath.IsEmpty())
	{
		FFileHelper::SaveStringToFile(FDTProbeScript::FromPlayer(SimSys->GetRun().GetPlayer(0)).ToText(), *(OutPath + TEXT(".L16")));
	}
}

void UDTBotSubsystem::Activate(const FDTBotConfig& Config)
{
	Deactivate();
	UDTSimSubsystem* SimSys = Sim.Get();
	if (!SimSys || !Heroes.IsValid())
	{
		UE_LOG(LogDT, Error, TEXT("Bot: no sim or hero subsystem in this world; bot not started"));
		return;
	}
	Brain.Reset(Config);
	InputHash = 0;
	InputCount = 0;
	FramesSinceEnd = 0;
	bDead = bVictory = bTimedOut = bExitRequested = false;
	bPawnMuted = false;
	Minutes.Reset();
	LastHit.Reset();
	Hits = HitsStill = HitsWall = 0;
	bStickAtRest = true;
	bProbe = bProbeApplied = bSnapshotDone = false;
	bCarryGiven = false;
	CarryGift = 0;
	Level630 = 0;
	EndSeconds = 0.0;
	HeroEventHandle = Heroes->OnHeroEvent.AddUObject(this, &UDTBotSubsystem::HandleHeroEvent);
	// The bot answers the hands; unattended auto-pick (slot 0) would pre-empt it.
	DTBotSubsystemLocal::SetCVar(TEXT("dt.Offer.AutoPick"), TEXT("0"));
	StepEndHandle = SimSys->OnStepEnd.AddUObject(this, &UDTBotSubsystem::HandleStepEnd);
	if (UDTBossSubsystem* Boss = GetWorld() ? GetWorld()->GetSubsystem<UDTBossSubsystem>() : nullptr)
	{
		BossHandle = Boss->OnBossDefeated.AddUObject(this, &UDTBotSubsystem::HandleBossDefeated);
	}
	bActive = true;
	FParse::Value(FCommandLine::Get(), TEXT("DTBotTrace="), TracePath);
	if (!TracePath.IsEmpty())
	{
		IFileManager::Get().Delete(*TracePath, false, true, true);
		FString Order;
		for (UWorldSubsystem* Sub : GetWorld()->GetSubsystemArrayCopy<UWorldSubsystem>())
		{
			Order += Sub->GetClass()->GetName() + TEXT(" ");
		}
		UE_LOG(LogDT, Log, TEXT("Bot: world subsystem order: %s"), *Order);
	}
	MutePawn();
	LatchNext();
	UE_LOG(LogDT, Log, TEXT("Bot: active (%s) fast=%d max_s=%.0f quit=%d"), *Config.Describe(), StepsPerFrame, MaxSeconds, bQuit ? 1 : 0);
}

void UDTBotSubsystem::Deactivate()
{
	if (UDTSimSubsystem* SimSys = Sim.Get())
	{
		SimSys->OnStepEnd.Remove(StepEndHandle);
	}
	if (UDTBossSubsystem* Boss = GetWorld() ? GetWorld()->GetSubsystem<UDTBossSubsystem>() : nullptr)
	{
		Boss->OnBossDefeated.Remove(BossHandle);
	}
	if (UDTHeroSubsystem* HeroSys = Heroes.Get())
	{
		HeroSys->OnHeroEvent.Remove(HeroEventHandle);
	}
	HeroEventHandle.Reset();
	StepEndHandle.Reset();
	BossHandle.Reset();
	bActive = false;
}

void UDTBotSubsystem::MutePawn()
{
	// The pawn latches its own (empty) input every frame, which would overwrite the bot's axes: player 0's pawn stops
	// ticking while the bot drives. Its drawing comes from the hero subsystem's SimRender, so it still renders.
	// Checked every frame: the pawn's BeginPlay (after this subsystem's OnWorldBeginPlay) registers its tick again, and a
	// single frame of pawn input would make the run depend on frame timing.
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<ADTHero> It(GetWorld()); It; ++It)
	{
		if (It->GetPlayerIndex() == 0 && (It->IsActorTickEnabled() || !It->HasActorBegunPlay()))
		{
			It->SetActorTickEnabled(false);
			if (!bPawnMuted && It->HasActorBegunPlay())
			{
				bPawnMuted = true;
				UE_LOG(LogDT, Log, TEXT("Bot: player 0 pawn input muted"));
			}
		}
	}
}

void UDTBotSubsystem::HandleBossDefeated(int64 StepIndex)
{
	bVictory = true;
}

void UDTBotSubsystem::HandleHeroEvent(const FDTHeroEvent& Event)
{
	const UDTSimSubsystem* SimSys = Sim.Get();
	if (!bActive || Event.OwnerId != 0 || !SimSys)
	{
		return;
	}
	const double T = static_cast<double>(Event.StepIndex + 1) * SimSys->GetClock().GetStepSeconds();
	const int32 Minute = FMath::Max(0, FMath::CeilToInt32(T / DTConst::TEL_RATE_UNIT_S) - 1);
	if (Minutes.Num() <= Minute)
	{
		Minutes.SetNum(Minute + 1);
	}
	if (Event.Kind == EDTHeroEventKind::Dash)
	{
		++Minutes[Minute].Dashes;
	}
	else if (Event.Kind == EDTHeroEventKind::HeroDamaged)
	{
		++Minutes[Minute].Hits;
		Minutes[Minute].DamageShare += Event.MaxHp > 0.0f ? Event.Amount / Event.MaxHp : 0.0f;
		LastHit = FString::Printf(TEXT("%s/%s/%s"), DTTelNames::HeroHitKind(Event.HitKind), DTTelNames::EnemyRole(Event.SourceRole), *Event.AttackId.ToString());
		const FDTHeroState* State = Heroes.IsValid() ? Heroes->GetState(0) : nullptr;
		++Hits;
		HitsStill += bStickAtRest ? 1 : 0;
		if (State)
		{
			const float Edge = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU - DTConst::HERO_RADIUS_UU) - FMath::Max(FMath::Abs(State->Pos.X), FMath::Abs(State->Pos.Y));
			HitsWall += Edge < DTBotSubsystemLocal::HitWallBandUU ? 1 : 0;
		}
		UE_LOG(LogDT, Verbose, TEXT("Bot: hit t=%.2f kind=%d role=%d attack=%s amount=%.1f hp=%.1f pos=(%.0f,%.0f) charges=%d"), T,
			static_cast<int32>(Event.HitKind), static_cast<int32>(Event.SourceRole), *Event.AttackId.ToString(), Event.Amount, Event.HpAfter,
			State ? State->Pos.X : 0.0f, State ? State->Pos.Y : 0.0f, State ? State->DashCharges : 0);
	}
}

void UDTBotSubsystem::WriteRecord() const
{
	if (OutPath.IsEmpty())
	{
		return;
	}
	FString Csv = TEXT("minute,t_end_s,dmg_share,dashes,hits,level,hp,alive\n");
	for (int32 Minute = 0; Minute < Minutes.Num(); ++Minute)
	{
		const FMinute& M = Minutes[Minute];
		Csv += FString::Printf(TEXT("%d,%.4f,%.6f,%d,%d,%d,%.2f,%d\n"), Minute, M.EndSeconds, M.DamageShare, M.Dashes, M.Hits, M.Level, M.Hp, M.Alive);
	}
	const FDTBotStats& Stats = Brain.GetStats();
	FString Reforge = TEXT("none");
	if (Sim.IsValid())
	{
		const FDTPlayerRunState& Player = Sim->GetRun().GetPlayer(0);
		for (const int32 Row : Player.PathReforges)
		{
			Reforge = DTReforge::RowId(Row).ToString();
		}
		for (const FDTWeaponInstance& Weapon : Player.Weapons)
		{
			Reforge = Weapon.Evolve != INDEX_NONE ? DTReforge::RowId(Weapon.Evolve).ToString() : Reforge;
		}
	}
	FDTReforgeRead Read;
	if (const UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr)
	{
		Read = Weapons->GetSim().GetReforgeRead(0);
	}
	const FString ReadText = FString::Printf(TEXT("reforge_read=pillars:%d;pillar_s:%.2f;by_hit:%d;by_time:%d;discharges:%d;husks:%d;shades:%d\n"),
		Read.Pillars, Read.PillarSeconds, Read.ShatterByHit, Read.ShatterByTime, Read.Discharges, Read.Husks, Read.Shades);
	const FString CarryText = Brain.GetConfig().CarryWeapon.IsNone() ? FString() : FString::Printf(TEXT("carry_gift=%d\n"), CarryGift);
	const FString Summary = ReadText + CarryText + FString::Printf(TEXT("outcome=%s\nend_s=%.4f\nlevel_630=%d\nsteps=%lld\ndash_presses=%d\nintercession_presses=%d\npicks=%d\ndeclines=%d\ninput_hash=%08x\nlast_hit=%s\nhits=%d\nhits_still=%d\nhits_wall=%d\nreforge=%s\nconfig=%s\n"),
		bVictory ? TEXT("victory") : (bDead ? TEXT("death") : TEXT("timeout")), EndSeconds, Level630, Sim.IsValid() ? Sim->GetStepIndex() : -1,
		Stats.DashPresses, Stats.IntercessionPresses, Stats.Picks, Stats.Declines, InputHash, *LastHit, Hits, HitsStill, HitsWall, *Reforge, *Brain.GetConfig().Describe());
	FFileHelper::SaveStringToFile(Csv, *OutPath);
	FFileHelper::SaveStringToFile(Summary, *(OutPath + TEXT(".txt")));
	UE_LOG(LogDT, Log, TEXT("Bot: record written to %s"), *OutPath);
}

void UDTBotSubsystem::LatchNext()
{
	UDTSimSubsystem* SimSys = Sim.Get();
	UDTHeroSubsystem* HeroSys = Heroes.Get();
	const FDTHeroState* State = HeroSys ? HeroSys->GetState(0) : nullptr;
	if (!SimSys || !State)
	{
		return;
	}
	const UWorld* World = GetWorld();
	FDTBotWorld View;
	View.Hero = State;
	View.StepSeconds = static_cast<float>(SimSys->GetClock().GetStepSeconds());
	View.StepIndex = SimSys->GetStepIndex();
	View.Telegraphs = SimSys->GetView().Telegraphs;
	if (const UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr)
	{
		const FDTEnemyPool& Pool = Enemies->GetPool();
		View.EnemyPos = Pool.GetPositions();
		View.EnemyVel = Pool.GetVelocities();
		View.EnemyRadius = Pool.GetRadii();
		View.RoundPos = Pool.GetRounds().GetPositions();
		View.RoundVel = Pool.GetRounds().GetVelocities();
		View.RoundRadius = Pool.GetRounds().GetRadii();
	}
	if (const UDTGemSubsystem* Gems = World ? World->GetSubsystem<UDTGemSubsystem>() : nullptr)
	{
		View.Gems = Gems->GetPool().GetPositions();
		View.Chests = Gems->GetPool().GetChestPositions();
	}
	const FDTHeroInput Input = Brain.Decide(View);
	if (Brain.GetConfig().Version == 1 && Brain.GetConfig().Skill < 1.0f)
	{
		HeroSys->SetAimMode(0, EDTAimMode::Manual); // v1 only: a player opts into Manual, so v2 stays in Auto (bot.md R-BOT-08)
	}
	HeroSys->LatchInput(0, Input);
	bStickAtRest = Input.Move.IsNearlyZero();

	using DTBotSubsystemLocal::Mix;
	Mix(InputHash, &Input.Move, sizeof(Input.Move));
	Mix(InputHash, &Input.AimStick, sizeof(Input.AimStick));
	const uint8 Presses = (Input.bDashPress ? 1 : 0) | (Input.bIntercessionPress ? 2 : 0);
	Mix(InputHash, &Presses, sizeof(Presses));
	++InputCount;
}

void UDTBotSubsystem::HandleStepEnd()
{
	if (!bActive)
	{
		return;
	}
	if (const UDTHeroSubsystem* HeroSys = Heroes.Get())
	{
		bDead |= HeroSys->IsRunOver();
	}
	UDTSimSubsystem* SimSys = Sim.Get();
	if (SimSys && EndSeconds <= 0.0)
	{
		const double T = static_cast<double>(SimSys->GetStepIndex()) * SimSys->GetClock().GetStepSeconds();
		const int32 Minute = FMath::Max(0, FMath::CeilToInt32(T / DTConst::TEL_RATE_UNIT_S) - 1);
		if (Minutes.Num() <= Minute)
		{
			Minutes.SetNum(Minute + 1);
		}
		const FDTPlayerRunState& Player = SimSys->GetRun().GetPlayer(0);
		const FDTHeroState* State = Heroes.IsValid() ? Heroes->GetState(0) : nullptr;
		if (bProbe && !bProbeApplied && Player.Keepers.Primary != EDTPath::None)
		{
			ApplyProbe();
		}
		ApplyCarry();
		if (!bSnapshotDone && Player.Level >= DTConst::TEL_POWER_CHECK_LEVEL)
		{
			WriteBuildSnapshot();
		}
		FMinute& M = Minutes[Minute];
		M.EndSeconds = T;
		M.Level = Player.Level;
		M.Hp = State ? State->Hp : 0.0f;
		if (const UDTEnemySubsystem* Enemies = GetWorld() ? GetWorld()->GetSubsystem<UDTEnemySubsystem>() : nullptr)
		{
			M.Alive = Enemies->GetLiveCount();
		}
		if (T <= DTBotSubsystemLocal::LevelProbeS + UE_KINDA_SMALL_NUMBER)
		{
			Level630 = Player.Level;
		}
		if (MaxSeconds > 0.0f && T >= MaxSeconds)
		{
			bTimedOut = true;
		}
		if (IsRunFinished())
		{
			EndSeconds = T;
		}
	}
	if (IsRunFinished())
	{
		return; // the input stream (and its hash) is the run's; steps after its end are not the bot's
	}
	LatchNext();
	if (SimSys && !TracePath.IsEmpty())
	{
		TraceStep(*SimSys);
	}
	if (SimSys && SimSys->GetStepIndex() % DTConst::SIM_FIXED_HZ == 0)
	{
		const FDTHeroState* State = Heroes.IsValid() ? Heroes->GetState(0) : nullptr;
		UE_LOG(LogDT, VeryVerbose, TEXT("Bot: trace step=%lld hash=%08x pos=(%.3f,%.3f) hp=%.2f enemies=%d"), SimSys->GetStepIndex(), InputHash,
			State ? State->Pos.X : 0.0f, State ? State->Pos.Y : 0.0f, State ? State->Hp : 0.0f, SimSys->GetView().EnemyPositions.Num());
	}
}

void UDTBotSubsystem::TraceStep(UDTSimSubsystem& SimSys)
{
	using DTBotSubsystemLocal::Mix;
	const auto Bits = [](float Value) { uint32 Out = 0; FMemory::Memcpy(&Out, &Value, sizeof(Out)); return Out; };
	const UWorld* World = GetWorld();
	const UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr;
	const UDTGemSubsystem* Gems = World ? World->GetSubsystem<UDTGemSubsystem>() : nullptr;
	const FDTHeroState* State = Heroes.IsValid() ? Heroes->GetState(0) : nullptr;
	const FDTPlayerRunState& Player = SimSys.GetRun().GetPlayer(0);
	uint32 EnemyHash = 0;
	uint32 RoundHash = 0;
	uint32 GemHash = 0;
	int32 NumEnemies = 0;
	int32 NumRounds = 0;
	int32 NumGems = 0;
	if (Enemies)
	{
		const TConstArrayView<FVector2f> Pos = Enemies->GetPool().GetPositions();
		const TConstArrayView<FVector2f> Rounds = Enemies->GetPool().GetRounds().GetPositions();
		NumEnemies = Pos.Num();
		NumRounds = Rounds.Num();
		Mix(EnemyHash, Pos.GetData(), Pos.Num() * sizeof(FVector2f));
		Mix(RoundHash, Rounds.GetData(), Rounds.Num() * sizeof(FVector2f));
	}
	if (Gems)
	{
		const TConstArrayView<FVector2f> Pos = Gems->GetPool().GetPositions();
		NumGems = Pos.Num();
		Mix(GemHash, Pos.GetData(), Pos.Num() * sizeof(FVector2f));
	}
	FString Line = FString::Printf(TEXT("%lld,%08x,%08x,%08x,%08x,%d,%08x,%d,%08x,%d,%08x,%d"), SimSys.GetStepIndex(), InputHash,
		State ? Bits(State->Pos.X) : 0, State ? Bits(State->Pos.Y) : 0,
		State ? Bits(State->Hp) : 0, NumEnemies, EnemyHash, NumRounds, RoundHash, NumGems, GemHash, Player.Level);
	for (int32 Stream = 0; Stream < static_cast<int32>(EDTRngStream::Count); ++Stream)
	{
		Line += FString::Printf(TEXT(",%08x"), static_cast<uint32>(SimSys.GetRun().GetRng().Get(static_cast<EDTRngStream>(Stream)).GetCurrentSeed()));
	}
	Line += FString::Printf(TEXT(",%08x\n"), static_cast<uint32>(Player.Offer.Rng.GetCurrentSeed()));
	FFileHelper::SaveStringToFile(Line, *TracePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

int32 UDTBotSubsystem::ServiceOffers()
{
	UDTOfferSubsystem* OfferSys = Offers.Get();
	UDTSimSubsystem* SimSys = Sim.Get();
	if (!bActive || !OfferSys || !SimSys || SimSys->GetRun().GetNumPlayers() == 0)
	{
		return 0;
	}
	OfferSys->UpdateFlow();
	int32 Answered = 0;
	while (Answered < DTBotSubsystemLocal::HandGuard)
	{
		const FDTOfferHand* Hand = OfferSys->GetOpenHand(0);
		if (!Hand)
		{
			break;
		}
		const int32 Slot = Brain.ChooseCard(*Hand, SimSys->GetRun().GetPlayer(0));
		const bool bDone = Slot == INDEX_NONE ? OfferSys->Decline(0) : OfferSys->Pick(0, Slot);
		if (!bDone && !OfferSys->Pick(0, 0))
		{
			UE_LOG(LogDT, Warning, TEXT("Bot: could not answer hand %d"), Hand->HandId);
			break;
		}
		++Answered;
	}
	return Answered;
}

int32 UDTBotSubsystem::RunSteps(int32 MaxSteps)
{
	UDTSimSubsystem* SimSys = Sim.Get();
	UDTOfferSubsystem* OfferSys = Offers.Get();
	if (!bActive || !SimSys)
	{
		return 0;
	}
	int32 Ran = 0;
	while (Ran < MaxSteps && !IsRunFinished())
	{
		ServiceOffers();
		if (OfferSys && (OfferSys->IsAnyHandOpen() || SimSys->IsPauseRequested()))
		{
			break; // a hand the bot could not answer holds the sim, as it would for a player
		}
		SimSys->StepNow(1);
		++Ran;
	}
	return Ran;
}

void UDTBotSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!bActive || !World || !World->HasBegunPlay())
	{
		return;
	}
	MutePawn();
	if (StepsPerFrame > 0)
	{
		RunSteps(StepsPerFrame);
	}
	else
	{
		ServiceOffers();
	}

	if (!IsRunFinished() || bExitRequested)
	{
		return;
	}
	UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this);
	const bool bOpen = Telemetry && Telemetry->IsSessionActive();
	if (bOpen && (bTimedOut || ++FramesSinceEnd > DTBotSubsystemLocal::EndGraceFrames))
	{
		// Death and victory are closed by the game mode / boss on the next tick; a timeout is the bot's to close.
		Telemetry->EndSession(bTimedOut && !bDead && !bVictory ? TEXT("bot_timeout") : (bVictory ? TEXT("victory") : TEXT("death")));
	}
	if (!(Telemetry && Telemetry->IsSessionActive()))
	{
		WriteRecord();
		const FDTBotStats& Stats = Brain.GetStats();
		UE_LOG(LogDT, Log, TEXT("Bot: run over (%s) step=%lld dash_presses=%d intercession_presses=%d picks=%d declines=%d input_hash=%08x"),
			bVictory ? TEXT("victory") : (bDead ? TEXT("death") : TEXT("timeout")), Sim.IsValid() ? Sim->GetStepIndex() : -1,
			Stats.DashPresses, Stats.IntercessionPresses, Stats.Picks, Stats.Declines, InputHash);
		if (bQuit)
		{
			bExitRequested = true;
			FPlatformMisc::RequestExit(false, TEXT("DT bot run over"));
		}
		else
		{
			bExitRequested = true; // log once
		}
	}
}
