#include "Progression/DTProgression.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"

int32 FDTProgression::XPToNext(int32 Level)
{
	const int32 Clamped = FMath::Max(1, Level);
	check(DTCat::NumXpLevels > 0);
	if (Clamped <= DTCat::NumXpLevels)
	{
		return DTCat::XpCurve[Clamped - 1].XpToNext;
	}
	// Past the table: each step is the previous one x C.XP.GROWTH, rounded to the nearest integer (R-PRG-02).
	double Step = static_cast<double>(DTCat::XpCurve[DTCat::NumXpLevels - 1].XpToNext);
	const double Max = static_cast<double>(MAX_int32);
	for (int32 L = DTCat::NumXpLevels + 1; L <= Clamped && Step < Max; ++L)
	{
		Step = FMath::RoundToDouble(Step * static_cast<double>(DTConst::XP_GROWTH));
	}
	return static_cast<int32>(FMath::Min(Step, Max));
}

EDTHandKind FDTProgression::HandKindForLevel(int32 Level)
{
	if (Level == DTConst::OFFER_SECOND_KEEPER_LEVEL)
	{
		return EDTHandKind::Keeper;
	}
	if (Level == DTConst::OFFER_REFORGING_LEVEL)
	{
		return EDTHandKind::Reforging;
	}
	return EDTHandKind::Level;
}

void FDTProgression::EnsureCurve(FDTRunState& Run)
{
	for (int32 Index = 0; Index < Run.GetNumPlayers(); ++Index)
	{
		FDTPlayerRunState& Player = Run.GetPlayer(Index);
		if (Player.XPToNext <= 0)
		{
			Player.XPToNext = XPToNext(Player.Level);
		}
	}
}

void FDTProgression::QueueHand(FDTPlayerOfferState& Offer, EDTHandKind Kind, int32 Level)
{
	FDTOfferHand Hand;
	Hand.HandKind = Kind;
	Hand.Level = Level;
	if (Kind == EDTHandKind::Chest)
	{
		Offer.PendingHands.Add(MoveTemp(Hand));
		return;
	}
	const int32 FirstChest = Offer.PendingHands.IndexOfByPredicate([](const FDTOfferHand& H) { return H.HandKind == EDTHandKind::Chest; });
	if (FirstChest == INDEX_NONE)
	{
		Offer.PendingHands.Add(MoveTemp(Hand));
	}
	else
	{
		Offer.PendingHands.Insert(MoveTemp(Hand), FirstChest);
	}
}

int32 FDTProgression::GrantXP(FDTPlayerRunState& Player, int64 Amount, bool bCountTotal, TArray<FDTLevelGain>* OutLevels)
{
	if (Amount <= 0)
	{
		return 0;
	}
	if (bCountTotal)
	{
		Player.XPTotal += Amount;
	}
	if (Player.XPToNext <= 0)
	{
		Player.XPToNext = XPToNext(Player.Level);
	}
	// R-PRG-05: the overflow carries; one pickup may pass several levels; nothing is capped while hands are pending.
	int64 Pool = static_cast<int64>(Player.XP) + Amount;
	int32 Gained = 0;
	while (Pool >= Player.XPToNext)
	{
		Pool -= Player.XPToNext;
		++Player.Level;
		++Gained;
		Player.XPToNext = XPToNext(Player.Level);
		QueueHand(Player.Offer, HandKindForLevel(Player.Level), Player.Level);
		if (OutLevels)
		{
			OutLevels->Add({ Player.PlayerIndex, Player.Level });
		}
	}
	Player.XP = static_cast<int32>(Pool);
	return Gained;
}

int32 FDTProgression::AddXP(FDTRunState& Run, int32 OwnerId, int64 Amount, TArray<FDTLevelGain>* OutLevels)
{
	if (!Run.IsValidPlayer(OwnerId))
	{
		return 0;
	}
	return GrantXP(Run.GetPlayer(OwnerId), Amount, true, OutLevels);
}

bool FDTProgression::CreditPickup(FDTRunState& Run, int32 Collector, int32 Value, TConstArrayView<float> XpGainMul,
	TArray<FDTLevelGain>* OutLevels, TArray<int64>* OutCredits)
{
	if (!Run.IsValidPlayer(Collector))
	{
		return false;
	}
	const int32 NumPlayers = Run.GetNumPlayers();
	auto Credit = [&XpGainMul, Value](int32 Player) -> int64
	{
		const float Mul = XpGainMul.IsValidIndex(Player) ? XpGainMul[Player] : 1.0f;
		return FMath::Max<int64>(0, FMath::RoundToInt64(static_cast<double>(Value) * static_cast<double>(Mul)));
	};
	if (OutCredits)
	{
		OutCredits->SetNumZeroed(NumPlayers);
	}

	// The pickup event names the collector (R-PRG-01); ApplyEvent counts its amount into the collector's XPTotal.
	const int64 CollectorCredit = Credit(Collector);
	FDTGameEvent Event;
	Event.Kind = EDTGameEventKind::Pickup;
	Event.OwnerId = Collector;
	Event.Amount = static_cast<int32>(FMath::Min<int64>(CollectorCredit, MAX_int32));
	Run.ApplyEvent(Event);

	for (int32 Player = 0; Player < NumPlayers; ++Player)
	{
		const int64 Amount = Player == Collector ? CollectorCredit : Credit(Player);
		GrantXP(Run.GetPlayer(Player), Amount, Player != Collector, OutLevels);
		if (OutCredits)
		{
			(*OutCredits)[Player] = Amount;
		}
	}
	return true;
}

int64 FDTProgression::XPToReach(const FDTPlayerRunState& Player, int32 TargetLevel)
{
	if (TargetLevel <= Player.Level)
	{
		return 0;
	}
	int64 Need = static_cast<int64>(Player.XPToNext > 0 ? Player.XPToNext : XPToNext(Player.Level)) - Player.XP;
	for (int32 L = Player.Level + 1; L < TargetLevel; ++L)
	{
		Need += XPToNext(L);
	}
	return Need;
}
