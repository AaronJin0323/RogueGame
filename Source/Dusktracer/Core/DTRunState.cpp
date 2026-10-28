#include "Core/DTRunState.h"

void FDTRunState::Start(int32 RunSeed, int32 NumPlayers)
{
	Rng.Reset(RunSeed);
	Players.Reset();
	Players.SetNum(FMath::Max(1, NumPlayers));
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		Players[Index].PlayerIndex = Index;
	}
}

bool FDTRunState::ApplyEvent(const FDTGameEvent& Event)
{
	if (!IsValidPlayer(Event.OwnerId))
	{
		return false;
	}
	FDTPlayerRunState& Owner = Players[Event.OwnerId];
	switch (Event.Kind)
	{
	case EDTGameEventKind::Hit:
		Owner.DamageDealt += Event.Amount;
		break;
	case EDTGameEventKind::Kill:
		++Owner.Kills;
		break;
	case EDTGameEventKind::Pickup:
		++Owner.Pickups;
		Owner.Xp += Event.Amount;
		break;
	}
	return true;
}
