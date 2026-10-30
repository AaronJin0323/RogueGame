#include "Keepers/DTKeeperSpec.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"

namespace DTKeeperSpecLocal
{
	static const FName WordOn(TEXT("on"));
	static const FName WordVolley(TEXT("Volley"));
	static const FName ParamEveryNVolley(TEXT("every_n_volley"));
	static const FName ParamPattern(TEXT("pattern"));
	static const TCHAR* const ClauseJoin = TEXT(" + ");
	static const TCHAR* const FlagPrefix = TEXT("Flag.");
	static const TCHAR* const StatusPrefix = TEXT("Status.");

	/** Card columns (Key, Amp1, Amp2, Trait) of an archetype row. */
	constexpr int32 NumCardColumns = 4;

	/** Parameter names that are hero or weapon stats, not flag behaviour (R-KEE-02 stat-only check). */
	static const TCHAR* const StatNames[] = { TEXT("damage"), TEXT("damage_mult"), TEXT("rate"), TEXT("fire_rate"),
		TEXT("cooldown"), TEXT("cooldown_mult"), TEXT("crit"), TEXT("crit_chance"), TEXT("crit_mult"), TEXT("armor"),
		TEXT("hp"), TEXT("max_hp"), TEXT("move_speed") };

	/** Words whose value must be a known Status.* tag. */
	static bool IsStatusWord(const FString& Name)
	{
		return Name == TEXT("status") || Name.EndsWith(TEXT("_status"));
	}

	static DTCat::EFlag FindFlag(const FString& Tag)
	{
		for (int32 Index = 0; Index < DTCat::NumFlags; ++Index)
		{
			if (Tag == DTCat::FlagIds[Index])
			{
				return static_cast<DTCat::EFlag>(Index);
			}
		}
		return DTCat::EFlag::None;
	}

	static bool Fail(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}

	static EDTFlagTier TierOf(int32 Column)
	{
		return Column == static_cast<int32>(DTCat::EEffectColumn::Key) ? EDTFlagTier::Key : EDTFlagTier::Trait;
	}

	static EDTBlessingRole RoleOf(int32 Column)
	{
		switch (Column)
		{
		case 0: return EDTBlessingRole::Key;
		case 3: return EDTBlessingRole::Trait;
		default: return EDTBlessingRole::Amp;
		}
	}

	static const TCHAR* PathName(EDTPath Path)
	{
		switch (Path)
		{
		case EDTPath::Storm: return TEXT("Storm");
		case EDTPath::Forge: return TEXT("Forge");
		case EDTPath::Tide: return TEXT("Tide");
		case EDTPath::Shade: return TEXT("Shade");
		case EDTPath::Radiance: return TEXT("Radiance");
		case EDTPath::Quicksilver: return TEXT("Quicksilver");
		default: return TEXT("None");
		}
	}

	struct FCatalogCache
	{
		TArray<FString> Errors;
		TArray<bool> Accepted; // NumArchetypes x NumCardColumns
		FCatalogCache()
		{
			Accepted.Init(true, DTCat::NumArchetypes * NumCardColumns);
			DTKeepers::ValidateCatalog(TConstArrayView<DTCat::FArchetypeRow>(DTCat::Archetypes, DTCat::NumArchetypes), Errors);
			for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
			{
				for (int32 Column = 0; Column < NumCardColumns; ++Column)
				{
					Accepted[Row * NumCardColumns + Column] = DTKeepers::ValidateBlessing(DTCat::Archetypes[Row].Effects[Column],
						RoleOf(Column), DTCat::Archetypes[Row].KeyFlag);
				}
			}
		}
	};

	static const FCatalogCache& Cache()
	{
		static const FCatalogCache Instance;
		return Instance;
	}
}

// ---- clause ------------------------------------------------------------------------------------------------------------

bool FDTSpecClause::HasNum(FName Name) const
{
	return Nums.ContainsByPredicate([Name](const FDTSpecNum& N) { return N.Name == Name; });
}

float FDTSpecClause::GetNum(FName Name, float Default) const
{
	const FDTSpecNum* Found = Nums.FindByPredicate([Name](const FDTSpecNum& N) { return N.Name == Name; });
	return Found ? Found->Value : Default;
}

FName FDTSpecClause::GetWord(FName Name) const
{
	const TPair<FName, FName>* Found = Words.FindByPredicate([Name](const TPair<FName, FName>& W) { return W.Key == Name; });
	return Found ? Found->Value : NAME_None;
}

