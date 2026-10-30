// The S2 Keeper mechanics of the weapon system (keepers.md §9): Mark, Blind, hunters, Debt, crit reactions, novas, dash volleys,
// Momentum. Stubs until the failing-first tests have been read.
#include "Combat/DTWeaponSim.h"

void FDTWeaponSim::ApplyMark(int32 OwnerId, uint32 Serial, int64 StepIndex) {}
bool FDTWeaponSim::IsMarked(int32 OwnerId, uint32 Serial) const { return false; }
bool FDTWeaponSim::IsBlinded(int32 OwnerId, uint32 Serial) const { return false; }
void FDTWeaponSim::AbsorbStatuses(const FDTSimContext& Context) {}
void FDTWeaponSim::MarkCircle(const FDTSimContext& Context, int32 OwnerId, const FVector2f& Center, float Radius, uint16 SourceId) {}
int32 FDTWeaponSim::ExecuteMarked(const FDTSimContext& Context, int32 OwnerId, float Below, uint16 SourceId) { return 0; }
int32 FDTWeaponSim::GetLiveHunters(int32 OwnerId) const { return 0; }
void FDTWeaponSim::RaiseHunters(int32 OwnerId, int32 Count, const FVector2f& At, float LifeS, float DmgRatio) {}
int32 FDTWeaponSim::GetDebt(int32 OwnerId) const { return 0; }
void FDTWeaponSim::AddDebt(int32 OwnerId, int32 Debt) {}
bool FDTWeaponSim::IsPaydayReady(int32 OwnerId) const { return false; }
void FDTWeaponSim::ForcePayday(int32 OwnerId, float Mult, float ExecuteBelow) {}
float FDTWeaponSim::GetMomentum(int32 OwnerId) const { return 0.0f; }
void FDTWeaponSim::GrantMomentum(int32 OwnerId, float Stacks, bool bHold) {}
float FDTWeaponSim::TakeDashRefund(int32 OwnerId) { return 0.0f; }
int32 FDTWeaponSim::TakeDashRefills(int32 OwnerId) { return 0; }
float FDTWeaponSim::HpOf(int32 Enemy) const { return 0.0f; }
float FDTWeaponSim::BestCritMult(int32 OwnerId) const { return 1.0f; }
void FDTWeaponSim::AddFlash(int32 OwnerId, const FVector2f& Position, const FVector2f& Dir, float Reach, float ArcDeg, float Seconds) {}
