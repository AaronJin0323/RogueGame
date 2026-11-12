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
	// Row layout (pixels at the drawn canvas size): the name, then its bar, on one line at the top centre, in the rectangle the
	// first visual wave gives it (D-080 item 11); the XP row sits in the top-right block (FDTXPBar), out of the hero's column.
	constexpr float BarMaxWidthPx = 560.0f;
	constexpr float BarHeightPx = 14.0f;
	// The row's lower edge, y 29.0 at 1080 (rows 15 to 28): the HUD's lower edge on the hero's column that 06_UX R-UX-01 counts
	// and T-UX-01 reads (through BossRowRect). One pixel lower breaks the rule: the reach up to y 30 is 1 412.39 uu, under 1 416.
	constexpr float RowBottomPx = 29.0f;
	constexpr float RowTopPx = RowBottomPx - BarHeightPx;
	constexpr float NameGapPx = 12.0f;
	constexpr float NameScale = 1.25f;
	constexpr float NameRaisePx = 1.0f;
	// Each side of the centre column: the top-right block's margin, width and the gap to the column (FDTXPBar's block).
	constexpr float SideBlockPx = 24.0f + 360.0f + 24.0f;
}

FBox2f UDTBossSubsystem::BossRowRect(float ClipX)
{
	using namespace DTBossSubsystemLocal;
	return FBox2f(FVector2f(0.0f, RowTopPx), FVector2f(ClipX, RowBottomPx));
}

void UDTBossSubsystem::BossRowLayout(float ClipX, const FVector2f& NameSize, FBox2f& OutName, FBox2f& OutBar)
{
	using namespace DTBossSubsystemLocal;
	const FBox2f Row = BossRowRect(ClipX);
	const float ColumnW = ClipX - 2.0f * SideBlockPx;
	const float BarW = FMath::Max(0.0f, FMath::Min(BarMaxWidthPx, ColumnW - NameSize.X - NameGapPx));
	const float X = 0.5f * (ClipX - (NameSize.X + NameGapPx + BarW));
	// The name's line box is centred on the row and raised by NameRaisePx, so that its drawn glyphs sit inside the row's rows in
	// every culture (measured on 1080 frames: zh-Hans ink reached row 29 when centred).
	const float NameY = Row.Min.Y + 0.5f * (BarHeightPx - NameSize.Y) - NameRaisePx;
	OutName = FBox2f(FVector2f(X, NameY), FVector2f(X + NameSize.X, NameY + NameSize.Y));
	const float BarX = X + NameSize.X + NameGapPx;
	OutBar = FBox2f(FVector2f(BarX, Row.Min.Y), FVector2f(BarX + BarW, Row.Max.Y));
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
	BeforeHero.Owner = this;
	AfterKills.Owner = this;
	AfterKills.bAfterKills = true;
}

void UDTBossSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
		SimSystem->Unregister(&BeforeHero);
		SimSystem->Unregister(&AfterKills);
	}
	BeforeHero.Owner = nullptr;
	AfterKills.Owner = nullptr;
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
	// R-BOS-13: the win is read before the Hero phase; a death in the fight ends the run after the step's kills.
	SimSystem->Register(EDTSimPhase::Input, &BeforeHero);
	SimSystem->Register(EDTSimPhase::Effects, &AfterKills);
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
	SyncHold();
}

void UDTBossSubsystem::SyncRun(const FDTSimContext& Context)
{
	if (Context.StepIndex < LastStep)
	{
		// A new run (UDTSimSubsystem::StartRun): forget the old fight.
		Core.Reset();
		bStartRequested = false;
	}
	LastStep = Context.StepIndex;
}

void UDTBossSubsystem::SyncHold()
{
	if (UDTHeroSubsystem* Hero = Heroes.Get())
	{
		Hero->HoldRunEnd(Core.HasHull());
	}
}

FDTBossStepIn UDTBossSubsystem::MakeStepIn(const FDTSimContext& Context, UDTEnemySubsystem& Enemy) const
{
	FDTBossStepIn In;
	In.StepSeconds = Context.StepSeconds;
	In.StepIndex = Context.StepIndex;
	In.View = Context.View;
	In.Pool = &Enemy.GetPool();
	In.Rng = &Context.Run->GetRng().Get(EDTRngStream::Boss);
	return In;
}

void UDTBossSubsystem::ReadRunEnd(const FDTSimContext& Context, bool bAfterKills)
{
	UDTEnemySubsystem* Enemy = Enemies.Get();
	UDTHeroSubsystem* Hero = Heroes.Get();
	if (!Context.View || !Context.Run || !Enemy)
	{
		return;
	}
	SyncRun(Context);
	// After the kills only on the step every hero went down with the hull standing. Any other win is read before the
	// next Hero phase: the step it was always read on, so no won run changes its length.
	const bool bDeathHeld = Hero && Hero->IsRunEndHeld();
	if (bAfterKills && !bDeathHeld)
	{
		return;
	}
	Core.ReadDefeat(MakeStepIn(Context, *Enemy), Sink);
	if (bDeathHeld && !Context.View->bRunWon)
	{
		Hero->AnnounceAllDown(); // the hull stands: the run ends by the death (HandleAllHeroesDown ends the fight)
	}
	SyncHold();
}

void UDTBossSubsystem::SimStep(const FDTSimContext& Context)
{
	if (!Context.View || !Context.Run)
	{
		return;
	}
	SyncRun(Context);
	UDTEnemySubsystem* Enemy = Enemies.Get();
	if (!Enemy)
	{
		return;
	}
	if (const UDTHeroSubsystem* Hero = Heroes.Get(); Hero && Hero->IsRunEndHeld())
	{
		return; // every hero is down: the fight stops where the death found it, as it did when the death ended it at once
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
	Core.Step(MakeStepIn(Context, *Enemy), Sink);
	SyncHold();
}

void UDTBossSubsystem::EndTelemetrySession()
{
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		Telemetry->EndSession(TEXT("victory"));
	}
}

void UDTBossSubsystem::DrawBossBar(UCanvas* Canvas) const
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
	Canvas->TextSize(Font, Name.ToString(), NameW, NameH, NameScale, NameScale);

	FBox2f NameBox(ForceInit);
	FBox2f BarBox(ForceInit);
	BossRowLayout(Canvas->ClipX, FVector2f(NameW, NameH), NameBox, BarBox);
	const float BarW = BarBox.GetSize().X;
	const float BarX = BarBox.Min.X;
	const float BarY = BarBox.Min.Y;

	FCanvasTextItem Label(FVector2D(NameBox.Min.X, NameBox.Min.Y), Name, Font, DTPal::Linear(DTPal::UI_Text));
	Label.Scale = FVector2D(NameScale, NameScale);
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
