// The whole simulation advances in fixed steps from an accumulator (07_Technical R-TEC-03).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"

/**
 * Turns variable frame times into a whole number of fixed steps. At most MaxSubsteps run per frame; time beyond that is
 * dropped (the game slows down during a hitch instead of spiralling) and counted. The leftover fraction of a step is
 * carried to the next frame and exposed as the render interpolation alpha.
 */
class DUSKTRACER_API FDTFixedStep
{
public:
	explicit FDTFixedStep(int32 InStepsPerSecond = DTConst::SIM_FIXED_HZ, int32 InMaxSubsteps = DTConst::SIM_MAX_SUBSTEPS);

	/** Adds a frame's time and returns how many fixed steps to run now. Negative or non-finite times count as zero. */
	int32 Advance(double FrameSeconds);

	double GetStepSeconds() const { return StepSeconds; }
	/** Fraction of a step carried to the next frame, in [0, 1): the render interpolation weight. */
	float GetAlpha() const { return static_cast<float>(Accumulator / StepSeconds); }
	double GetAccumulator() const { return Accumulator; }
	int64 GetTotalSteps() const { return TotalSteps; }
	int64 GetDroppedSteps() const { return DroppedSteps; }

private:
	double StepSeconds;
	int32 MaxSubsteps;
	double Accumulator = 0.0;
	int64 TotalSteps = 0;
	int64 DroppedSteps = 0;
};
