#include "Audio/Observe/DTObserveWarn.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"

namespace DTWarnEvents
{
	DTCat::ESoundId SoundOf(EDTTelegraphShape Shape)
	{
		switch (Shape)
		{
		case EDTTelegraphShape::Band:    return DTCat::ESoundId::Snd_Warn_Band;
		case EDTTelegraphShape::Arc:     return DTCat::ESoundId::Snd_Warn_Arc;
		case EDTTelegraphShape::AimLine: return DTCat::ESoundId::Snd_Warn_Line;
		default:                         return DTCat::ESoundId::Snd_Warn_Ring; // Ring, RingGap, SpiralLane, HazardRing
		}
	}

	void NewThisStep(TConstArrayView<FDTTelegraph> Was, TConstArrayView<FDTTelegraph> Now, TFunctionRef<void(DTCat::ESoundId Sound, int32 Count)> Raise)
	{
		constexpr int32 NumWarn = 4;
		const DTCat::ESoundId Order[NumWarn] = { DTCat::ESoundId::Snd_Warn_Line, DTCat::ESoundId::Snd_Warn_Band, DTCat::ESoundId::Snd_Warn_Ring, DTCat::ESoundId::Snd_Warn_Arc };
		int32 Fresh[NumWarn] = {};
		uint64 Taken = 0; // C.AUD.SCAN_MAX is at most 64 entries: one bit each
		static_assert(DTConst::AUD_SCAN_MAX <= 64, "the matched set is one 64-bit word");
		for (const FDTTelegraph& Tele : Now)
		{
			// The nearest of last step's unmatched telegraphs that fits; among equally near, the greatest Progress (see the header for why).
			int32 Best = INDEX_NONE;
			float BestDist = MatchUU * MatchUU;
			for (int32 Index = 0; Index < Was.Num(); ++Index)
			{
				const FDTTelegraph& Old = Was[Index];
				const float Dist = FVector2f::DistSquared(Old.Origin, Tele.Origin);
				if (!(Taken & (1ull << Index)) && Old.Shape == Tele.Shape && Old.Progress <= Tele.Progress + UE_KINDA_SMALL_NUMBER && Dist <= BestDist
					&& (Best == INDEX_NONE || Dist < BestDist || Old.Progress > Was[Best].Progress))
				{
					Best = Index;
					BestDist = Dist;
				}
			}
			if (Best != INDEX_NONE)
			{
				Taken |= 1ull << Best;
			}
			else
			{
				const DTCat::ESoundId Sound = SoundOf(Tele.Shape);
				for (int32 Slot = 0; Slot < NumWarn; ++Slot)
				{
					Fresh[Slot] += Order[Slot] == Sound ? 1 : 0;
				}
			}
		}
		for (int32 Slot = 0; Slot < NumWarn; ++Slot)
		{
			if (Fresh[Slot] > 0)
			{
				Raise(Order[Slot], Fresh[Slot]); // several new telegraphs of one shape in one step: one request carrying their count
			}
		}
	}
}

void FDTObserveWarn::OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out)
{
	UDTSimSubsystem* Sim = World.GetSubsystem<UDTSimSubsystem>();
	if (!Sim)
	{
		return;
	}
	// The public view, read and never written: at most C.AUD.SCAN_MAX entries (R-AUD-02).
	const TArray<FDTTelegraph>& Telegraphs = Sim->GetView().Telegraphs;
	Scan.Reset();
	Scan.Append(Telegraphs.GetData(), FMath::Min(Telegraphs.Num(), static_cast<int32>(DTConst::AUD_SCAN_MAX)));
	DTWarnEvents::NewThisStep(Last, Scan, [&Out, Step](DTCat::ESoundId Sound, int32 Count) { Out.Run(static_cast<int32>(Sound), Count, Step); });
	Swap(Last, Scan);
}
