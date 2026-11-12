#include "Audio/Observe/DTObserveFire.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTCatalogGen.h"
#include "Engine/World.h"

namespace DTObserveFireLocal
{
	constexpr int32 LocalOwner = 0; // the solo player, as the Danger family reads
	static const TCHAR* const WeaponPrefix = TEXT("Weapon.");
	static const TCHAR* const FirePrefix = TEXT("Snd.Fire.");

	/** Per row of DTCat::Weapons, its fire sound: matched by name once, the first time it is asked. */
	static const TArray<int32>& SoundsOfWeapons()
	{
		static const TArray<int32> Table = []()
		{
			TArray<int32> Out;
			for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
			{
				Out.Add(DTFireEvents::SoundOfId(DTCat::Weapons[Row].Id));
			}
			return Out;
		}();
		return Table;
	}
}

namespace DTFireEvents
{
	int32 SoundOfId(const TCHAR* WeaponId)
	{
		using namespace DTObserveFireLocal;
		const FString Id(WeaponId ? WeaponId : TEXT(""));
		if (!Id.StartsWith(WeaponPrefix, ESearchCase::CaseSensitive) || Id.Len() == FCString::Strlen(WeaponPrefix))
		{
			return INDEX_NONE;
		}
		const FString Fire = FString(FirePrefix) + Id.RightChop(FCString::Strlen(WeaponPrefix));
		for (int32 Sound = 0; Sound < DTCat::SoundCount; ++Sound)
		{
			if (Fire.Equals(DTCat::Sounds[Sound].Id, ESearchCase::CaseSensitive))
			{
				return Sound;
			}
		}
		return INDEX_NONE;
	}

	int32 SoundOf(int32 WeaponRow)
	{
		const TArray<int32>& Table = DTObserveFireLocal::SoundsOfWeapons();
		return Table.IsValidIndex(WeaponRow) ? Table[WeaponRow] : INDEX_NONE;
	}

	void Diff(const FDTFireView& Was, const FDTFireView& Now, TFunctionRef<void(int32 Slot, int32 Sound, int32 Count)> Raise)
	{
		for (int32 Slot = 0; Slot < DTConst::WEAPON_SLOTS; ++Slot)
		{
			// A counter that went down, or a slot that held another weapon, is a new baseline: nothing of it is this step's news. A slot
			// that held none last step (a weapon just given: its counter was zero there) sounds its first firing.
			const bool bOther = Was.Weapon[Slot] != INDEX_NONE && Was.Weapon[Slot] != Now.Weapon[Slot];
			if (Now.Weapon[Slot] == INDEX_NONE || bOther || Now.Spawned[Slot] <= Was.Spawned[Slot])
			{
				continue;
			}
			const int32 Sound = SoundOf(Now.Weapon[Slot]);
			if (Sound != INDEX_NONE)
			{
				Raise(Slot, Sound, static_cast<int32>(FMath::Min<int64>(Now.Spawned[Slot] - Was.Spawned[Slot], MAX_int32)));
			}
		}
	}
}

int64 FDTFireDensity::MinGapSteps()
{
	const int64 Voices = FMath::Max(1, DTConst::AUD_FIRE_DENSE_VOICES_PER_S);
	return (static_cast<int64>(DTConst::SIM_FIXED_HZ) + Voices - 1) / Voices;
}

bool FDTFireDensity::Admit(int64 Step)
{
	const int64 Second = DTConst::SIM_FIXED_HZ;
	Recent.RemoveAll([Step, Second](int64 Fired) { return Fired <= Step - Second; });
	Recent.Add(Step);
	// The rate over the last second, from its first firing to this one: (firings - 1) over their span. Dense when it is above
	// C.AUD.FIRE_DENSE_SHOTS_PER_S, read in whole steps: (n - 1) / (span / FIXED_HZ) > SHOTS.
	bool bDense = false;
	if (Recent.Num() >= 2)
	{
		const int64 Span = Step - Recent[0];
		bDense = Span > 0 && static_cast<int64>(Recent.Num() - 1) * Second > static_cast<int64>(DTConst::AUD_FIRE_DENSE_SHOTS_PER_S) * Span;
	}
	if (!bDense || !bSounded || Step - LastSounded >= MinGapSteps())
	{
		LastSounded = Step;
		bSounded = true;
		return true;
	}
	return false;
}

void FDTFireDensity::Reset()
{
	Recent.Reset();
	LastSounded = TNumericLimits<int64>::Lowest();
	bSounded = false;
}

void FDTObserveFire::Reset()
{
	Last = FDTFireView();
	bPrimed = false;
	for (FDTFireDensity& Slot : Density)
	{
		Slot.Reset();
	}
}

void FDTObserveFire::OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out)
{
	FDTFireView Now;
	Now.Step = Step;
	if (const UDTWeaponSubsystem* Weapons = World.GetSubsystem<UDTWeaponSubsystem>())
	{
		// The weapon sim's public const readers only: the counter that only grows, and the weapon each slot's last composed spec holds.
		const FDTWeaponSim& Sim = Weapons->GetSim();
		for (int32 Slot = 0; Slot < DTConst::WEAPON_SLOTS; ++Slot)
		{
			Now.Spawned[Slot] = Sim.GetCounters(DTObserveFireLocal::LocalOwner, Slot).Spawned;
			Now.Weapon[Slot] = Sim.GetLastSpec(DTObserveFireLocal::LocalOwner, Slot).Weapon;
		}
	}
	Observe(Now, Out);
}

void FDTObserveFire::Observe(const FDTFireView& Now, IDTAudioRequests& Out)
{
	if (!bPrimed)
	{
		// The first step of a run is the baseline: rounds already counted are not this step's news.
		Last = Now;
		bPrimed = true;
		return;
	}
	DTFireEvents::Diff(Last, Now, [&](int32 Slot, int32 Sound, int32 Count)
	{
		if (Density[Slot].Admit(Now.Step))
		{
			Out.Run(Sound, Count, Now.Step);
		}
		else
		{
			Out.Merged(Sound, Count); // withheld by R-AUD-07's density: asked and merged, so the counts still add up (R-AUD-20)
		}
	});
	Last = Now;
}