void FDTSpecClause::RemoveWord(FName Name)
{
	Words.RemoveAll([Name](const TPair<FName, FName>& W) { return W.Key == Name; });
}

// ---- names -------------------------------------------------------------------------------------------------------------

FName DTKeepers::SourceTag(EDTPath Path)
{
	return FName(*FString::Printf(TEXT("Keeper.%s"), DTKeeperSpecLocal::PathName(Path)));
}

uint16 DTKeepers::KeeperSource(EDTPath Path)
{
	// Ids are stable for the process (FDTTelemetrySources); looked up once instead of per hit.
	static const TArray<uint16> Ids = []()
	{
		TArray<uint16> Out;
		for (int32 P = 0; P < static_cast<int32>(EDTPath::Count); ++P)
		{
			Out.Add(DTWeapons::KeeperSource(static_cast<EDTPath>(P)));
		}
		return Out;
	}();
	return Ids.IsValidIndex(static_cast<int32>(Path)) ? Ids[static_cast<int32>(Path)] : DTWeapons::KeeperSource(Path);
}

FColor DTKeepers::TrailToken(EDTPath Path)
{
	switch (Path)
	{
	case EDTPath::Storm: return DTPal::Keeper_Storm;
	case EDTPath::Forge: return DTPal::Keeper_Forge;
	case EDTPath::Tide: return DTPal::Keeper_Tide;
	case EDTPath::Shade: return DTPal::Keeper_Shade;
	case EDTPath::Radiance: return DTPal::Keeper_Radiance;
	case EDTPath::Quicksilver: return DTPal::Keeper_Quicksilver;
	default: return DTPal::Player_TrailNeutral;
	}
}

bool DTKeepers::ParseStatus(FName Word, EDTStatus& OutStatus)
{
	static const TCHAR* const Names[] = { TEXT("Status.Shock"), TEXT("Status.Burn"), TEXT("Status.Chill"), TEXT("Status.Freeze") };
	static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EDTStatus::Count), "one name per EDTStatus");
	const FString Text = Word.ToString();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		if (Text == Names[Index])
		{
			OutStatus = static_cast<EDTStatus>(Index);
			return true;
		}
	}
	return false;
}

bool DTKeepers::EnchantStatus(EDTPath Path, EDTStatus& OutStatus)
{
	// Parsed once per Path (the enchantment rolls on every player hit).
	static const TArray<int32> Cached = []()
	{
		TArray<int32> Out;
		for (int32 P = 0; P < static_cast<int32>(EDTPath::Count); ++P)
		{
			EDTStatus Status;
			Out.Add(EnchantStatusUncached(static_cast<EDTPath>(P), Status) ? static_cast<int32>(Status) : INDEX_NONE);
		}
		return Out;
	}();
	const int32 Index = static_cast<int32>(Path);
	if (!Cached.IsValidIndex(Index) || Cached[Index] == INDEX_NONE)
	{
		return false;
	}
	OutStatus = static_cast<EDTStatus>(Cached[Index]);
	return true;
}

bool DTKeepers::EnchantStatusUncached(EDTPath Path, EDTStatus& OutStatus)
{
	for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
	{
		if (DTCat::Archetypes[Row].Path != Path)
		{
			continue;
		}
		// "Status.Chill>Status.Freeze": the enchantment applies the first; the chain is the status's own behaviour.
		FString First = DTCat::Archetypes[Row].EnchantStatus;
		int32 Arrow = INDEX_NONE;
		if (First.FindChar(TEXT('>'), Arrow))
		{
			First.LeftInline(Arrow);
		}
		return ParseStatus(FName(*First.TrimStartAndEnd()), OutStatus);
	}
	return false;
}

int32 DTKeepers::FindArchetype(FName ArchetypeId)
{
	if (ArchetypeId.IsNone())
	{
		return INDEX_NONE;
	}
	for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
	{
		if (ArchetypeId == FName(DTCat::Archetypes[Row].Id))
		{
			return Row;
		}
	}
	return INDEX_NONE;
}

// ---- parsing -------------------------------------------------------------------------------------------------------------

