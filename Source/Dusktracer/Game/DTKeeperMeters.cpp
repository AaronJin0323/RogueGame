#include "Game/DTKeeperMeters.h"
#include "Core/DTPaletteGen.h"

void DTKeeperMeters::Build(const FDTKeeperMeterRead& Read, float DilationLeft, float DilationTotal, TArray<FDTHudMeter>& Out)
{
	static const FName DebtKey(TEXT("Hud.Debt"));
	static const FName PaydayKey(TEXT("Hud.Payday"));
	static const FName MomentumKey(TEXT("Hud.Momentum"));
	static const FName MomentumOpenKey(TEXT("Hud.MomentumOpen"));
	static const FName ShadesKey(TEXT("Hud.Shades"));
	static const FName DilationKey(TEXT("Hud.Dilation"));
	Out.Reset();
	auto Add = [&Out](FName Key, float Value, float Max, const FColor& Color)
	{
		FDTHudMeter& Meter = Out.AddDefaulted_GetRef();
		Meter.Key = Key;
		Meter.Value = Value;
		Meter.Max = Max;
		Meter.Fill01 = Max > 0.0f ? FMath::Clamp(Value / Max, 0.0f, 1.0f) : -1.0f;
		Meter.Color = Color;
	};
	if (Read.bPayday)
	{
		Add(PaydayKey, 1.0f, 1.0f, DTPal::Keeper_Shade);
	}
	else if (Read.bDebt)
	{
		Add(DebtKey, static_cast<float>(Read.Debt), static_cast<float>(Read.DebtMax), DTPal::Keeper_Shade);
	}
	if (Read.bMomentum)
	{
		Add(Read.MomentumMax > 0.0f ? MomentumKey : MomentumOpenKey, Read.Momentum, Read.MomentumMax, DTPal::Keeper_Quicksilver);
	}
	if (Read.bShades)
	{
		Add(ShadesKey, static_cast<float>(Read.Shades), static_cast<float>(Read.ShadesMax), DTPal::Keeper_Shade);
	}
	if (DilationLeft > 0.0f)
	{
		Add(DilationKey, DilationLeft, DilationTotal, DTPal::Keeper_Quicksilver);
	}
}
