#include "Game/DTHUD.h"
#include "CanvasItem.h"
#include "ChartCreation.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Internationalization/Text.h"

namespace DTHUDLocal
{
	// Pal.UI.Text (04_Presentation §3, #E8E4DC); mirrors the token table until a token source exists (R-PRE-09).
	const FLinearColor UIText = FLinearColor(FColor(0xE8, 0xE4, 0xDC));
	constexpr float MarginPx = 24.0f;
	constexpr float LineGapPx = 6.0f;
	constexpr float TitleScale = 1.5f;
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
	DrawLine(FText::FromStringTable(TEXT("ST_UI"), TEXT("Test.LocRoundTrip")), 1.0f);

	// Developer readout, not player text.
	if (FrameTimes.IsValid() && FrameTimes->bHasSample)
	{
		const double Fps = FrameTimes->FrameMs > 0.0 ? 1000.0 / FrameTimes->FrameMs : 0.0;
		DrawLine(FText::AsCultureInvariant(FString::Printf(TEXT("%.0f fps  %.2f ms frame  %.2f ms game"),
			Fps, FrameTimes->FrameMs, FrameTimes->GameThreadMs)), 1.0f);
	}
}