bool DTKeepers::ParseSpec(const TCHAR* Text, FDTEffectSpec& Out, FString* OutError)
{
	using namespace DTKeeperSpecLocal;
	Out.Clauses.Reset();
	const FString Source = FString(Text ? Text : TEXT("")).TrimStartAndEnd();
	if (Source.IsEmpty())
	{
		return Fail(OutError, TEXT("empty effect spec"));
	}
	TArray<FString> Parts;
	Source.ParseIntoArray(Parts, ClauseJoin, true);
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		FString Tag = Part;
		FString Body;
		int32 Open = INDEX_NONE;
		if (Part.FindChar(TEXT('{'), Open))
		{
			if (!Part.EndsWith(TEXT("}")))
			{
				return Fail(OutError, FString::Printf(TEXT("'%s': unbalanced braces"), *Part));
			}
			Tag = Part.Left(Open).TrimStartAndEnd();
			Body = Part.Mid(Open + 1, Part.Len() - Open - 2);
		}
		if (!Tag.StartsWith(FlagPrefix))
		{
			return Fail(OutError, FString::Printf(TEXT("'%s' is not a Flag.* (stat-only effects are rejected, R-KEE-02)"), *Tag));
		}
		FDTSpecClause& Clause = Out.Clauses.AddDefaulted_GetRef();
		Clause.Flag = FindFlag(Tag);
		if (Clause.Flag == DTCat::EFlag::None)
		{
			return Fail(OutError, FString::Printf(TEXT("unknown flag '%s'"), *Tag));
		}
		TArray<FString> Pairs;
		Body.ParseIntoArray(Pairs, TEXT(";"), true);
		for (FString Pair : Pairs)
		{
			Pair.TrimStartAndEndInline();
			FString Name;
			FString Value;
			if (!Pair.Split(TEXT("="), &Name, &Value) || Name.TrimStartAndEnd().IsEmpty() || Value.TrimStartAndEnd().IsEmpty())
			{
				return Fail(OutError, FString::Printf(TEXT("'%s': parameter '%s' is not name=value"), *Tag, *Pair));
			}
			Name.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();
			const bool bSigned = Value.StartsWith(TEXT("+")) || Value.StartsWith(TEXT("-"));
			if (FCString::IsNumeric(*Value) || (bSigned && FCString::IsNumeric(*Value.Mid(1))))
			{
				Clause.Nums.Add(FDTSpecNum{ FName(*Name), FCString::Atof(*Value), bSigned });
			}
			else if (Value == TEXT("true") || Value == TEXT("false"))
			{
				Clause.Nums.Add(FDTSpecNum{ FName(*Name), Value == TEXT("true") ? 1.0f : 0.0f, false });
			}
			else
			{
				if (IsStatusWord(Name))
				{
					EDTStatus Status;
					if (!Value.StartsWith(StatusPrefix) || !ParseStatus(FName(*Value), Status))
					{
						return Fail(OutError, FString::Printf(TEXT("'%s': unknown status '%s'"), *Tag, *Value));
					}
				}
				Clause.Words.Emplace(FName(*Name), FName(*Value));
			}
		}
	}
	return true;
}

bool DTKeepers::ValidateBlessing(const TCHAR* Text, EDTBlessingRole Role, DTCat::EFlag KeyFlag, FString* OutError)
{
	using namespace DTKeeperSpecLocal;
	FDTEffectSpec Spec;
	if (!ParseSpec(Text, Spec, OutError))
	{
		return false;
	}
	bool bAnyBehaviour = false;
	for (const FDTSpecClause& Clause : Spec.Clauses)
	{
		if (Clause.Nums.Num() == 0 && Clause.Words.Num() == 0)
		{
			return Fail(OutError, FString::Printf(TEXT("%s names no parameter"), DTCat::FlagIds[static_cast<int32>(Clause.Flag)]));
		}
		bAnyBehaviour |= Clause.Words.Num() > 0; // a word (pattern, trigger, status, ...) is behaviour
		for (const FDTSpecNum& Num : Clause.Nums)
		{
			const FString Name = Num.Name.ToString();
			bool bStat = false;
			for (const TCHAR* Stat : StatNames)
			{
				bStat |= Name == Stat;
			}
			bAnyBehaviour |= !bStat;
		}
	}
	if (!bAnyBehaviour)
	{
		return Fail(OutError, TEXT("the whole effect is a stat delta (R-KEE-02)"));
	}
	if (Role == EDTBlessingRole::Key)
	{
		// Exactly one behaviour flag, the archetype's key flag; StatusApply riders carry a status, not a behaviour.
		TSet<DTCat::EFlag> Behaviours;
		for (const FDTSpecClause& Clause : Spec.Clauses)
		{
			if (Clause.Flag != DTCat::EFlag::StatusApply || KeyFlag == DTCat::EFlag::StatusApply)
			{
				Behaviours.Add(Clause.Flag);
			}
		}
		if (Behaviours.Num() != 1 || !Behaviours.Contains(KeyFlag))
		{
			return Fail(OutError, FString::Printf(TEXT("a Key must carry exactly its key flag %s"),
				KeyFlag == DTCat::EFlag::None ? TEXT("None") : DTCat::FlagIds[static_cast<int32>(KeyFlag)]));
		}
	}
	return true;
}

