#include "Presentation/DTTracerArt.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Keepers/DTKeeperSpec.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"
#include "Player/DTHeroSubsystem.h"

namespace DTTracerArtLocal
{
	/** Rotation about +Z that turns +X onto the unit direction Dir, without a trigonometric call. */
	static FQuat YawTo(const FVector2f& Dir)
	{
		const float HalfCos = FMath::Sqrt(FMath::Max(0.0f, 0.5f * (1.0f + Dir.X)));
		const float HalfSin = FMath::Sqrt(FMath::Max(0.0f, 0.5f * (1.0f - Dir.X)));
		return FQuat(0.0, 0.0, Dir.Y < 0.0f ? -HalfSin : HalfSin, HalfCos);
	}

	static float Lerp01(float Full, float T)
	{
		return FMath::Lerp(1.0f, Full, T);
	}

	/** A plane instance Length long and Width wide whose +X end sits at Head and whose body runs back along -Dir. */
	static FTransform Quad(const FVector2f& Head, const FVector2f& Dir, float Length, float Width, float InvPlane, double Z)
	{
		const FVector2f Mid = Head - Dir * (0.5f * Length);
		return FTransform(YawTo(Dir), FVector(Mid.X, Mid.Y, Z), FVector(Length * InvPlane, Width * InvPlane, 1.0f));
	}
}

// ---- tokens -----------------------------------------------------------------------------------------------------------------

FColor DTTracerArt::HeadToken(FName WeaponId, const FDTKeeperRunState& Keepers)
{
	// R-PRE-01: no Keeper, effect, modifier or weapon recolours the core.
	return DTPal::Player_Core;
}

int32 DTTracerArt::BodySlot(const FDTKeeperRunState& Keepers)
{
	const bool bHeld = Keepers.Primary != EDTPath::None && Keepers.Primary < EDTPath::Count && DTKeepers::OwnedOfPath(Keepers, Keepers.Primary) > 0;
	return bHeld ? static_cast<int32>(Keepers.Primary) : NeutralSlot;
}

int32 DTTracerArt::TipSlot(const FDTKeeperRunState& Keepers)
{
	const int32 Body = BodySlot(Keepers);
	const bool bSecond = Body != NeutralSlot && Keepers.bCommitDone && Keepers.Second != EDTPath::None && Keepers.Second < EDTPath::Count;
	return bSecond ? static_cast<int32>(Keepers.Second) : Body;
}

FColor DTTracerArt::SlotToken(int32 Slot)
{
	return Slot >= 0 && Slot < NeutralSlot ? DTKeepers::TrailToken(static_cast<EDTPath>(Slot)) : DTPal::Player_TrailNeutral;
}

// ---- density ----------------------------------------------------------------------------------------------------------------

float DTTracerArt::Density(int32 LivePlayerRounds)
{
	const float Start = static_cast<float>(DTConst::TRC_DIM_START_ROUNDS);
	const float Full = static_cast<float>(DTConst::TRC_DIM_FULL_ROUNDS);
	return FMath::Clamp((static_cast<float>(LivePlayerRounds) - Start) / FMath::Max(1.0f, Full - Start), 0.0f, 1.0f);
}

float DTTracerArt::HeadDim(int32 LivePlayerRounds)
{
	return DTTracerArtLocal::Lerp01(DTConst::TRC_DIM_EMISSIVE_MIN, Density(LivePlayerRounds));
}

float DTTracerArt::TailDim(int32 LivePlayerRounds)
{
	return DTTracerArtLocal::Lerp01(DTConst::TRC_DIM_TAIL_MIN, Density(LivePlayerRounds));
}

float DTTracerArt::TailWidthDim(int32 LivePlayerRounds)
{
	return DTTracerArtLocal::Lerp01(DTConst::TRC_DIM_WIDTH_MIN, Density(LivePlayerRounds));
}

// ---- shapes -----------------------------------------------------------------------------------------------------------------

