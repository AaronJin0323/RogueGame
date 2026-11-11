#include "Audio/DTAudioScheduler.h"
#include "Core/DTConstantsGen.h"

static_assert(DTAudio::KeyDegrees[UE_ARRAY_COUNT(DTAudio::KeyDegrees) - 1] == DTConst::AUD_KEY_SEMIS_MAX, "the key's top degree is C.AUD.KEY_SEMIS_MAX");

namespace DTAudioSchedulerLocal
{
	constexpr int64 DTAudNever = TNumericLimits<int64>::Lowest() / 2;

	/** Whole steps that cover Ms milliseconds, at least one. */
	static int64 StepsOf(double Ms)
	{
		return FMath::Max<int64>(1, static_cast<int64>(FMath::CeilToDouble(Ms * DTConst::SIM_FIXED_HZ / 1000.0)));
	}

	static int32 SideRank(DTCat::ESoundSide Side)
	{
		return Side == DTCat::ESoundSide::Danger ? 0 : (Side == DTCat::ESoundSide::UI ? 1 : 2);
	}
}

void FDTAudioScheduler::Configure(TConstArrayView<DTCat::FSoundRow> InRows, uint64 Seed)
{
	Rows = TArray<DTCat::FSoundRow>(InRows.GetData(), InRows.Num());
	Counts.Init(FDTAudioCounts(), Rows.Num());
	Reset(Seed);
}

void FDTAudioScheduler::Reset(uint64 Seed)
{
	using namespace DTAudioSchedulerLocal;
	LastStartStep.Init(DTAudNever, Rows.Num());
	LastStartTime.Init(-1.0e9, Rows.Num());
	LastVariant.Init(INDEX_NONE, Rows.Num());
	// What still waits belongs to the run that ended: dropped, so the counts still add up (R-AUD-20).
	for (const TArray<FBatch>* Left : { &Pending, &Waiting })
	{
		for (const FBatch& Batch : *Left)
		{
			Counts[Batch.Sound].Dropped += Batch.Count;
		}
	}
	Pending.Reset();
	Waiting.Reset();
	Alive.Reset();
	PlayerStarts.Reset();
	StartsThisFrame = 0;
	Stream = Seed;
	DuckStart = -1.0e9;
	DuckHoldUntil = -1.0e9;
	DuckFrom = 0.0f;
	DuckDb = 0.0f;
}

void FDTAudioScheduler::Request(int32 Sound, int32 Count, int64 Step)
{
	if (!Rows.IsValidIndex(Sound) || Count <= 0)
	{
		return;
	}
	Counts[Sound].Asked += Count;
	FBatch Batch;
	Batch.Sound = Sound;
	Batch.Count = Count;
	Batch.Step = Step;
	const int64 Grid = FMath::Max(1, DTConst::AUD_GRID_STEPS);
	Batch.Due = Rows[Sound].bGrid && !IsDanger(Sound) ? ((Step + Grid - 1) / Grid) * Grid : Step;
	Pending.Add(Batch);
}

void FDTAudioScheduler::RequestUi(int32 Sound, int32 Count)
{
	Request(Sound, Count, 0);
}

int64 FDTAudioScheduler::IntervalSteps(int32 Sound) const
{
	return Rows[Sound].IntervalMs > 0 ? DTAudioSchedulerLocal::StepsOf(Rows[Sound].IntervalMs) : 0;
}

int64 FDTAudioScheduler::LengthSteps(int32 Sound, float Pitch) const
{
	return DTAudioSchedulerLocal::StepsOf(Rows[Sound].LengthMs / FMath::Max(0.1f, Pitch));
}

int32 FDTAudioScheduler::NumAlive(DTCat::ESoundSide Side) const
{
	int32 N = 0;
	for (const FLive& Live : Alive)
	{
		N += Live.Side == Side ? 1 : 0;
	}
	return N;
}

void FDTAudioScheduler::Expire(double Now, int64 Step)
{
	Alive.RemoveAll([Now, Step](const FLive& Live) { return Live.bRealTime ? Live.EndTime <= Now : Live.EndStep <= Step; });
	PlayerStarts.RemoveAll([Step](int64 Started) { return Started <= Step - DTConst::SIM_FIXED_HZ; });
}