bool DTKeepers::ValidateCatalog(TConstArrayView<DTCat::FArchetypeRow> Rows, TArray<FString>& OutErrors)
{
	using namespace DTKeeperSpecLocal;
	const int32 Before = OutErrors.Num();
	TMap<EDTPath, int32> PerPath;
	TMap<DTCat::EFlag, int32> KeyFlagRow;
	static const TCHAR* const ColumnNames[] = { TEXT("Key"), TEXT("Amp1"), TEXT("Amp2"), TEXT("Trait") };
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		const DTCat::FArchetypeRow& R = Rows[Row];
		const FString Id = R.Id ? R.Id : TEXT("");
		PerPath.FindOrAdd(R.Path)++;
		const FString Prefix = FString::Printf(TEXT("Archetype.%s."), PathName(R.Path));
		if (!Id.StartsWith(Prefix))
		{
			OutErrors.Add(FString::Printf(TEXT("row %d: id '%s' is not %s<Name>"), Row, *Id, *Prefix));
		}
		const FString Name = Id.Mid(Prefix.Len());
		if (R.KeyFlag == DTCat::EFlag::None)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: no key flag"), *Id));
		}
		else if (const int32* Other = KeyFlagRow.Find(R.KeyFlag))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: key flag shared with row %d (R-KEE-02)"), *Id, *Other));
		}
		else
		{
			KeyFlagRow.Add(R.KeyFlag, Row);
		}
		for (int32 Column = 0; Column < NumCardColumns; ++Column)
		{
			const FString Expected = FString::Printf(TEXT("Blessing.%s.%s.%s"), PathName(R.Path), *Name, ColumnNames[Column]);
			if (Expected != R.BlessingIds[Column])
			{
				OutErrors.Add(FString::Printf(TEXT("%s: blessing id '%s', expected '%s'"), *Id, R.BlessingIds[Column], *Expected));
			}
			FString Error;
			if (!ValidateBlessing(R.Effects[Column], RoleOf(Column), R.KeyFlag, &Error))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: %s"), *Expected, *Error));
			}
		}
		for (int32 Column = NumCardColumns; Column < static_cast<int32>(DTCat::EEffectColumn::Count); ++Column)
		{
			FDTEffectSpec Spec;
			FString Error;
			if (!ParseSpec(R.Effects[Column], Spec, &Error))
			{
				OutErrors.Add(FString::Printf(TEXT("%s Tier%d: %s"), *Id, Column - NumCardColumns + 1, *Error));
			}
		}
		const TPair<const TCHAR*, const TCHAR*> Extra[] = { { TEXT("IntercessionVariant"), R.IntercessionVariant }, { TEXT("SecondKeeperLayer"), R.SecondKeeperLayer } };
		for (const TPair<const TCHAR*, const TCHAR*>& Column : Extra)
		{
			FDTEffectSpec Spec;
			FString Error;
			if (!ParseSpec(Column.Value, Spec, &Error))
			{
				OutErrors.Add(FString::Printf(TEXT("%s %s: %s"), *Id, Column.Key, *Error));
			}
		}
		FString Status = R.EnchantStatus ? R.EnchantStatus : TEXT("");
		TArray<FString> Statuses;
		Status.ParseIntoArray(Statuses, TEXT(">"), true);
		for (const FString& One : Statuses)
		{
			EDTStatus Parsed;
			if (!ParseStatus(FName(*One.TrimStartAndEnd()), Parsed))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: unknown EnchantStatus '%s'"), *Id, *One));
			}
		}
		if (Statuses.Num() == 0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: no EnchantStatus"), *Id));
		}
	}
	for (const TPair<EDTPath, int32>& Path : PerPath)
	{
		if (Path.Value != DTConst::KEEPER_ARCHETYPES_PER_KEEPER)
		{
			OutErrors.Add(FString::Printf(TEXT("Keeper.%s: %d archetypes, expected C.KEEPER.ARCHETYPES_PER_KEEPER"), PathName(Path.Key), Path.Value));
		}
	}
	// R-KEE-16 / R-KEE-20: IncompatFlags pairs are symmetric between the archetypes that hold those key flags.
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		for (int32 Flag = 0; Flag < DTCat::NumFlags; ++Flag)
		{
			if ((Rows[Row].IncompatFlags & DTCat::FlagBit(static_cast<DTCat::EFlag>(Flag))) == 0)
			{
				continue;
			}
			const int32* Other = KeyFlagRow.Find(static_cast<DTCat::EFlag>(Flag));
			if (!Other || (Rows[*Other].IncompatFlags & DTCat::FlagBit(Rows[Row].KeyFlag)) == 0)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: IncompatFlags %s is not symmetric"), Rows[Row].Id, DTCat::FlagIds[Flag]));
			}
		}
	}
	return OutErrors.Num() == Before;
}

