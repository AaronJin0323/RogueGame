// Arena art debug console command (R-TEC-12: compiled out of Shipping; 02_Systems/arena_art.md R-ARN-02).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DTCameraPawn.h"
#include "HAL/IConsoleManager.h"

namespace DTDebugArena
{
	// Seconds a staged frame waits for the run's camera before the command gives up: a scene's commands run before the run starts.
	constexpr float FrameWaitSeconds = 60.0f;

	/** Moves the run's camera to the distance that frames Radius, aimed Shift from the hero toward it, at the game's pitch and lens.
	 *  False while no camera follows a hero yet. */
	static bool Apply(UWorld* World, float Radius, float Shift)
	{
		for (TActorIterator<ADTCameraPawn> It(World); It; ++It)
		{
			if (!It->GetAttachParentActor())
			{
				continue;
			}
			// The camera's own sum, one place for every frame (ADTCameraPawn::FramingTransform, D-080).
			It->SetActorRelativeTransform(ADTCameraPawn::FramingTransform(Radius, Shift));
			UE_LOG(LogDT, Log, TEXT("DT.Arena.Frame %.0f uu aimed %.1f uu toward the camera (a staged frame, not the game's frame)"), Radius, Shift);
			return true;
		}
		return false;
	}

	/**
	 * DT.Arena.Frame: a staged frame at another frame radius, for the edge frames of R-ARN-02 (the apron holds at the frame D-036
	 * reverts to as well as at today's). The game's frame is fixed (06_UX R-UX-02) and nothing in play calls this: it moves the view of
	 * a tool run only, reads no sim state and writes none.
	 */
	static void Frame(const TArray<FString>& Args, UWorld* World)
	{
		const float Radius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
		// Centred unless a shift is given, as the staged edge frames have always been (2000 uu, the frame before D-036).
		const float Shift = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		if (!World || !World->IsGameWorld() || Radius <= 0.0f)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Arena.Frame <radius uu>: game worlds only"));
			return;
		}
		if (Apply(World, Radius, Shift))
		{
			return;
		}
		const TWeakObjectPtr<UWorld> Weak(World);
		const double GiveUp = FPlatformTime::Seconds() + FrameWaitSeconds;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Radius, Shift, GiveUp](float)
		{
			UWorld* Live = Weak.Get();
			if (!Live || Apply(Live, Radius, Shift))
			{
				return false;
			}
			if (FPlatformTime::Seconds() > GiveUp)
			{
				UE_LOG(LogDT, Warning, TEXT("DT.Arena.Frame: no camera followed a hero in %.0f s"), FrameWaitSeconds);
				return false;
			}
			return true;
		}));
	}

	static FAutoConsoleCommandWithWorldAndArgs FrameCmd(TEXT("DT.Arena.Frame"),
		TEXT("DT.Arena.Frame <radius uu> [aim shift uu]: stage the camera at another frame radius, centred unless a shift is given (edge frames of the arena art; tool runs only)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Frame));
}

#endif // !UE_BUILD_SHIPPING
