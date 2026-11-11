#include "Combat/DTKnockback.h"

void DTKnockback::Push(FDTWorldView& View, int32 OwnerId, int32 Enemy, const FVector2f& Center, const FVector2f& Fallback, float DistanceUu)
{
	if (DistanceUu <= 0.0f || !View.EnemyPositions.IsValidIndex(Enemy) || !View.EnemySerials.IsValidIndex(Enemy))
	{
		return;
	}
	const FVector2f Away = View.EnemyPositions[Enemy] - Center;
	FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
	Move.EnemyIndex = Enemy;
	Move.EnemySerial = View.EnemySerials[Enemy];
	Move.Delta = (Away.IsNearlyZero() ? Fallback : Away.GetSafeNormal()) * DistanceUu;
	Move.OwnerId = OwnerId;
}