bool DTKeepers::IsBlessingAccepted(int32 Archetype, int32 Column)
{
	using namespace DTKeeperSpecLocal;
	const FCatalogCache& C = Cache();
	const int32 Index = Archetype * NumCardColumns + Column;
	return Archetype >= 0 && Column >= 0 && Column < NumCardColumns && C.Accepted.IsValidIndex(Index) && C.Accepted[Index];
}

const TArray<FString>& DTKeepers::CatalogErrors()
{
	return DTKeeperSpecLocal::Cache().Errors;
}

// ---- flags ---------------------------------------------------------------------------------------------------------------

void DTKeepers::FoldClause(FDTWeaponFlags& Flags, const FDTSpecClause& Clause, uint16 SourceId, EDTFlagTier Tier)
{
	using namespace DTKeeperSpecLocal;
	FName On = Clause.GetWord(WordOn);
	if (On.IsNone() && Clause.HasNum(ParamEveryNVolley) && !Clause.GetWord(ParamPattern).IsNone())
	{
		On = WordVolley; // its own shape every Nth volley (e.g. Tempest Tier3's ring), not a change of the per-hit one
	}
	FDTFlagEntry* Entry = Flags.Entries.FindByPredicate([&Clause, On](const FDTFlagEntry& E)
	{
		return E.Flag == Clause.Flag && E.Params.GetWord(WordOn) == On;
	});
	if (!Entry)
	{
		Entry = &Flags.Add(Clause.Flag, SourceId, Tier);
		if (!On.IsNone())
		{
			Entry->Params.SetWord(WordOn, On);
		}
	}
	Entry->Tier = FMath::Max(Entry->Tier, Tier);
	for (const FDTSpecNum& Num : Clause.Nums)
	{
		const float Value = Num.bRelative ? Entry->Params.Get(Num.Name) + Num.Value : Num.Value;
		Entry->Params.Replace(Num.Name, Value);
	}
	for (const TPair<FName, FName>& Word : Clause.Words)
	{
		Entry->Params.SetWord(Word.Key, Word.Value);
	}
}

int32 DTKeepers::OwnedOfPath(const FDTKeeperRunState& Keepers, EDTPath Path)
{
	int32 Count = 0;
	for (const FName& Id : Keepers.Owned)
	{
		for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
		{
			const DTCat::FArchetypeRow& R = DTCat::Archetypes[Row];
			for (int32 Column = 0; R.Path == Path && Column < DTKeeperSpecLocal::NumCardColumns; ++Column)
			{
				Count += Id == FName(R.BlessingIds[Column]) ? 1 : 0;
			}
		}
	}
	return Count;
}