FTransform DTTracerArt::Head(const FVector2f& At, const FVector2f& Step, float HitRadius, float InvMesh, double Z)
{
	const float Half = HitRadius * static_cast<float>(DTConst::TRC_HEAD_WIDTH_RATIO) * InvMesh;
	const float Travel = Step.Size();
	if (Travel <= UE_KINDA_SMALL_NUMBER)
	{
		return FTransform(FQuat::Identity, FVector(At.X, At.Y, Z), FVector(Half));
	}
	return FTransform(DTTracerArtLocal::YawTo(Step / Travel), FVector(At.X, At.Y, Z), FVector(Half * static_cast<float>(DTConst::TRC_HEAD_STRETCH), Half, Half));
}

float DTTracerArt::TailLength(float SpeedUUPerS, float HitRadius)
{
	const float Shortest = HitRadius * static_cast<float>(DTConst::TRC_HEAD_WIDTH_RATIO * DTConst::TRC_TAIL_MIN_RATIO);
	const float Longest = FMath::Max(Shortest, static_cast<float>(DTConst::TRC_TAIL_MAX_UU));
	return FMath::Clamp(SpeedUUPerS * static_cast<float>(DTConst::TRC_TAIL_S), Shortest, Longest);
}

bool DTTracerArt::Tail(const FVector2f& At, const FVector2f& Step, float HitRadius, float WidthDim, float InvPlane, double Z, FTransform& Out)
{
	const float Travel = Step.Size();
	if (Travel <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const float Width = 2.0f * HitRadius * static_cast<float>(DTConst::TRC_HEAD_WIDTH_RATIO * DTConst::TRC_TAIL_WIDTH_RATIO) * WidthDim;
	Out = DTTracerArtLocal::Quad(At, Step / Travel, TailLength(Travel * static_cast<float>(DTConst::SIM_FIXED_HZ), HitRadius), Width, InvPlane, Z);
	return true;
}

bool DTTracerArt::EnemyTrail(const FVector2f& At, const FVector2f& Velocity, float HitRadius, float InvPlane, double Z, FTransform& Out)
{
	const float Speed = Velocity.Size();
	const float Drawn = HitRadius * static_cast<float>(DTConst::ENEMY_FIRE_DRAW_RADIUS_RATIO);
	const float Flown = Speed * static_cast<float>(DTConst::TRC_ENEMY_TRAIL_S);
	if (Speed <= UE_KINDA_SMALL_NUMBER || Flown <= Drawn)
	{
		return false; // too slow for a trail worth its round: shorter than the round is wide
	}
	// From the round's centre, so no gap opens at its rear edge from any camera; the round hides the first drawn radius of it.
	Out = DTTracerArtLocal::Quad(At, Velocity / Speed, Drawn + Flown, 2.0f * Drawn * static_cast<float>(DTConst::TRC_ENEMY_TRAIL_WIDTH_RATIO), InvPlane, Z);
	return true;
}

float DTTracerArt::RoundAge(const FVector2f& Origin, const FVector2f& At, const FVector2f& Velocity)
{
	const float Speed = Velocity.Size();
	return Speed > UE_KINDA_SMALL_NUMBER ? FVector2f::Distance(Origin, At) / Speed : -1.0f;
}

bool DTTracerArt::MuzzleFlash(const FVector2f& Origin, const FVector2f& At, const FVector2f& Velocity, float HitRadius, float InvPlane, double Z, FTransform& Out)
{
	const float Age = RoundAge(Origin, At, Velocity);
	const float Lasts = static_cast<float>(DTConst::TRC_MUZZLE_S);
	if (Age < 0.0f || Age >= Lasts)
	{
		return false;
	}
	const float Left = 1.0f - Age / Lasts;
	const float Drawn = HitRadius * static_cast<float>(DTConst::ENEMY_FIRE_DRAW_RADIUS_RATIO);
	const float Length = Drawn * static_cast<float>(DTConst::TRC_MUZZLE_LENGTH_RATIO) * Left;
	const FVector2f Dir = Velocity.GetSafeNormal();
	// The bright end at the muzzle, the point down the flight: the plane's +X looks back at the muzzle. The muzzle is the edge of the hull
	// that fired, a Turret's radius down the flight from the firing point at its centre: a flash at the centre would lie under the hull.
	const FVector2f Muzzle = Origin + Dir * static_cast<float>(DTConst::ROLE_TURRET_RADIUS_UU);
	Out = DTTracerArtLocal::Quad(Muzzle, -Dir, Length, 2.0f * Drawn * static_cast<float>(DTConst::TRC_MUZZLE_WIDTH_RATIO) * Left, InvPlane, Z);
	return true;
}

// ---- the hero's clear zone ------------------------------------------------------------------------------------------------------

float& DTTracerArt::ClearZoneSetting()
{
	static float Setting = -1.0f;
	return Setting;
}

float DTTracerArt::ClearZoneRatio()
{
#if !UE_BUILD_SHIPPING
	const float Setting = ClearZoneSetting();
	if (Setting >= 0.0f)
	{
		return Setting;
	}
#endif
	return static_cast<float>(DTConst::TRC_CLEAR_ZONE_RATIO);
}

DTTracerArt::FClearZone::FClearZone(TConstArrayView<FVector2f> InHeroes, float Ratio)
{
	if (Ratio > 0.0f)
	{
		const float Radius = Ratio * static_cast<float>(DTConst::HERO_RADIUS_UU);
		RadiusSq = Radius * Radius;
		Heroes.Append(InHeroes.GetData(), InHeroes.Num());
	}
}

DTTracerArt::FClearZone DTTracerArt::FClearZone::OfWorld(const UWorld* World, float Alpha)
{
	const float Ratio = ClearZoneRatio();
	const UDTHeroSubsystem* HeroSystem = World ? World->GetSubsystem<UDTHeroSubsystem>() : nullptr;
	UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
	if (Ratio <= 0.0f || !HeroSystem || !Sim)
	{
		return FClearZone();
	}
	// The heroes as drawn this frame (UDTHeroSubsystem interpolates them the same way): read once per frame, never written.
	TArray<FVector2f, TInlineAllocator<4>> Drawn;
	for (int32 Player = 0; Player < Sim->GetRun().GetNumPlayers(); ++Player)
	{
		if (const FDTHeroState* State = HeroSystem->GetState(Player))
		{
			Drawn.Add(FMath::Lerp(State->PrevPos, State->Pos, Alpha));
		}
		else
		{
			Drawn.Add(FVector2f(UE_BIG_NUMBER, UE_BIG_NUMBER)); // no hero: a zone nothing reaches
		}
	}
	return FClearZone(Drawn, Ratio);
}
// ---- instances --------------------------------------------------------------------------------------------------------------

void DTTracerArt::SyncInstances(UInstancedStaticMeshComponent* Part, TConstArrayView<FTransform> Transforms, TConstArrayView<float> Row)
{
	if (!Part)
	{
		return;
	}
	const int32 Wanted = Transforms.Num();
	const int32 Have = Part->GetInstanceCount();
	if (Wanted > Have)
	{
		const TArray<FTransform> Added(Transforms.GetData() + Have, Wanted - Have);
		Part->AddInstances(Added, false, false, false);
		const int32 Stride = Part->NumCustomDataFloats;
		if (Stride > 0 && Row.Num() >= Stride)
		{
			TArray<float> Data;
			Data.SetNumUninitialized((Wanted - Have) * Stride);
			for (int32 Slot = 0; Slot < Data.Num(); ++Slot)
			{
				Data[Slot] = Row[Slot % Stride];
			}
			Part->SetCustomData(Have, Wanted - 1, Data, false);
		}
	}
	else if (Wanted < Have)
	{
		TArray<int32> Removed;
		Removed.Reserve(Have - Wanted);
		for (int32 Index = Have - 1; Index >= Wanted; --Index)
		{
			Removed.Add(Index);
		}
		Part->RemoveInstances(Removed, true);
	}
	if (Wanted > 0)
	{
		Part->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
	}
	else if (Have > 0)
	{
		Part->MarkRenderStateDirty();
	}
}
