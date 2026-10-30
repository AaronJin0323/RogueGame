#include "Game/DTHUD.h"
#include "CanvasItem.h"
#include "ChartCreation.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/World.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Progression/DTXPBar.h"

namespace DTHUDLocal
{
	const FLinearColor UIText = DTPal::Linear(DTPal::UI_Text);
	constexpr float MarginPx = 24.0f;
	constexpr float LineGapPx = 6.0f;
	constexpr float TitleScale = 1.5f;
	const FLinearColor BarBack = DTPal::Linear(DTPal::UI_Panel);
	const FLinearColor BarFill = DTPal::Linear(DTPal::Player_TrailNeutral);
	const FLinearColor BarReady = DTPal::Linear(DTPal::Player_Core);
	// Hero block layout (pixels at the drawn canvas size).
	constexpr float BarWidthPx = 240.0f;
	constexpr float BarHeightPx = 10.0f;
	constexpr float PipSizePx = 18.0f;
	constexpr float PipGapPx = 6.0f;
	constexpr float BlockGapPx = 16.0f;
	constexpr float RunOverScale = 2.0f;
	// Exponential smoothing of the readout so it is legible; the measured numbers come from Tools/Perf.ps1.
	constexpr double Smoothing = 0.1;
}

/** Smoothed frame and game-thread time from the engine's performance tracking (same source as stat unit). */
class FDTHUDFrameTimes : public IPerformanceDataConsumer
{
public:
	virtual void StartCharting() override {}
	virtual void StopCharting() override {}
	virtual void ProcessFrame(const FFrameData& FrameData) override
	{
		const double Alpha = bHasSample ? DTHUDLocal::Smoothing : 1.0;
		FrameMs += (FrameData.TrueDeltaSeconds * 1000.0 - FrameMs) * Alpha;
		GameThreadMs += (FrameData.GameThreadTimeSeconds * 1000.0 - GameThreadMs) * Alpha;
		bHasSample = true;
	}

	double FrameMs = 0.0;
	double GameThreadMs = 0.0;
	bool bHasSample = false;
};

void ADTHUD::BeginPlay()
{
	Super::BeginPlay();
	if (GEngine)
	{
		FrameTimes = MakeShared<FDTHUDFrameTimes>();
		GEngine->AddPerformanceDataConsumer(FrameTimes);
	}
}

void ADTHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine && FrameTimes.IsValid())
	{
		GEngine->RemovePerformanceDataConsumer(FrameTimes);
	}
	FrameTimes.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADTHUD::DrawHeroes()
{
	const UDTHeroSubsystem* Hero = GetWorld() ? GetWorld()->GetSubsystem<UDTHeroSubsystem>() : nullptr;
	if (!Hero || !Canvas || !GEngine)
	{
		return;
	}
	using namespace DTHUDLocal;
	UFont* const Font = GEngine->GetLargeFont();
	auto TextHeight = [this, Font](const FText& Line, float Scale)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->TextSize(Font, Line.ToString(), Width, Height, Scale, Scale);
		return Height;
	};
	auto Text = [this, Font](const FText& Line, float X, float Y, float Scale)
	{
		FCanvasTextItem Item(FVector2D(X, Y), Line, Font, UIText);
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	};
	auto Box = [this](float X, float Y, float W, float H, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	};
	auto Bar = [&](float X, float Y, float W, float Fill01, const FLinearColor& Color)
	{
		Box(X, Y, W, BarHeightPx, BarBack);
		Box(X, Y, W * FMath::Clamp(Fill01, 0.0f, 1.0f), BarHeightPx, Color);
	};

	const FText HpFormat = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Hp"));
	const FText DashLabel = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Dash"));
	const FText MeterLabel = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Intercession"));
	const FText MeterReady = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.IntercessionReady"));
	const FText AimLabel = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.AimMode"));
	const float LineH = TextHeight(DashLabel, 1.0f);
	const float BlockH = 3.0f * (LineH + LineGapPx) + 2.0f * (BarHeightPx + LineGapPx) + PipSizePx + LineGapPx;

	float Y = Canvas->ClipY - MarginPx - BlockH * Hero->GetNumHeroes() - BlockGapPx * (Hero->GetNumHeroes() - 1);
	for (int32 OwnerId = 0; OwnerId < Hero->GetNumHeroes(); ++OwnerId)
	{
		const FDTHeroState& S = *Hero->GetState(OwnerId);
		const float X = MarginPx;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Hp"), FText::AsNumber(FMath::CeilToInt(FMath::Max(0.0f, S.Hp))));
		Args.Add(TEXT("MaxHp"), FText::AsNumber(FMath::CeilToInt(S.MaxHp)));
		Text(FText::Format(HpFormat, Args), X, Y, 1.0f);
		Y += LineH + LineGapPx;
		Bar(X, Y, BarWidthPx, S.MaxHp > 0.0f ? S.Hp / S.MaxHp : 0.0f, BarFill);
		Y += BarHeightPx + LineGapPx;

		Text(DashLabel, X, Y, 1.0f);
		Y += LineH + LineGapPx;
		const int32 MaxCharges = FDTHeroSim::ComputeMaxCharges(S.Stats);
		const float Cooldown = DTConst::DASH_COOLDOWN_S * S.Stats.DashCooldownMul;
		for (int32 Pip = 0; Pip < MaxCharges; ++Pip)
		{
			const float PipX = X + Pip * (PipSizePx + PipGapPx);
			Box(PipX, Y, PipSizePx, PipSizePx, BarBack);
			if (Pip < S.DashCharges)
			{
				Box(PipX, Y, PipSizePx, PipSizePx, BarReady);
			}
			else if (Pip == S.DashCharges && Cooldown > 0.0f)
			{
				// The refilling pip fills from the bottom as its charge returns.
				const float Refill01 = FMath::Clamp(1.0f - S.DashRefillLeft / Cooldown, 0.0f, 1.0f);
				Box(PipX, Y + PipSizePx * (1.0f - Refill01), PipSizePx, PipSizePx * Refill01, BarFill);
			}
		}
		Y += PipSizePx + LineGapPx;

		const bool bReady = S.Intercession01 >= 1.0f;
		Text(bReady ? MeterReady : MeterLabel, X, Y, 1.0f);
		Y += LineH + LineGapPx;
		Bar(X, Y, BarWidthPx, S.Intercession01, bReady ? BarReady : BarFill);
		Y += BarHeightPx + LineGapPx;
		if (Hero->GetAimMode(OwnerId) == EDTAimMode::Manual)
		{
			Text(AimLabel, X, Y, 1.0f);
		}
		Y += LineH + LineGapPx + BlockGapPx;
	}

	if (Hero->IsRunOver())
	{
		const FText RunOver = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.RunOver"));
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->TextSize(Font, RunOver.ToString(), Width, Height, RunOverScale, RunOverScale);
		Text(RunOver, (Canvas->ClipX - Width) * 0.5f, (Canvas->ClipY - Height) * 0.5f, RunOverScale);
	}
}

void ADTHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}

	UFont* const Font = GEngine->GetLargeFont();
	float Y = DTHUDLocal::MarginPx;
	auto DrawLine = [this, Font, &Y](const FText& Line, float Scale)
	{
		FCanvasTextItem Item(FVector2D(DTHUDLocal::MarginPx, Y), Line, Font, DTHUDLocal::UIText);
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->TextSize(Font, Line.ToString(), Width, Height, Scale, Scale);
		Y += Height + DTHUDLocal::LineGapPx;
	};

	DrawLine(FText::FromStringTable(TEXT("ST_UI"), TEXT("Product.Name")), DTHUDLocal::TitleScale);
	const FText RoundTrip = FText::FromStringTable(TEXT("ST_UI"), TEXT("Test.LocRoundTrip"));
	DrawLine(RoundTrip, 1.0f);
	if (!bLoggedRoundTrip)
	{
		bLoggedRoundTrip = true;
		UE_LOG(LogDT, Log, TEXT("LocRoundTrip culture=%s text=%s"), *FInternationalization::Get().GetCurrentCulture()->GetName(), *RoundTrip.ToString());
	}

	// Developer readout, not player text.
	if (FrameTimes.IsValid() && FrameTimes->bHasSample)
	{
		const double Fps = FrameTimes->FrameMs > 0.0 ? 1000.0 / FrameTimes->FrameMs : 0.0;
		DrawLine(FText::AsCultureInvariant(FString::Printf(TEXT("%.0f fps  %.2f ms frame  %.2f ms game"),
			Fps, FrameTimes->FrameMs, FrameTimes->GameThreadMs)), 1.0f);
	}

	DrawHeroes();
	// Above the world HUD; only the offer screen draws over it (progression.md R-PRG-11).
	FDTXPBar::Draw(Canvas, GetWorld(), Font);

	// The offer screen draws over everything while a hand is open (offers.md R-OFF-13).
	if (UDTOfferSubsystem* Offers = GetWorld() ? GetWorld()->GetSubsystem<UDTOfferSubsystem>() : nullptr)
	{
		Offers->DrawScreen(Canvas);
	}
}