bool FDTAudioScheduler::FreeSlotFor(DTCat::ESoundSide Side) const
{
	const int32 Total = Alive.Num();
	const int32 KeptDanger = FMath::Max(0, DTConst::AUD_VOICES_DANGER - NumAlive(DTCat::ESoundSide::Danger));
	const int32 KeptUi = FMath::Max(0, DTConst::AUD_VOICES_UI - NumAlive(DTCat::ESoundSide::UI));
	switch (Side)
	{
	case DTCat::ESoundSide::Danger: return Total < DTConst::AUD_VOICES_MAX;
	case DTCat::ESoundSide::UI:     return Total + KeptDanger < DTConst::AUD_VOICES_MAX;
	default:                        return Total + KeptDanger + KeptUi < DTConst::AUD_VOICES_MAX;
	}
}

float FDTAudioScheduler::NextUnit()
{
	return static_cast<float>(static_cast<double>(DTAudio::SplitMix(Stream) >> 11) * (1.0 / 9007199254740992.0));
}

void FDTAudioScheduler::Frame(double Now, int64 Step, TArray<FDTAudioVoice>& Out)
{
	StartsThisFrame = 0;
	Expire(Now, Step);

	// One batch per id and step (per id for the interface: one frame), in the order asked.
	TArray<FBatch> Batches = MoveTemp(Waiting);
	Waiting.Reset();
	for (const FBatch& Asked : Pending)
	{
		FBatch* Same = Batches.FindByPredicate([&](const FBatch& B)
		{
			return !B.bWaited && B.Sound == Asked.Sound && (IsUi(Asked.Sound) || B.Step == Asked.Step);
		});
		if (Same)
		{
			Same->Count += Asked.Count;
		}
		else
		{
			FBatch Fresh = Asked;
			Fresh.Time = Now;
			Batches.Add(Fresh);
		}
	}
	Pending.Reset();
	Batches.StableSort([this](const FBatch& A, const FBatch& B)
	{
		return DTAudioSchedulerLocal::SideRank(Rows[A.Sound].Side) < DTAudioSchedulerLocal::SideRank(Rows[B.Sound].Side);
	});
	for (FBatch& Batch : Batches)
	{
		if (!Resolve(Batch, Now, Step, Out))
		{
			Waiting.Add(Batch);
		}
	}
	UpdateDuck(Now);
}

bool FDTAudioScheduler::Resolve(FBatch& Batch, double Now, int64 Step, TArray<FDTAudioVoice>& Out)
{
	const DTCat::FSoundRow& Row = Rows[Batch.Sound];
	FDTAudioCounts& Count = Counts[Batch.Sound];
	const bool bDanger = IsDanger(Batch.Sound);
	const bool bUi = IsUi(Batch.Sound);

	// The grid (R-AUD-09): only the sound waits.
	if (Step < Batch.Due)
	{
		return false;
	}
	const int64 StartStep = FMath::Max(Batch.Step, Batch.Due);
	// Inside the interval: merged into the voice that sounds, never carried (R-AUD-06).
	const bool bInside = bUi ? Now - LastStartTime[Batch.Sound] < Row.IntervalMs / 1000.0
		: StartStep - LastStartStep[Batch.Sound] < IntervalSteps(Batch.Sound);
	if (bInside)
	{
		Count.Merged += Batch.Count;
		return true;
	}
	int32 Own = 0;
	for (const FLive& Live : Alive)
	{
		Own += Live.Sound == Batch.Sound ? 1 : 0;
	}
	if (Own >= FMath::Max(1, Row.Voices))
	{
		Count.Dropped += Batch.Count;
		return true;
	}
	// The per-frame cap, danger first: danger never waits; anything else waits one frame, then drops.
	if (!bDanger && StartsThisFrame >= DTConst::AUD_STARTS_PER_FRAME_MAX)
	{
		if (Batch.bWaited)
		{
			Count.Dropped += Batch.Count;
			return true;
		}
		Batch.bWaited = true;
		return false;
	}
	if (!bDanger && !bUi && PlayerStarts.Num() >= DTConst::AUD_PLAYER_STARTS_PER_S_MAX)
	{
		Count.Dropped += Batch.Count;
		return true;
	}
	if (!FreeSlotFor(Row.Side))
	{
		int32 Oldest = INDEX_NONE;
		if (bDanger)
		{
			for (int32 Index = 0; Index < Alive.Num(); ++Index)
			{
				if (Alive[Index].Side == DTCat::ESoundSide::Player && (Oldest == INDEX_NONE || Alive[Index].StartOrder < Alive[Oldest].StartOrder))
				{
					Oldest = Index;
				}
			}
		}
		if (Oldest == INDEX_NONE)
		{
			Count.Dropped += Batch.Count;
			return true;
		}
		Alive.RemoveAt(Oldest);
		++Stolen;
	}
	FBatch Started = Batch;
	Started.Step = StartStep;
	Start(Started, Now, Step, Out);
	return true;
}

