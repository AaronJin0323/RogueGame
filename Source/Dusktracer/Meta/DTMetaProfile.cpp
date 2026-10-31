#include "Meta/DTMetaProfile.h"

namespace DTMetaProfileLocal
{
	constexpr double SecondsPerMinute = 60.0;
	constexpr int32 U32Bytes = 4;
	constexpr int32 I64Bytes = 8;

	static const FName StatSteps(TEXT("Steps"));
	static const FName StatKills(TEXT("Kills"));
	static const FName StatElites(TEXT("Elites"));
	static const FName StatBossKills(TEXT("BossKills"));
	static const FName StatCurrency(TEXT("CurrencyEarned"));

	static bool KeyLess(const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	}

	static void WriteBytes(TArray<uint8>& Out, int64 Value, int32 Bytes)
	{
		for (int32 Byte = 0; Byte < Bytes; ++Byte)
		{
			Out.Add(static_cast<uint8>(static_cast<uint64>(Value) >> (8 * Byte)));
		}
	}

	struct FWriter
	{
		TArray<uint8>& Out;

		void I32(int32 Value) { WriteBytes(Out, Value, U32Bytes); }
		void I64(int64 Value) { WriteBytes(Out, Value, I64Bytes); }
		void Name(FName Value)
		{
			const FString Text = Value.IsNone() ? FString() : Value.ToString();
			const FTCHARToUTF8 Utf8(*Text);
			I32(Utf8.Length());
			Out.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		}
		void Names(const TSet<FName>& Set)
		{
			TArray<FName> Keys = Set.Array();
			Keys.Sort(&KeyLess);
			I32(Keys.Num());
			for (const FName& Key : Keys)
			{
				Name(Key);
			}
		}
		template <typename ValueType>
		void Map(const TMap<FName, ValueType>& Values)
		{
			TArray<FName> Keys;
			Values.GetKeys(Keys);
			Keys.Sort(&KeyLess);
			I32(Keys.Num());
			for (const FName& Key : Keys)
			{
				Name(Key);
				Number(Values[Key]);
			}
		}
		void Number(int32 Value) { I32(Value); }
		void Number(int64 Value) { I64(Value); }
	};

	struct FReader
	{
		const TArray<uint8>& In;
		int32 Offset = 0;
		bool bOk = true;

		int32 Left() const { return In.Num() - Offset; }
		bool Need(int32 Bytes)
		{
			bOk = bOk && Bytes >= 0 && Bytes <= Left();
			return bOk;
		}
		uint64 Raw(int32 Bytes)
		{
			uint64 Value = 0;
			if (Need(Bytes))
			{
				for (int32 Byte = 0; Byte < Bytes; ++Byte)
				{
					Value |= static_cast<uint64>(In[Offset + Byte]) << (8 * Byte);
				}
				Offset += Bytes;
			}
			return Value;
		}
		int32 I32() { return static_cast<int32>(static_cast<uint32>(Raw(U32Bytes))); }
		int64 I64() { return static_cast<int64>(Raw(I64Bytes)); }
		void Number(int32& Value) { Value = I32(); }
		void Number(int64& Value) { Value = I64(); }
		/** May be empty only where a field allows it (LastPatron). */
		FName Name(bool bMayBeEmpty)
		{
			const int32 Size = I32();
			bOk = bOk && Size < NAME_SIZE && (bMayBeEmpty || Size > 0);
			if (!Need(Size) || Size == 0)
			{
				return NAME_None;
			}
			const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(In.GetData() + Offset), Size);
			Offset += Size;
			return FName(*FString::ConstructFromPtrSize(Text.Get(), Text.Length()));
		}
		/** A count that cannot fit in what is left fails here, before anything is allocated. */
		int32 Count()
		{
			const int32 Num = I32();
			bOk = bOk && Num >= 0 && Num <= Left() / U32Bytes;
			return bOk ? Num : 0;
		}
		void Names(TSet<FName>& Out)
		{
			const int32 Num = Count();
			for (int32 Index = 0; Index < Num && bOk; ++Index)
			{
				Out.Add(Name(false));
			}
			bOk = bOk && Out.Num() == Num;
		}
		template <typename ValueType>
		void Map(TMap<FName, ValueType>& Out)
		{
			const int32 Num = Count();
			for (int32 Index = 0; Index < Num && bOk; ++Index)
			{
				const FName Key = Name(false);
				ValueType Value = 0;
				Number(Value);
				Out.Add(Key, Value);
			}
			bOk = bOk && Out.Num() == Num;
		}
	};
}

TArray<uint8> DTMeta::Serialize(const FDTMetaProfile& P)
{
	TArray<uint8> Out;
	DTMetaProfileLocal::FWriter W{ Out };
	W.I64(P.Currency);
	W.I32(P.Runs);
	W.I32(P.Wins);
	W.I32(P.DeathsSinceWin);
	W.Name(P.LastPatron);
	W.Map(P.TreeRanks);
	W.Names(P.Unlocked);
	W.Names(P.Discoveries);
	W.Map(P.LifetimeStats);
	W.I32(P.Act);
	W.Names(P.StoryFlags);
	W.I32(P.RecollectionsRead);
	W.Map(P.ArenaWins);
	W.Map(P.ArenaHeat);
	W.I32(static_cast<int32>(P.FateTide));
	W.I32(static_cast<int32>(P.FateShade));
	W.Names(P.FateSeen);
	return Out;
}

