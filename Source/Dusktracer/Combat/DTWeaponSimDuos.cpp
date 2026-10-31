// Duo reactions (reactions.md): stub for the failing-first run.
#include "Combat/DTWeaponSim.h"

void FDTWeaponSim::SetReactions(int32 OwnerId, uint32 Live, uint32 Duos) {}

const FDTReactionCounters& FDTWeaponSim::GetReactionCounters(int32 OwnerId, int32 Row) const
{
	static const FDTReactionCounters None;
	return None;
}

TArray<FDTReactionEvent> FDTWeaponSim::TakeReactionEvents() { return TArray<FDTReactionEvent>(); }
int32 FDTWeaponSim::GetToast(int32 OwnerId, int64 StepIndex) const { return INDEX_NONE; }
float FDTWeaponSim::GetWildfireLeft(int32 OwnerId) const { return 0.0f; }
FDTDuoHero FDTWeaponSim::GetDuoHero(int32 OwnerId) const { return FDTDuoHero(); }
void FDTWeaponSim::ApplyBlind(int32 OwnerId, uint32 Serial, int64 StepIndex, float Seconds) {}
