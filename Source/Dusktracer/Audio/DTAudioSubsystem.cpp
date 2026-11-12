#include "Audio/DTAudioSubsystem.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Misc/CoreDelegates.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace DTAudioSubsystemLocal
{
	static TAutoConsoleVariable<int32> CVarAudioEnabled(
		TEXT("dt.Audio.Enabled"),
		1,
		TEXT("Audio runtime: 1 = observe, schedule and hand voices to the sink (default); 0 = off, nothing observed or scheduled (the sim proof, debugging)."));

	/** Hands voices to the engine. Never made in an unattended process (R-AUD-03). */
	class FDTEngineSink : public IDTAudioSink
	{
	public:
		/** Pushes the duck's mix once, here: a silent process never makes this sink, so it never pushes a mix. */
		FDTEngineSink(UWorld* InWorld, USoundMix* InDuckMix, USoundClass* InPlayerClass) : World(InWorld), DuckMix(InDuckMix), PlayerClass(InPlayerClass)
		{
			if (World.IsValid() && DuckMix.IsValid())
			{
				UGameplayStatics::PushSoundMixModifier(World.Get(), DuckMix.Get());
				// The engine starts where the sender believes it is: 0 dB, whatever an earlier world left in the device.
				SetPlayerDuckDb(0.0f);
			}
		}
		/** One push, one pop: the world ends with SC_Player back at 0 dB and the mix taken off (R-AUD-05). */
		virtual ~FDTEngineSink() override
		{
			if (World.IsValid() && DuckMix.IsValid())
			{
				if (PlayerClass.IsValid())
				{
					UGameplayStatics::ClearSoundMixClassOverride(World.Get(), DuckMix.Get(), PlayerClass.Get(), 0.0f);
				}
				UGameplayStatics::PopSoundMixModifier(World.Get(), DuckMix.Get());
			}
		}
		virtual void SetPlayerDuckDb(float Db) override
		{
			if (World.IsValid() && DuckMix.IsValid() && PlayerClass.IsValid())
			{
				// No engine fade: the engine is given the scheduler's own curve, one value per frame it moves.
				UGameplayStatics::SetSoundMixClassOverride(World.Get(), DuckMix.Get(), PlayerClass.Get(), FMath::Pow(10.0f, Db / 20.0f), 1.0f, 0.0f, true);
			}
		}
		virtual void Start(const DTCat::FSoundRow& Row, const FDTAudioVoice& Voice, USoundBase* Sound) override
		{
			if (Sound && World.IsValid())
			{
				// The UI class plays while the world is paused (R-AUD-04).
				UGameplayStatics::PlaySound2D(World.Get(), Sound, Voice.Volume, Voice.Pitch, 0.0f, nullptr, nullptr, Row.Class == DTCat::ESoundClass::UI);
			}
		}
		virtual bool IsEngine() const override { return true; }

	private:
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<USoundMix> DuckMix;
		TWeakObjectPtr<USoundClass> PlayerClass;
	};

	static const TCHAR* const DuckMixPath = TEXT("/Game/DT/Audio/Mix/SMX_Duck.SMX_Duck");
	static const TCHAR* const PlayerClassPath = TEXT("/Game/DT/Audio/Mix/SC_Player.SC_Player");

	static double Percentile(TArray<float> Values, double P)
	{
		if (Values.Num() == 0)
		{
			return 0.0;
		}
		// Only DT.Audio.Stats and the closing line ask: a copy of the ring, sorted, never per frame.
		Values.Sort();
		const int32 Index = FMath::Clamp(static_cast<int32>(FMath::CeilToDouble(P * Values.Num())) - 1, 0, Values.Num() - 1);
		return Values[Index];
	}

	static void PrintStats(const TArray<FString>& Args, UWorld* World)
	{
		const UDTAudioSubsystem* Audio = World ? World->GetSubsystem<UDTAudioSubsystem>() : nullptr;
		UE_LOG(LogDT, Display, TEXT("%s"), Audio ? *Audio->DescribeStats() : TEXT("DT.Audio.Stats: no audio runtime in this world"));
	}

	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Audio.Stats"),
		TEXT("Audio: per sound id asked, played, merged, dropped and missing; the timer's p50, p95 and worst frame per part."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintStats));

	static const TCHAR* const PartNames[UDTAudioSubsystem::NumParts] = { TEXT("observe"), TEXT("schedule"), TEXT("hand") };
}

bool UDTAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// dt.Audio.Enabled 0 when the world is made: no audio runtime in it at all (the sim proof compares a run with and without it).
	const UWorld* World = Cast<UWorld>(Outer);
	return IsEnabled() && World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

