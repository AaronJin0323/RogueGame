#include "Presentation/DTHeroPose.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Player/DTHeroTypes.h"

namespace DTHeroPoseLocal
{
	struct FPartInfo
	{
		const TCHAR* Name;
		const TCHAR* Mesh;
	};

	// Asset paths (not tunables), in EDTHeroPart order.
	static const FPartInfo Parts[DTHeroPose::NumParts] = {
		{ TEXT("Torso"), TEXT("/Game/DT/Meshes/SM_Hero_Torso.SM_Hero_Torso") },
		{ TEXT("Pauldrons"), TEXT("/Game/DT/Meshes/SM_Hero_Pauldrons.SM_Hero_Pauldrons") },
		{ TEXT("Hood"), TEXT("/Game/DT/Meshes/SM_Hero_Hood.SM_Hero_Hood") },
		{ TEXT("Visor"), TEXT("/Game/DT/Meshes/SM_Hero_Visor.SM_Hero_Visor") },
		{ TEXT("Undersuit"), TEXT("/Game/DT/Meshes/SM_Hero_Undersuit.SM_Hero_Undersuit") },
		{ TEXT("Hips"), TEXT("/Game/DT/Meshes/SM_Hero_Hips.SM_Hero_Hips") },
		{ TEXT("Belt"), TEXT("/Game/DT/Meshes/SM_Hero_Belt.SM_Hero_Belt") },
		{ TEXT("ArmL"), TEXT("/Game/DT/Meshes/SM_Hero_ArmL.SM_Hero_ArmL") },
		{ TEXT("ArmR"), TEXT("/Game/DT/Meshes/SM_Hero_ArmR.SM_Hero_ArmR") },
		{ TEXT("LegL"), TEXT("/Game/DT/Meshes/SM_Hero_LegL.SM_Hero_LegL") },
		{ TEXT("LegR"), TEXT("/Game/DT/Meshes/SM_Hero_LegR.SM_Hero_LegR") },
	};

	constexpr float LeftSide = -1.0f;
	constexpr float RightSide = 1.0f;

	/** V at unit length, or Fallback when V has no direction. */
	static FVector2f UnitOr(const FVector2f& V, const FVector2f& Fallback)
	{
		const FVector2f Unit = V.GetSafeNormal();
		return Unit.IsNearlyZero() ? Fallback : Unit;
	}

	/** A turn of Degrees that tips the figure's up axis toward Dir (a unit vector on the floor); negative tips it away. */
	static FQuat TipToward(const FVector& Dir, float Degrees)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		const FVector Tipped = FVector::UpVector * FMath::Cos(Radians) + Dir * FMath::Sin(Radians);
		return FQuat::FindBetweenNormals(FVector::UpVector, Tipped);
	}

	/** The turn applied about a fixed point: the joint stays where it is. */
	static FTransform TurnAbout(const FVector& Joint, const FQuat& Turn)
	{
		return FTransform(Turn, Joint - Turn.RotateVector(Joint));
	}
}

FDTHeroPoseInput DTHeroPose::FromState(const FDTHeroState& State)
{
	FDTHeroPoseInput Input;
	Input.AimDir = State.AimDir;
	Input.Vel = State.Vel;
	Input.DashDir = State.DashDir;
	Input.bDashing = State.DashLeft > 0.0f;
	return Input;
}

const TCHAR* DTHeroPose::MeshPath(EDTHeroPart Part)
{
	return DTHeroPoseLocal::Parts[static_cast<int32>(Part)].Mesh;
}

const TCHAR* DTHeroPose::PartName(EDTHeroPart Part)
{
	return DTHeroPoseLocal::Parts[static_cast<int32>(Part)].Name;
}

FColor DTHeroPose::Token(EDTHeroPart Part)
{
	switch (Part)
	{
	case EDTHeroPart::Visor:
	case EDTHeroPart::Belt:
		return DTPal::Hero_DarkMetal;
	case EDTHeroPart::Undersuit:
		return DTPal::Hero_Undersuit;
	default:
		return DTPal::Hero_Armour;
	}
}

FVector DTHeroPose::Waist()
{
	return FVector(0.0, 0.0, static_cast<double>(DTConst::HERO_ART_WAIST_Z_UU));
}

FVector DTHeroPose::Neck()
{
	return FVector(0.0, 0.0, static_cast<double>(DTConst::HERO_ART_NECK_Z_UU));
}

FVector DTHeroPose::Shoulder(float Side)
{
	return FVector(0.0, static_cast<double>(Side) * static_cast<double>(DTConst::HERO_ART_SHOULDER_Y_UU), static_cast<double>(DTConst::HERO_ART_SHOULDER_Z_UU));
}

