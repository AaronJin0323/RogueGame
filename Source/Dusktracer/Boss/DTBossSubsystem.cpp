#include "Boss/DTBossSubsystem.h"
#include "CanvasItem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Director/DTDirector.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySubsystem.h"
#include "TimerManager.h"

namespace DTBossSubsystemLocal
{
	// Bar layout (pixels at the drawn canvas size): bottom centre, clear of the hero block (bottom left) and the XP bar (top).
	constexpr float BarWidthRatio = 0.5f;
	constexpr float BarHeightPx = 14.0f;
	constexpr float BottomPx = 48.0f;
	constexpr float NameGapPx = 6.0f;
	constexpr float NameScale = 1.25f;
}

bool UDTBossSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTBossSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Enemies = Collection.InitializeDependency<UDTEnemySubsystem>();
	Director = Collection.InitializeDependency<UDTDirector>();
	Heroes = Collection.InitializeDependency<UDTHeroSubsystem>();
	Sink.Owner = this;
}

void UDTBossSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
	}
	if (UDTDirector* Dir = Director.Get())
	{
		Dir->OnBossTime.Remove(BossTimeHandle);
	}
	if (UDTHeroSubsystem* Hero = Heroes.Get())
	{
		Hero->OnAllHeroesDown.Remove(AllDownHandle);
	}
	Sink.Owner = nullptr;
	Super::Deinitialize();
}

void UDTBossSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	if (!Enemies.IsValid())
	{
		Enemies = InWorld.GetSubsystem<UDTEnemySubsystem>();
	}
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (!SimSystem)
	{
		UE_LOG(LogDT, Error, TEXT("UDTBossSubsystem: no UDTSimSubsystem in this world; the boss will not step"));
		return;
	}
	// The boss runs last in the Enemies phase (07_Technical §4). Registration is AddUnique per phase, in call order:
	// registering the enemy system first fixes the order whichever OnWorldBeginPlay runs first.
	if (UDTEnemySubsystem* Enemy = Enemies.Get())
	{
		SimSystem->Register(EDTSimPhase::Enemies, Enemy);
	}
	SimSystem->Register(EDTSimPhase::Enemies, this);
	if (UDTDirector* Dir = Director.IsValid() ? Director.Get() : InWorld.GetSubsystem<UDTDirector>())
	{
		Director = Dir;
		BossTimeHandle = Dir->OnBossTime.AddUObject(this, &UDTBossSubsystem::HandleBossTime);
	}
	if (UDTHeroSubsystem* Hero = Heroes.IsValid() ? Heroes.Get() : InWorld.GetSubsystem<UDTHeroSubsystem>())
	{
		Heroes = Hero;
		AllDownHandle = Hero->OnAllHeroesDown.AddUObject(this, &UDTBossSubsystem::HandleAllHeroesDown);
	}
}

void UDTBossSubsystem::StartFight()
{
	if (Core.IsActive())
	{
		UE_LOG(LogDT, Warning, TEXT("Boss: a fight is already running (phase %d)"), Core.GetPhase());
		return;
	}
	bStartRequested = true;
}

void UDTBossSubsystem::ForcePhase(int32 InPhase)
{
	Core.ForcePhase(InPhase, Sink);
}

void UDTBossSubsystem::HandleBossTime(float RunSeconds)
{
	UE_LOG(LogDT, Log, TEXT("Boss: boss time at %.1f s; the Warden enters at the next step"), RunSeconds);
	StartFight();
}

void UDTBossSubsystem::HandleAllHeroesDown(int64 StepIndex)
{
	Core.End(TEXT("death"), Sink);
}

void UDTBossSubsystem::SimStep(const FDTSimContext& Context)
{
	if (!Context.View || !Context.Run)
	{
		return;
	}
	if (Context.StepIndex < LastStep)
	{
		// A new run (UDTSimSubsystem::StartRun): forget the old fight.
		Core.Reset();
		bStartRequested = false;
	}
	LastStep = Context.StepIndex;
	UDTEnemySubsystem* Enemy = Enemies.Get();
	if (!Enemy)
	{
		return;
	}
	if (bStartRequested)
	{
		bStartRequested = false;
		DashesAtStart.Reset();
		if (const UDTHeroSubsystem* Hero = Heroes.Get())
		{
			for (int32 Owner = 0; Owner < Hero->GetNumHeroes(); ++Owner)
			{
				DashesAtStart.Add(Hero->GetState(Owner)->Dashes);
			}
		}
		Core.Start(GetWins(), Context.View->HeroPositions, Sink);
		UE_LOG(LogDT, Log, TEXT("Boss: the Warden (form %s) queued at step %lld"), FDTBossCore::FormName(Core.GetForm()), Context.StepIndex);
	}
	FDTBossStepIn In;
	In.StepSeconds = Context.StepSeconds;
	In.TimeScale = Context.View->EnemyTimeScale;
	In.StepIndex = Context.StepIndex;
	In.View = Context.View;
	In.Pool = &Enemy->GetPool();
	In.Rng = &Context.Run->GetRng().Get(EDTRngStream::Boss);
	Core.Step(In, Sink);
}

void UDTBossSubsystem::EndTelemetrySession()
{
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		Telemetry->EndSession(TEXT("victory"));
	}
}