bool UDTAudioSubsystem::IsToolProcess(bool bUnattendedFlag, const FString& DesktopName)
{
	// A tool launch is -unattended, or sits on Start-DTHidden's private desktop (Tools/HiddenLaunch.ps1), even with no flag.
	return bUnattendedFlag || (!DesktopName.IsEmpty() && !DesktopName.Equals(TEXT("Default"), ESearchCase::IgnoreCase));
}

bool UDTAudioSubsystem::IsSilent(bool bToolProcess, bool bAutomationTesting)
{
	return bToolProcess || bAutomationTesting;
}

FString UDTAudioSubsystem::ProcessDesktopName()
{
#if PLATFORM_WINDOWS
	if (HDESK Desktop = ::GetThreadDesktop(::GetCurrentThreadId()))
	{
		WCHAR Name[256] = {};
		DWORD Needed = 0;
		if (::GetUserObjectInformationW(Desktop, UOI_NAME, Name, sizeof(Name) - sizeof(WCHAR), &Needed))
		{
			return FString(Name);
		}
	}
#endif
	return FString();
}

bool UDTAudioSubsystem::IsToolProcessNow()
{
	return IsToolProcess(FParse::Param(FCommandLine::Get(), TEXT("unattended")), ProcessDesktopName());
}

bool UDTAudioSubsystem::IsSilentNow()
{
	return IsSilent(IsToolProcessNow(), GIsAutomationTesting);
}

bool UDTAudioSubsystem::IsEnabled()
{
	return DTAudioSubsystemLocal::CVarAudioEnabled.GetValueOnGameThread() != 0;
}

void UDTAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Requests.Scheduler = &Scheduler;
	Observer.BeforeStep = [this]() { FollowRun(); };
	Observer.SetOutput(Requests);
	Scheduler.Configure(DTAudio::Catalog(), DTAudio::StreamSeed(0));
	Missing.Init(0, DTCat::SoundCount);
	MissingLogged.Init(false, DTCat::SoundCount);
	for (TArray<float>& Ring : Timer)
	{
		Ring.Init(0.0f, TimerFrames);
	}
	bTool = IsToolProcessNow();
	bSilent = IsSilent(bTool, GIsAutomationTesting);
	bLog = FParse::Param(FCommandLine::Get(), TEXT("DTAudioLog"));
	// Lock 1 (R-AUD-03): a silent process never makes the engine sink. Lock 2: a tool process holds the application volume at
	// zero, here and every Tick, and never gives it back; an automation run in a person's editor leaves their volume alone.
	if (bTool)
	{
		FApp::SetVolumeMultiplier(0.0f);
	}
	ResolveAssets();
	if (!bSilent)
	{
		Sink = MakeShared<DTAudioSubsystemLocal::FDTEngineSink>(GetWorld(), DuckMix, PlayerClass);
	}
	// The frame's flush runs after every tickable of the world, the sim's step included, paused or not (R-AUD-05).
	TickEndHandle = FWorldDelegates::OnWorldTickEnd.AddUObject(this, &UDTAudioSubsystem::HandleWorldTickEnd);
	int32 Resolved = 0;
	for (const TObjectPtr<USoundBase>& Asset : Assets)
	{
		Resolved += Asset ? 1 : 0;
	}
	// The silence of R-AUD-03, in the log of every run: which process this is, whether the engine sink exists, what holds the volume.
	UE_LOG(LogDT, Log, TEXT("Audio: start: %s; %s; volume %s; %d of %d wave(s) resolved; duck mix %s"),
		bTool ? TEXT("tool process") : (bSilent ? TEXT("automation") : TEXT("a person's launch")),
		Sink.IsValid() && Sink->IsEngine() ? TEXT("engine sink") : TEXT("no engine sink"),
		bTool ? TEXT("held at 0") : TEXT("left to the settings"), Resolved, Assets.Num(), DuckMix ? TEXT("found") : TEXT("missing"));
}

void UDTAudioSubsystem::HandleWorldTickEnd(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	if (InWorld == GetWorld() && IsEnabled())
	{
		RunFrame(FPlatformTime::Seconds());
	}
}

void UDTAudioSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldTickEnd.Remove(TickEndHandle);
	Sink.Reset(); // the engine sink takes its duck back while the world still stands
	Observer.Unbind();
	bBound = false;
	if (NumTimedFrames() > 0)
	{
		// A person's run closes with one line of the timer (R-AUD-21); a tool run's log carries the same.
		UE_LOG(LogDT, Log, TEXT("Audio: %d frame(s); p95 ms observe %.4f schedule %.4f hand %.4f; worst %.4f %.4f %.4f"), NumTimedFrames(),
			PartMs(0, 0.95), PartMs(1, 0.95), PartMs(2, 0.95), PartWorstMs(0), PartWorstMs(1), PartWorstMs(2));
	}
	{
		int64 Asked = 0, Played = 0, Missed = 0;
		for (int32 Sound = 0; Sound < Scheduler.NumSounds(); ++Sound)
		{
			Asked += Scheduler.GetCounts(Sound).Asked;
			Played += Scheduler.GetCounts(Sound).Played;
			Missed += GetMissing(Sound);
		}
		UE_LOG(LogDT, Log, TEXT("Audio: end: asked %lld, voices started %lld, handed to the engine %lld, missing %lld"), Asked, Played, EngineStarts, Missed);
	}
	if (bLog)
	{
		WriteLog();
	}
	Super::Deinitialize();
}

void UDTAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SyncBinding();
}

TStatId UDTAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDTAudioSubsystem, STATGROUP_Tickables);
}

void UDTAudioSubsystem::ResolveAssets()
{
	DuckMix = LoadObject<USoundMix>(nullptr, DTAudioSubsystemLocal::DuckMixPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	PlayerClass = LoadObject<USoundClass>(nullptr, DTAudioSubsystemLocal::PlayerClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Assets.Reset();
	AssetFirst.Reset();
	for (const DTCat::FSoundRow& Row : DTAudio::Catalog())
	{
		AssetFirst.Add(Assets.Num());
		for (int32 Variant = 0; Variant < FMath::Max(1, Row.Variants); ++Variant)
		{
			const FString Package = Variant == 0 ? FString(Row.Asset) : FString::Printf(TEXT("%s_%d"), Row.Asset, Variant);
			USoundBase* Sound = nullptr;
			// R-AUD-22: a row the owner has not accepted loads nothing, so a stale local import of it stays silent.
			if (Row.bPlayable && FPackageName::DoesPackageExist(Package))
			{
				Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *FPackageName::GetShortName(Package)), nullptr, LOAD_NoWarn | LOAD_Quiet);
			}
			Assets.Add(Sound);
		}
	}
}

USoundBase* UDTAudioSubsystem::AssetOf(const FDTAudioVoice& Voice)
{
	if (!AssetFirst.IsValidIndex(Voice.Sound))
	{
		return nullptr;
	}
	const int32 Index = AssetFirst[Voice.Sound] + Voice.Variant;
	USoundBase* Sound = Assets.IsValidIndex(Index) ? Assets[Index].Get() : nullptr;
	if (!Sound)
	{
		++Missing[Voice.Sound];
		if (!MissingLogged[Voice.Sound])
		{
			MissingLogged[Voice.Sound] = true;
			UE_LOG(LogDT, Log, TEXT("Audio: %s %s; its voices are silent and counted"), DTCat::Sounds[Voice.Sound].Id,
				DTCat::Sounds[Voice.Sound].bPlayable ? *FString::Printf(TEXT("has no asset at %s"), DTCat::Sounds[Voice.Sound].Asset) : TEXT("may not play (heard is not yes or delegated)"));
		}
	}
	return Sound;
}

void UDTAudioSubsystem::SyncBinding()
{
	UDTSimSubsystem* SimSys = Sim.Get();
	const bool bWanted = IsEnabled() && SimSys;
	if (bWanted && !bBound)
	{
		Observer.Bind(*SimSys);
		bBound = true;
	}
	else if (!bWanted && bBound)
	{
		Observer.Unbind();
		bBound = false;
	}
}

void UDTAudioSubsystem::FollowRun()
{
	UDTSimSubsystem* SimSys = Sim.Get();
	if (!SimSys)
	{
		return;
	}
	const int32 Seed = SimSys->GetRun().GetRng().GetRunSeed();
	const int64 Step = SimSys->GetStepIndex();
	if (Seed != RunSeed || Step < LastStep)
	{
		RunSeed = Seed;
		Scheduler.ResetRun(DTAudio::StreamSeed(Seed));
		Stingers.Reset();
		Observer.Reset();
	}
	LastStep = Step;
}

void UDTAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bTool && FApp::GetVolumeMultiplier() != 0.0f)
	{
		// The second lock holds whatever set the volume since (the settings subsystem sets its default at its own start).
		FApp::SetVolumeMultiplier(0.0f);
	}
	SyncBinding();
	// The frame's flush is HandleWorldTickEnd's, after the sim has stepped.
}