void FDTAudioScheduler::Start(const FBatch& Batch, double Now, int64 Step, TArray<FDTAudioVoice>& Out)
{
	const DTCat::FSoundRow& Row = Rows[Batch.Sound];
	const bool bDanger = IsDanger(Batch.Sound);
	FDTAudioVoice Voice;
	Voice.Sound = Batch.Sound;
	Voice.Count = Batch.Count;
	Voice.Step = IsUi(Batch.Sound) ? Step : Batch.Step;
	Voice.Time = Now;
	// Variants in turn (R-AUD-10).
	const int32 Variants = FMath::Max(1, Row.Variants);
	Voice.Variant = (LastVariant[Batch.Sound] + 1) % Variants;
	LastVariant[Batch.Sound] = Voice.Variant;
	float JitterDb = 0.0f;
	if (!bDanger && Row.Pitch == DTCat::ESoundPitch::Key)
	{
		constexpr int32 NumDegrees = static_cast<int32>(UE_ARRAY_COUNT(DTAudio::KeyDegrees));
		const int32 Degree = DTAudio::KeyDegrees[FMath::Min(NumDegrees - 1, static_cast<int32>(NextUnit() * NumDegrees))];
		Voice.Pitch = FMath::Pow(2.0f, Degree / 12.0f);
		JitterDb = (NextUnit() * 2.0f - 1.0f) * DTConst::AUD_LEVEL_JITTER_DB;
	}
	// `ladder` (the gem streak) plays as written until its family brings its step rule.
	const float Aggregate = FMath::Min(1.0f + Row.AggGain * FMath::Log2(static_cast<float>(FMath::Max(1, Batch.Count))), DTConst::AUD_AGGREGATE_MAX);
	Voice.Volume = Row.Volume * Aggregate * FMath::Pow(10.0f, JitterDb / 20.0f);

	FLive Live;
	Live.Sound = Batch.Sound;
	Live.Side = Row.Side;
	Live.bRealTime = IsUi(Batch.Sound);
	Live.EndStep = Batch.Step + LengthSteps(Batch.Sound, Voice.Pitch);
	Live.EndTime = Now + Row.LengthMs / 1000.0 / FMath::Max(0.1f, Voice.Pitch);
	Live.StartOrder = StartOrder++;
	Alive.Add(Live);

	if (bDanger)
	{
		// The duck (R-AUD-05): from where it is now toward C.AUD.DUCK_DB, held while a danger voice sounds.
		if (Now > DuckHoldUntil)
		{
			DuckFrom = DuckDbAt(Now);
			DuckStart = Now;
		}
		DuckHoldUntil = FMath::Max(DuckHoldUntil, Live.EndTime);
	}
	Voice.DuckDb = DuckDbAt(Now);

	FDTAudioCounts& Count = Counts[Batch.Sound];
	++Count.Played;
	Count.Merged += Batch.Count - 1;
	LastStartStep[Batch.Sound] = Batch.Step;
	LastStartTime[Batch.Sound] = Now;
	++StartsThisFrame;
	if (Row.Side == DTCat::ESoundSide::Player)
	{
		PlayerStarts.Add(Batch.Step);
	}
	Out.Add(Voice);
}

float FDTAudioScheduler::DuckDbAt(double T) const
{
	const float Floor = static_cast<float>(DTConst::AUD_DUCK_DB);
	const double Attack = FMath::Max(1.0, static_cast<double>(DTConst::AUD_DUCK_ATTACK_MS)) / 1000.0;
	const double Release = FMath::Max(1.0, static_cast<double>(DTConst::AUD_DUCK_RELEASE_MS)) / 1000.0;
	const auto Attacked = [&](double At)
	{
		return FMath::Max(Floor, DuckFrom + Floor * static_cast<float>(FMath::Max(0.0, At - DuckStart) / Attack));
	};
	if (T <= DuckHoldUntil)
	{
		return T < DuckStart ? DuckFrom : Attacked(T);
	}
	return FMath::Min(0.0f, Attacked(DuckHoldUntil) - Floor * static_cast<float>((T - DuckHoldUntil) / Release));
}

void FDTAudioScheduler::UpdateDuck(double Now)
{
	DuckDb = DuckDbAt(Now);
}