FVector DTHeroPose::Hip(float Side)
{
	return FVector(0.0, static_cast<double>(Side) * static_cast<double>(DTConst::HERO_ART_HIP_Y_UU), static_cast<double>(DTConst::HERO_ART_HIP_Z_UU));
}

FDTHeroPose DTHeroPose::Evaluate(const FDTHeroPoseInput& Input)
{
	using namespace DTHeroPoseLocal;

	const FVector2f Aim = UnitOr(Input.AimDir, FVector2f(1.0f, 0.0f));
	const float YawRadians = FMath::Atan2(Aim.Y, Aim.X);
	// The figure's own space: +X is where it faces. A world direction is turned back by the facing to be read in it.
	const auto ToFigure = [&Aim](const FVector2f& World)
	{
		return FVector(static_cast<double>(World.X * Aim.X + World.Y * Aim.Y), static_cast<double>(World.Y * Aim.X - World.X * Aim.Y), 0.0);
	};

	float Lean = 0.0f;
	float Drop = 0.0f;
	float LegLeft = 0.0f;
	float LegRight = 0.0f;
	float ArmLeft = 0.0f;
	float ArmRight = 0.0f;
	FVector Along = FVector::ForwardVector;
	if (Input.bDashing)
	{
		// Dash: the body thrown along the dash, both legs and both arms trailing, the hips dropped.
		Along = ToFigure(UnitOr(Input.DashDir, Aim));
		Lean = static_cast<float>(DTConst::HERO_ART_DASH_LEAN_DEG);
		Drop = static_cast<float>(DTConst::HERO_ART_DASH_DROP_UU);
		LegLeft = LegRight = -static_cast<float>(DTConst::HERO_ART_DASH_LEG_DEG);
		ArmLeft = ArmRight = -static_cast<float>(DTConst::HERO_ART_DASH_ARM_DEG);
	}
	else
	{
		// Moving: one held stride along the velocity, deeper with speed. No cycle: the pose has no clock.
		const float Speed = Input.Vel.Size();
		const float Move01 = FMath::Clamp(Speed / static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S), 0.0f, 1.0f);
		if (Move01 > 0.0f)
		{
			Along = ToFigure(Input.Vel / Speed);
		}
		Lean = Move01 * static_cast<float>(DTConst::HERO_ART_MOVE_LEAN_DEG);
		LegLeft = Move01 * static_cast<float>(DTConst::HERO_ART_MOVE_STRIDE_DEG);
		LegRight = -LegLeft;
		ArmRight = Move01 * static_cast<float>(DTConst::HERO_ART_MOVE_ARM_DEG);
		ArmLeft = -ArmRight;
	}

	// Figure space, pelvis outward. A * B applies A first.
	const FTransform Pelvis(FQuat::Identity, FVector(0.0, 0.0, -static_cast<double>(Drop)));
	const FTransform Upper = TurnAbout(Waist(), TipToward(Along, Lean)) * Pelvis;
	const FTransform Head = TurnAbout(Neck(), TipToward(Along, -Lean * static_cast<float>(DTConst::HERO_ART_HEAD_LEVEL_RATIO))) * Upper;
	// A limb hangs from its joint: its far end swings forward when the joint's up axis is tipped back.
	const FTransform ArmL = TurnAbout(Shoulder(LeftSide), TipToward(Along, -ArmLeft)) * Upper;
	const FTransform ArmR = TurnAbout(Shoulder(RightSide), TipToward(Along, -ArmRight)) * Upper;
	const FTransform LegL = TurnAbout(Hip(LeftSide), TipToward(Along, -LegLeft)) * Pelvis;
	const FTransform LegR = TurnAbout(Hip(RightSide), TipToward(Along, -LegRight)) * Pelvis;

	const double Scale = static_cast<double>(DTConst::HERO_ART_DRAW_SCALE);
	const FTransform Root(FQuat(FVector::UpVector, static_cast<double>(YawRadians)), FVector::ZeroVector, FVector(Scale));

	FDTHeroPose Pose;
	const auto Set = [&Pose, &Root](EDTHeroPart Part, const FTransform& InFigure)
	{
		Pose.Parts[static_cast<int32>(Part)] = InFigure * Root;
	};
	Set(EDTHeroPart::Torso, Upper);
	Set(EDTHeroPart::Pauldrons, Upper);
	Set(EDTHeroPart::Undersuit, Upper);
	Set(EDTHeroPart::Hood, Head);
	Set(EDTHeroPart::Visor, Head);
	Set(EDTHeroPart::Hips, Pelvis);
	Set(EDTHeroPart::Belt, Pelvis);
	Set(EDTHeroPart::ArmL, ArmL);
	Set(EDTHeroPart::ArmR, ArmR);
	Set(EDTHeroPart::LegL, LegL);
	Set(EDTHeroPart::LegR, LegR);
	return Pose;
}