void UDTAudioSubsystem::RunFrame(double Now)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FollowRun();
	const uint64 C0 = FPlatformTime::Cycles64();
	Observer.Frame(*World);
	const uint64 C1 = FPlatformTime::Cycles64();
	Started.Reset();
	const UDTSimSubsystem* SimSys = Sim.Get();
	Scheduler.Frame(Now, SimSys ? SimSys->GetStepIndex() : 0, Started);
	const uint64 C2 = FPlatformTime::Cycles64();
	for (const FDTAudioVoice& Voice : Started)
	{
		USoundBase* Sound = AssetOf(Voice);
		if (Sink.IsValid())
		{
			Sink->Start(DTCat::Sounds[Voice.Sound], Voice, Sound);
			EngineStarts += Sink->IsEngine() && Sound ? 1 : 0;
		}
		if (bLog)
		{
			VoiceLog.Add(Voice);
		}
	}
	if (Sink.IsValid() && DuckSender.Next(Scheduler.GetDuckDb()))
	{
		Sink->SetPlayerDuckDb(DuckSender.Sent);
	}
	const uint64 C3 = FPlatformTime::Cycles64();
	const int32 Slot = static_cast<int32>(TimedFrames % TimerFrames);
	Timer[0][Slot] = static_cast<float>((FPlatformTime::ToSeconds64(C1 - C0) + Observer.TakeStepSeconds()) * 1000.0);
	Timer[1][Slot] = static_cast<float>(FPlatformTime::ToSeconds64(C2 - C1) * 1000.0);
	Timer[2][Slot] = static_cast<float>(FPlatformTime::ToSeconds64(C3 - C2) * 1000.0);
	++TimedFrames;
}

TArray<float> UDTAudioSubsystem::Filled(int32 Part) const
{
	TArray<float> Out(Timer[Part].GetData(), static_cast<int32>(FMath::Min<int64>(TimedFrames, TimerFrames)));
	return Out;
}

double UDTAudioSubsystem::PartMs(int32 Part, double Percentile) const
{
	return Part >= 0 && Part < NumParts ? DTAudioSubsystemLocal::Percentile(Filled(Part), Percentile) : 0.0;
}

double UDTAudioSubsystem::PartWorstMs(int32 Part) const
{
	float Worst = 0.0f;
	if (Part >= 0 && Part < NumParts)
	{
		for (const float Ms : Filled(Part))
		{
			Worst = FMath::Max(Worst, Ms);
		}
	}
	return Worst;
}

FString UDTAudioSubsystem::DescribeStats() const
{
	FString Out = FString::Printf(TEXT("DT.Audio.Stats: %s, %s, run seed %d, %d frame(s), %d voice(s) alive, %lld slot(s) stolen by danger\n"),
		bSilent ? (bTool ? TEXT("tool process: no engine sink, volume held at 0") : TEXT("automation: no engine sink, volume untouched")) : TEXT("engine sink"), bBound ? TEXT("observing") : TEXT("off"),
		RunSeed, NumTimedFrames(), Scheduler.NumAlive(), Scheduler.GetStolen());
	Out += TEXT("id,asked,played,merged,dropped,missing\n");
	for (int32 Sound = 0; Sound < Scheduler.NumSounds(); ++Sound)
	{
		const FDTAudioCounts& C = Scheduler.GetCounts(Sound);
		Out += FString::Printf(TEXT("%s,%lld,%lld,%lld,%lld,%lld\n"), DTCat::Sounds[Sound].Id, C.Asked, C.Played, C.Merged, C.Dropped, GetMissing(Sound));
	}
	for (int32 Part = 0; Part < NumParts; ++Part)
	{
		Out += FString::Printf(TEXT("timer %s ms: p50 %.4f p95 %.4f worst %.4f (watch %.2f)\n"), DTAudioSubsystemLocal::PartNames[Part], PartMs(Part, 0.5), PartMs(Part, 0.95),
			PartWorstMs(Part), DTConst::AUD_FRAME_MS_WATCH);
	}
	return Out;
}

void UDTAudioSubsystem::WriteLog() const
{
	FString Text = TEXT("step,time,id,variant,volume,pitch,duck_db,count\n");
	for (const FDTAudioVoice& Voice : VoiceLog)
	{
		Text += FString::Printf(TEXT("%lld,%.4f,%s,%d,%.4f,%.4f,%.2f,%d\n"), Voice.Step, Voice.Time, DTCat::Sounds[Voice.Sound].Id, Voice.Variant, Voice.Volume, Voice.Pitch,
			Voice.DuckDb, Voice.Count);
	}
	Text += TEXT("\n") + DescribeStats();
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Audio"), FString::Printf(TEXT("%d_%s.csv"), RunSeed, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
	FFileHelper::SaveStringToFile(Text, *Path);
	UE_LOG(LogDT, Log, TEXT("Audio: wrote %s"), *Path);
}
