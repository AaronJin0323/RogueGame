// The audio runtime of a game world (02_Systems/audio.md): owns the observer, the scheduler and the stinger queue, hands the
// voices it decides to the engine, and keeps the counts and the timer. It reads the sim and never writes it (R-AUD-01).
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTAudioObserver.h"
#include "Audio/DTAudioScheduler.h"
#include "Audio/DTAudioTypes.h"
#include "Audio/DTStingerQueue.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTAudioSubsystem.generated.h"

class USoundBase;
class USoundClass;
class USoundMix;

/**
 * One per game world. Each frame, at the end of the world's tick (after the sim has stepped, paused or not): the interface observer
 * polls (the run families observe at OnStepEnd), the scheduler resolves, the sink starts the voices. Its three parts are timed every
 * frame (R-AUD-21). Its own Tick only holds the volume lock and the observer's binding.
 *
 * Silence (R-AUD-03, D-056 ruling 2): see IsToolProcess and IsSilent. The observer, the scheduler and the counts still run.
 * dt.Audio.Enabled 0 when a world is made means no audio runtime in it; set to 0 later, it stops observing and flushing. DT.Audio.Stats prints the counts and the timer;
 * -DTAudioLog writes the voice log and the counts to Saved/Audio/<seed>_<time>.csv, never to a telemetry file (R-AUD-20).
 * Assets are resolved once, by the convention /Game/DT/Audio/SFX/SW_<Family>_<Name>[_n]: a missing one is silent, counted and
 * logged once per id.
 */
UCLASS()
class DUSKTRACER_API UDTAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/**
	 * The two predicates of R-AUD-03, pure so tests drive them. A tool process is -unattended or sits on a desktop that is not the
	 * default one (Start-DTHidden's private desktop; a packaged check launches with no -unattended). A silent process is a tool
	 * process or one running automation tests. Lock 1 (no engine sink) follows IsSilent; lock 2 (application volume at zero)
	 * follows IsToolProcess only, so a test run from a person's editor does not mute it.
	 */
	static bool IsToolProcess(bool bUnattendedFlag, const FString& DesktopName);
	static bool IsSilent(bool bToolProcess, bool bAutomationTesting);
	/** The name of this thread's desktop on Windows ("Default" on the person's desktop); empty elsewhere. */
	static FString ProcessDesktopName();
	static bool IsToolProcessNow();
	static bool IsSilentNow();
	/** dt.Audio.Enabled. */
	static bool IsEnabled();

	/** One frame at real time Now (Tick runs it; tests call it to drive frames). */
	void RunFrame(double Now);
	/** A run sound or an interface sound, as a family raises it (tests stage requests through these). */
	void RequestRun(int32 Sound, int32 Count, int64 Step) { Scheduler.Request(Sound, Count, Step); }
	void RequestUi(int32 Sound, int32 Count) { Scheduler.RequestUi(Sound, Count); }

	bool HasEngineSink() const { return Sink.IsValid() && Sink->IsEngine(); }
	/** Voices the engine sink handed to the engine. */
	int64 GetEngineStarts() const { return EngineStarts; }
	/** Voices of ids with no asset (silent, counted). */
	int64 GetMissing(int32 Sound) const { return Missing.IsValidIndex(Sound) ? Missing[Sound] : 0; }
	/** Test seam: a recording sink in place of whatever sink the process has. */
	void SetSinkForTest(TSharedPtr<IDTAudioSink> InSink) { Sink = MoveTemp(InSink); }
	const FDTAudioScheduler& GetScheduler() const { return Scheduler; }
	FDTStingerQueue& GetStingers() { return Stingers; }
	const TArray<FDTAudioVoice>& GetVoiceLog() const { return VoiceLog; }
	bool IsObserving() const { return bBound; }

	/** The counts and the timer, one line each (DT.Audio.Stats). */
	FString DescribeStats() const;
	/** Milliseconds of one timed part at a percentile (0..1) over the frames so far; part 0 observe, 1 schedule, 2 hand to the engine. */
	double PartMs(int32 Part, double Percentile) const;
	double PartWorstMs(int32 Part) const;
	int32 NumTimedFrames() const { return static_cast<int32>(TimedFrames); }

	static constexpr int32 NumParts = 3;
	/** The timer keeps the last this many frames per part, in a fixed ring: nothing grows over a run (R-AUD-21). */
	static constexpr int32 TimerFrames = 8192;

private:
	struct FRequests : public IDTAudioRequests
	{
		FDTAudioScheduler* Scheduler = nullptr;
		virtual void Run(int32 Sound, int32 Count, int64 Step, bool bNoDuck) override { Scheduler->Request(Sound, Count, Step, bNoDuck); }
		virtual void Ui(int32 Sound, int32 Count) override { Scheduler->RequestUi(Sound, Count); }
		virtual void HeldDuck(float Db) override { Scheduler->SetHeldDuckDb(Db); }
	};

	void ResolveAssets();
	USoundBase* AssetOf(const FDTAudioVoice& Voice);
	/** A new run (seed changed or the step went back): reset the scheduler on the run's own audio stream. */
	void FollowRun();
	void SyncBinding();
	void WriteLog() const;
	void HandleWorldTickEnd(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
	TArray<float> Filled(int32 Part) const;

	FDTAudioScheduler Scheduler;
	FDTStingerQueue Stingers;
	FDTAudioObserver Observer;
	FRequests Requests;
	TSharedPtr<IDTAudioSink> Sink;
	TWeakObjectPtr<UDTSimSubsystem> Sim;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Assets;
	/** The duck's carrier and the class it lowers (R-AUD-05), resolved once with the waves. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> DuckMix;
	UPROPERTY(Transient)
	TObjectPtr<USoundClass> PlayerClass;
	FDTDuckSender DuckSender;
	/** Per id, the index of its variant 0 in Assets. */
	TArray<int32> AssetFirst;
	TArray<int64> Missing;
	TArray<bool> MissingLogged;

	TArray<FDTAudioVoice> VoiceLog;
	TArray<FDTAudioVoice> Started;
	/** Per part, milliseconds of the last TimerFrames frames, a ring (R-AUD-21). */
	TArray<float> Timer[NumParts];
	int64 TimedFrames = 0;
	FDelegateHandle TickEndHandle;
	bool bTool = false;
	int64 EngineStarts = 0;
	int32 RunSeed = 0;
	int64 LastStep = 0;
	bool bBound = false;
	bool bSilent = false;
	bool bLog = false;
};