void UDTBossSubsystem::DrawBossBar(UCanvas* Canvas, float TextScale) const
{
	if (!Canvas || !GEngine || !Core.HasHull())
	{
		return;
	}
	using namespace DTBossSubsystemLocal;
	UFont* const Font = GEngine->GetLargeFont();
	const FText Name = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.BossName"));
	float NameW = 0.0f;
	float NameH = 0.0f;
	Canvas->TextSize(Font, Name.ToString(), NameW, NameH, NameScale * TextScale, NameScale * TextScale);

	const float BarW = Canvas->ClipX * BarWidthRatio;
	const float BarX = (Canvas->ClipX - BarW) * 0.5f;
	const float BarY = Canvas->ClipY - BottomPx - BarHeightPx;

	FCanvasTextItem Label(FVector2D((Canvas->ClipX - NameW) * 0.5f, BarY - NameGapPx - NameH), Name, Font, DTPal::Linear(DTPal::UI_Text));
	Label.Scale = FVector2D(NameScale * TextScale, NameScale * TextScale);
	Label.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Label);

	FCanvasTileItem Back(FVector2D(BarX, BarY), FVector2D(BarW, BarHeightPx), DTPal::Linear(DTPal::UI_Panel));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	// The bar reads the effective HP fraction (R-BOS-09), in the enemy's warm magenta (04_Presentation §3).
	FCanvasTileItem Fill(FVector2D(BarX, BarY), FVector2D(BarW * FMath::Clamp(Core.GetHpFrac(), 0.0f, 1.0f), BarHeightPx), DTPal::Linear(DTPal::Enemy_Optic));
	Fill.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Fill);
}

// ---- sink -------------------------------------------------------------------------------------------------------------

void UDTBossSubsystem::FSink::QueueSpawn(const FDTEnemySpawn& Spawn)
{
	if (UDTEnemySubsystem* Enemy = Owner ? Owner->Enemies.Get() : nullptr)
	{
		Enemy->QueueSpawn(Spawn);
	}
}

void UDTBossSubsystem::FSink::BossStart(FName Form, int32 Wins)
{
	const UDTHeroSubsystem* Hero = Owner ? Owner->Heroes.Get() : nullptr;
	const int32 Players = Hero ? Hero->GetNumHeroes() : 1;
	UE_LOG(LogDT, Log, TEXT("Boss: start form=%s wins=%d players=%d"), *Form.ToString(), Wins, Players);
	if (UDTTelemetrySubsystem* Telemetry = Owner ? UDTTelemetrySubsystem::Find(Owner) : nullptr)
	{
		Telemetry->Records().RecordBossStart(Form, Wins, Players);
	}
}

void UDTBossSubsystem::FSink::BossPhase(int32 Phase, float FightTime, float HpFrac, bool bByTime)
{
	UE_LOG(LogDT, Log, TEXT("Boss: phase %d at %.2f s hp_frac=%.3f by_time=%d"), Phase, FightTime, HpFrac, bByTime ? 1 : 0);
	if (UDTTelemetrySubsystem* Telemetry = Owner ? UDTTelemetrySubsystem::Find(Owner) : nullptr)
	{
		Telemetry->Records().RecordBossPhase(Phase, FightTime, HpFrac, bByTime);
	}
}

void UDTBossSubsystem::FSink::BossCalibrated(float WindowDps, float EffectiveMax, bool bClamped)
{
	UE_LOG(LogDT, Log, TEXT("Boss: calibrated window_dps=%.1f effective_max=%.0f clamped=%d"), WindowDps, EffectiveMax, bClamped ? 1 : 0);
	if (UDTTelemetrySubsystem* Telemetry = Owner ? UDTTelemetrySubsystem::Find(Owner) : nullptr)
	{
		Telemetry->Records().RecordBossCalibrated(WindowDps, EffectiveMax, bClamped);
	}
}

void UDTBossSubsystem::FSink::ThreatAction(const FVector2f& Origin, FName AttackId)
{
	if (UDTTelemetrySubsystem* Telemetry = Owner ? UDTTelemetrySubsystem::Find(Owner) : nullptr)
	{
		Telemetry->Records().RecordThreatAction(Origin, EDTEnemyRole::Boss, AttackId);
	}
}

void UDTBossSubsystem::FSink::BossEnd(FDTTelBossEnd& End)
{
	if (!Owner)
	{
		return;
	}
	if (const UDTHeroSubsystem* Hero = Owner->Heroes.Get())
	{
		for (int32 Player = 0; Player < End.PerPlayer.Num() && Player < Hero->GetNumHeroes(); ++Player)
		{
			const int32 Before = Owner->DashesAtStart.IsValidIndex(Player) ? Owner->DashesAtStart[Player] : 0;
			End.PerPlayer[Player].Dashes = Hero->GetState(Player)->Dashes - Before;
		}
	}
	UE_LOG(LogDT, Log, TEXT("Boss: end outcome=%s fight_time=%.1f s hp_left_frac=%.3f phase=%d"), *End.Outcome, End.FightTimeSeconds, End.HpLeftFrac, End.Phase);
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(Owner))
	{
		Telemetry->Records().RecordBossEnd(End);
	}
}

void UDTBossSubsystem::FSink::BossDefeated(int64 StepIndex, int32 KillerOwner)
{
	if (!Owner)
	{
		return;
	}
	UE_LOG(LogDT, Log, TEXT("Run end reason=victory step=%lld killer=%d"), StepIndex, KillerOwner);
	Owner->OnBossDefeated.Broadcast(StepIndex);
	// Called from inside the step: the telemetry session closes on the next tick, after the step's own records.
	if (UWorld* World = Owner->GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(Owner, &UDTBossSubsystem::EndTelemetrySession));
	}
}