FDTWeaponFlags DTKeepers::BuildKeeperFlags(const FDTKeeperRunState& Keepers, EDTPath Path, TConstArrayView<FDTSpecClause> Overlay)
{
	using namespace DTKeeperSpecLocal;
	FDTWeaponFlags Flags;
	if (Path == EDTPath::None || Path >= EDTPath::Count)
	{
		return Flags;
	}
	const uint16 Source = KeeperSource(Path);
	// 1. Owned blessings in pick order (R-KEE-03: each exactly once; rejected ones never apply, R-KEE-02).
	for (const FName& Id : Keepers.Owned)
	{
		for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
		{
			const DTCat::FArchetypeRow& R = DTCat::Archetypes[Row];
			if (R.Path != Path)
			{
				continue;
			}
			for (int32 Column = 0; Column < NumCardColumns; ++Column)
			{
				if (Id != FName(R.BlessingIds[Column]) || !IsBlessingAccepted(Row, Column))
				{
					continue;
				}
				FDTEffectSpec Spec;
				if (ParseSpec(R.Effects[Column], Spec))
				{
					for (const FDTSpecClause& Clause : Spec.Clauses)
					{
						FoldClause(Flags, Clause, Source, TierOf(Column));
					}
				}
			}
		}
	}
	// 2. Unlocked tiers apply the current leading archetype's lines (R-KEE-07): they follow a change of leader.
	const int32 P = static_cast<int32>(Path);
	const int32 Leading = FindArchetype(Keepers.Leading[P]);
	if (Leading != INDEX_NONE)
	{
		const int32 Tiers = FMath::Clamp(Keepers.Tier[P], 0, 3);
		for (int32 Tier = 0; Tier < Tiers; ++Tier)
		{
			FDTEffectSpec Spec;
			if (ParseSpec(DTCat::Archetypes[Leading].Effects[static_cast<int32>(DTCat::EEffectColumn::Tier1) + Tier], Spec))
			{
				for (const FDTSpecClause& Clause : Spec.Clauses)
				{
					FoldClause(Flags, Clause, Source, EDTFlagTier::Tier);
				}
			}
		}
	}
	// 3. An active Intercession reshapes the Keeper's flags for its window (R-KEE-14).
	for (const FDTSpecClause& Clause : Overlay)
	{
		FoldClause(Flags, Clause, Source, EDTFlagTier::Key);
	}
	return Flags;
}

float DTKeepers::PathParam(EDTPath Path, DTCat::EFlag Flag, FName Param, float Default)
{
	for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
	{
		const DTCat::FArchetypeRow& R = DTCat::Archetypes[Row];
		if (R.Path != Path)
		{
			continue;
		}
		for (int32 Column = 0; Column < DTKeeperSpecLocal::NumCardColumns; ++Column)
		{
			FDTEffectSpec Spec;
			if (!ParseSpec(R.Effects[Column], Spec))
			{
				continue;
			}
			for (const FDTSpecClause& Clause : Spec.Clauses)
			{
				const FDTSpecNum* Num = Clause.Flag == Flag
					? Clause.Nums.FindByPredicate([Param](const FDTSpecNum& N) { return N.Name == Param && !N.bRelative; }) : nullptr;
				if (Num)
				{
					return Num->Value;
				}
			}
		}
	}
	return Default;
}

// ---- Intercession ----------------------------------------------------------------------------------------------------------

bool DTKeepers::IsShapeWord(FName Name)
{
	static const FName Shape[] = { FName(TEXT("pattern")), FName(TEXT("trigger")), FName(TEXT("target")), FName(TEXT("scope")),
		FName(TEXT("on")), FName(TEXT("per")), FName(TEXT("around")), FName(TEXT("source")), FName(TEXT("then")) };
	for (const FName& S : Shape)
	{
		if (S == Name)
		{
			return true;
		}
	}
	return false;
}

FDTIntercessionPlan DTKeepers::ComposeIntercession(const FDTKeeperRunState& Keepers)
{
	FDTIntercessionPlan Plan;
	Plan.Base = Keepers.Primary;
	if (Plan.Base == EDTPath::None || Plan.Base >= EDTPath::Count)
	{
		return Plan;
	}
	Plan.Variant = FindArchetype(Keepers.Leading[static_cast<int32>(Plan.Base)]);
	if (Plan.Variant != INDEX_NONE)
	{
		ParseSpec(DTCat::Archetypes[Plan.Variant].IntercessionVariant, Plan.VariantSpec);
	}
	const bool bCommitted = Keepers.bCommitDone && Keepers.Second != EDTPath::None && Keepers.Second < EDTPath::Count;
	if (bCommitted)
	{
		Plan.LayerPath = Keepers.Second;
		Plan.Layer = FindArchetype(Keepers.Leading[static_cast<int32>(Keepers.Second)]);
		if (Plan.Layer != INDEX_NONE)
		{
			ParseSpec(DTCat::Archetypes[Plan.Layer].SecondKeeperLayer, Plan.LayerSpec);
		}
	}
	// A layer that names a flag the variant shapes keeps its numbers and status, never its shape (R-KEE-14).
	for (FDTSpecClause& Clause : Plan.LayerSpec.Clauses)
	{
		if (!Plan.VariantSpec.HasFlag(Clause.Flag))
		{
			continue;
		}
		const int32 Before = Clause.Words.Num();
		Clause.Words.RemoveAll([](const TPair<FName, FName>& W) { return IsShapeWord(W.Key); });
		Plan.ConflictsResolved += Clause.Words.Num() != Before ? 1 : 0;
	}
	return Plan;
}
