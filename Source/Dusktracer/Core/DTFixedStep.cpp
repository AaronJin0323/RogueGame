#include "Core/DTFixedStep.h"

FDTFixedStep::FDTFixedStep(int32 InStepsPerSecond, int32 InMaxSubsteps)
	: StepSeconds(1.0 / FMath::Max(1, InStepsPerSecond))
	, MaxSubsteps(FMath::Max(1, InMaxSubsteps))
{
}

int32 FDTFixedStep::Advance(double FrameSeconds)
{
	if (!FMath::IsFinite(FrameSeconds) || FrameSeconds < 0.0)
	{
		FrameSeconds = 0.0;
	}
	Accumulator += FrameSeconds;

	// A small tolerance keeps an exact multiple of the step (e.g. 1/60 s frames at 60 Hz) from losing a step to rounding.
	const double Tolerance = StepSeconds * 1e-6;
	int32 Steps = FMath::FloorToInt32((Accumulator + Tolerance) / StepSeconds);
	Accumulator = FMath::Max(0.0, Accumulator - Steps * StepSeconds);

	if (Steps > MaxSubsteps)
	{
		DroppedSteps += Steps - MaxSubsteps;
		Steps = MaxSubsteps;
	}
	TotalSteps += Steps;
	return Steps;
}