bool DTMeta::Deserialize(const TArray<uint8>& Payload, FDTMetaProfile& Out)
{
	FDTMetaProfile P;
	DTMetaProfileLocal::FReader R{ Payload };
	P.Currency = R.I64();
	P.Runs = R.I32();
	P.Wins = R.I32();
	P.DeathsSinceWin = R.I32();
	P.LastPatron = R.Name(true);
	R.Map(P.TreeRanks);
	R.Names(P.Unlocked);
	R.Names(P.Discoveries);
	R.Map(P.LifetimeStats);
	P.Act = R.I32();
	R.Names(P.StoryFlags);
	P.RecollectionsRead = R.I32();
	R.Map(P.ArenaWins);
	R.Map(P.ArenaHeat);
	const int32 Tide = R.I32();
	const int32 Shade = R.I32();
	R.Names(P.FateSeen);
	if (!R.bOk || R.Left() != 0
		|| Tide < 0 || Tide > static_cast<int32>(EDTFateTide::Returned)
		|| Shade < 0 || Shade > static_cast<int32>(EDTFateShade::OffLedger))
	{
		return false;
	}
	P.FateTide = static_cast<EDTFateTide>(Tide);
	P.FateShade = static_cast<EDTFateShade>(Shade);
	Out = MoveTemp(P);
	return true;
}

bool DTMeta::IsValid(const FDTMetaProfile& P)
{
	const auto NoNegative = [](const auto& Values)
	{
		for (const auto& Pair : Values)
		{
			if (Pair.Value < 0)
			{
				return false;
			}
		}
		return true;
	};
	return P.Currency >= 0 && P.Runs >= 0 && P.Wins >= 0 && P.Wins <= P.Runs && P.DeathsSinceWin >= 0 && P.DeathsSinceWin <= P.Runs
		&& P.Act >= DTConst::META_ACT_START && P.Act <= DTConst::META_ACT_MAX && P.RecollectionsRead >= 0
		&& NoNegative(P.TreeRanks) && NoNegative(P.LifetimeStats) && NoNegative(P.ArenaWins) && NoNegative(P.ArenaHeat);
}

bool DTMeta::Same(const FDTMetaProfile& A, const FDTMetaProfile& B)
{
	return Serialize(A) == Serialize(B);
}

bool DTMeta::IsEmpty(const FDTMetaProfile& Profile)
{
	return Same(Profile, FDTMetaProfile());
}

int64 DTMeta::ComputeAward(const FDTRunTally& Tally)
{
	const double Minutes = static_cast<double>(Tally.Steps) / DTConst::SIM_FIXED_HZ / DTMetaProfileLocal::SecondsPerMinute;
	const double Base = DTConst::META_CURRENCY_PER_MINUTE * Minutes
		+ static_cast<double>(DTConst::META_CURRENCY_PER_KILL) * Tally.Kills
		+ static_cast<double>(DTConst::META_CURRENCY_PER_ELITE) * Tally.Elites
		+ (Tally.bBossKilled ? static_cast<double>(DTConst::META_CURRENCY_BOSS_KILL) : 0.0);
	const double Heat = 1.0 + static_cast<double>(DTConst::META_CURRENCY_HEAT_BONUS) * Tally.Heat;
	return FMath::FloorToInt64(Base * Heat);
}

int64 DTMeta::ApplyRun(FDTMetaProfile& Profile, const FDTRunTally& Tally, EDTRunEnd End)
{
	using namespace DTMetaProfileLocal;
	const int64 Award = ComputeAward(Tally);
	Profile.Currency += Award;
	++Profile.Runs;
	if (End == EDTRunEnd::Victory)
	{
		++Profile.Wins;
		Profile.DeathsSinceWin = 0;
	}
	else if (End == EDTRunEnd::Death)
	{
		++Profile.DeathsSinceWin;
	}
	if (!Tally.Patron.IsNone())
	{
		Profile.LastPatron = Tally.Patron;
	}
	Profile.LifetimeStats.FindOrAdd(StatSteps) += Tally.Steps;
	Profile.LifetimeStats.FindOrAdd(StatKills) += Tally.Kills;
	Profile.LifetimeStats.FindOrAdd(StatElites) += Tally.Elites;
	Profile.LifetimeStats.FindOrAdd(StatBossKills) += Tally.bBossKilled ? 1 : 0;
	Profile.LifetimeStats.FindOrAdd(StatCurrency) += Award;
	for (const TPair<FName, int64>& Counter : Tally.Counters)
	{
		Profile.LifetimeStats.FindOrAdd(Counter.Key) += Counter.Value;
	}
	return Award;
}
