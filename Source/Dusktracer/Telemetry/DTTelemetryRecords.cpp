// S1 record writers of FDTTelemetryCore (02_Systems/telemetry.md §Data). Field names and order follow each owning spec's
// §Telemetry: offers.md, keepers.md, combat_core.md, progression.md, weapons.md, modifiers.md, director.md, boss.md.
#include "Telemetry/DTTelemetryRecords.h"
#include "Enemies/DTElite.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetryJson.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTTelNames
{
	const TCHAR* InputDevice(EDTTelInputDevice Device)
	{
		switch (Device)
		{
		case EDTTelInputDevice::Kbm: return TEXT("kbm");
		case EDTTelInputDevice::Pad: return TEXT("pad");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* OfferTool(EDTTelOfferTool Tool)
	{
		switch (Tool)
		{
		case EDTTelOfferTool::Reroll: return TEXT("reroll");
		case EDTTelOfferTool::Banish: return TEXT("banish");
		default: return TEXT("lock");
		}
	}

	const TCHAR* KeeperRole(EDTTelKeeperRole Role)
	{
		return Role == EDTTelKeeperRole::Patron ? TEXT("Patron") : TEXT("Second");
	}

	const TCHAR* BlessingRole(EDTTelBlessingRole Role)
	{
		switch (Role)
		{
		case EDTTelBlessingRole::Key: return TEXT("Key");
		case EDTTelBlessingRole::Amp: return TEXT("Amp");
		default: return TEXT("Trait");
		}
	}

	const TCHAR* CardKind(EDTCardKind Kind)
	{
		switch (Kind)
		{
		case EDTCardKind::Weapon: return TEXT("Weapon");
		case EDTCardKind::WeaponUpgrade: return TEXT("WeaponUpgrade");
		case EDTCardKind::Mod: return TEXT("Mod");
		case EDTCardKind::Blessing: return TEXT("Blessing");
		case EDTCardKind::Keeper: return TEXT("Keeper");
		case EDTCardKind::Reforging: return TEXT("Reforging");
		default: return TEXT("Reserve");
		}
	}

	const TCHAR* HandKind(EDTHandKind Kind)
	{
		switch (Kind)
		{
		case EDTHandKind::Level: return TEXT("Level");
		case EDTHandKind::Chest: return TEXT("Chest");
		case EDTHandKind::Keeper: return TEXT("Keeper");
		case EDTHandKind::Reforging: return TEXT("Reforging");
		default: return TEXT("Patron");
		}
	}

	const TCHAR* Rarity(EDTRarity Rarity)
	{
		switch (Rarity)
		{
		case EDTRarity::Common: return TEXT("Common");
		case EDTRarity::Rare: return TEXT("Rare");
		default: return TEXT("Epic");
		}
	}

	const TCHAR* EnemyRole(EDTEnemyRole Role)
	{
		switch (Role)
		{
		case EDTEnemyRole::Swarm: return TEXT("Swarm");
		case EDTEnemyRole::Tank: return TEXT("Tank");
		case EDTEnemyRole::Rusher: return TEXT("Rusher");
		case EDTEnemyRole::Turret: return TEXT("Turret");
		default: return TEXT("Boss");
		}
	}

	const TCHAR* Status(EDTStatus Status)
	{
		switch (Status)
		{
		case EDTStatus::Shock: return TEXT("Shock");
		case EDTStatus::Burn: return TEXT("Burn");
		case EDTStatus::Chill: return TEXT("Chill");
		case EDTStatus::Mark: return TEXT("Mark");
		case EDTStatus::Blind: return TEXT("Blind");
		default: return TEXT("Freeze");
		}
	}

	const TCHAR* HeroHitKind(EDTHeroHitKind Kind)
	{
		switch (Kind)
		{
		case EDTHeroHitKind::Contact: return TEXT("Contact");
		case EDTHeroHitKind::Bullet: return TEXT("Bullet");
		case EDTHeroHitKind::Charge: return TEXT("Charge");
		case EDTHeroHitKind::Sweep: return TEXT("Sweep");
		case EDTHeroHitKind::DashCharge: return TEXT("DashCharge");
		case EDTHeroHitKind::Blast: return TEXT("Blast");
		case EDTHeroHitKind::Drain: return TEXT("Drain");
		default: return TEXT("HazardRing");
		}
	}
}

namespace DTTelemetryRecordsLocal
{
	static constexpr int32 HandKinds = static_cast<int32>(EDTHandKind::Patron) + 1;

	/** {"<owner>": <json>, ...} in owner order. */
	template <typename TValue>
	static FString ByOwner(const TArray<TValue>& Values, TFunctionRef<FString(const TValue&)> ToJson)
	{
		FDTTelJson Out;
		for (int32 Owner = 0; Owner < Values.Num(); ++Owner)
		{
			Out.Raw(FString::FromInt(Owner), ToJson(Values[Owner]));
		}
		return Out.Close();
	}
}

FDTTelemetryCore::FPlayerInterval* FDTTelemetryCore::RecordOwner(int32 Owner)
{
	if (!bActive)
	{
		return nullptr;
	}
	if (!Players.IsValidIndex(Owner))
	{
		++UnownedEvents; // R-TEL-02: lands in no row and no record
		return nullptr;
	}
	return &Players[Owner];
}

// ---- offers.md ----------------------------------------------------------------------------------------------------------

void FDTTelemetryCore::RecordOffer(int32 Owner, const FDTOfferHand& Hand, const FDTPlayerOfferState& Tools)
{
	FPlayerInterval* P = RecordOwner(Owner);
	if (!P)
	{
		return;
	}
	FString Cards = TEXT("[");
	int32 Reserve = 0;
	for (int32 Slot = 0; Slot < Hand.Cards.Num(); ++Slot)
	{
		const FDTOfferCard& Card = Hand.Cards[Slot];
		const bool bReserve = Card.Kind == EDTCardKind::Reserve;
		Reserve += bReserve ? 1 : 0;
		Cards += (Slot ? TEXT(",") : TEXT("")) + FDTTelJson().Int(TEXT("slot"), Slot).Str(TEXT("kind"), DTTelNames::CardKind(Card.Kind))
			.Name(TEXT("id"), Card.ContentId).Name(TEXT("target"), Card.TargetWeapon).Str(TEXT("rarity"), DTTelNames::Rarity(Card.Rarity))
			.Num(TEXT("weight"), Card.Weight).Bool(TEXT("guaranteed"), Card.bGuaranteed).Bool(TEXT("locked"), Card.bLocked)
			.Bool(TEXT("reserve"), bReserve).Close();
	}
	Cards += TEXT("]");
	JsonlOut += FDTTelJson(JsonHead(TEXT("offer"), Owner)).Int(TEXT("hand_id"), Hand.HandId).Str(TEXT("hand_kind"), DTTelNames::HandKind(Hand.HandKind))
		.Int(TEXT("level"), Hand.Level).Int(TEXT("reroll_of"), Hand.RerollOf).Bool(TEXT("guarantee_filled"), Hand.bGuaranteeFilled)
		.Int(TEXT("rerolls_left"), Tools.RerollsLeft).Int(TEXT("banish_left"), Tools.BanishLeft).Int(TEXT("lock_left"), Tools.LockLeft)
		.Raw(TEXT("cards"), Cards).Line();

	// run_end: every shown card counts (rerolls and refills included); a hand counts once, when first shown unrerolled.
	P->CardsOffered += Hand.Cards.Num();
	P->ReserveCards += Reserve;
	if (Hand.RerollOf == INDEX_NONE && !P->HandKinds.Contains(Hand.HandId))
	{
		++P->HandsByKind[FMath::Clamp(static_cast<int32>(Hand.HandKind), 0, DTTelemetryRecordsLocal::HandKinds - 1)];
	}
	P->HandKinds.Add(Hand.HandId, Hand.HandKind);
}

void FDTTelemetryCore::RecordOfferPick(int32 Owner, int32 HandId, int32 Slot, const FDTOfferCard& Card, double WallDecideSeconds)
{
	FPlayerInterval* P = RecordOwner(Owner);
	if (!P)
	{
		return;
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("offer_pick"), Owner)).Int(TEXT("hand_id"), HandId).Int(TEXT("slot"), Slot)
		.Str(TEXT("card_kind"), DTTelNames::CardKind(Card.Kind)).Name(TEXT("id"), Card.ContentId).Name(TEXT("target"), Card.TargetWeapon)
		.Str(TEXT("rarity"), DTTelNames::Rarity(Card.Rarity)).Num(TEXT("wall_decide_s"), WallDecideSeconds).Line();
	const EDTHandKind* Kind = P->HandKinds.Find(HandId);
	if (!Kind || *Kind != EDTHandKind::Patron)
	{
		++P->Decisions; // the run-start Patron hand is not a decision of the budget (offers.md R-OFF-08)
	}
	P->WallPausedSeconds += FMath::Max(0.0, WallDecideSeconds);
}

void FDTTelemetryCore::RecordOfferTool(int32 Owner, int32 HandId, EDTTelOfferTool Tool, int32 Slot, FName ContentId)
{
	if (!RecordOwner(Owner))
	{
		return;
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("offer_tool"), Owner)).Int(TEXT("hand_id"), HandId).Str(TEXT("tool"), DTTelNames::OfferTool(Tool))
		.Int(TEXT("slot"), Slot).Name(TEXT("id"), ContentId).Line();
}

// ---- keepers.md ---------------------------------------------------------------------------------------------------------

void FDTTelemetryCore::RecordKeeperPick(int32 Owner, const FDTTelKeeperPick& Pick)
{
	if (!RecordOwner(Owner))
	{
		return;
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("keeper_pick"), Owner)).Name(TEXT("blessing"), Pick.Blessing)
		.Str(TEXT("path"), FDTTelemetrySources::PathName(Pick.Path)).Name(TEXT("archetype"), Pick.Archetype)
		.Str(TEXT("role"), DTTelNames::BlessingRole(Pick.Role)).Int(TEXT("favor"), Pick.Favor).Int(TEXT("tier"), Pick.Tier)
		.Name(TEXT("leading"), Pick.Leading).Line();
}

void FDTTelemetryCore::RecordKeeperCommit(int32 Owner, EDTTelKeeperRole Role, EDTPath Path, FName PrimaryLeading)
{
	if (!RecordOwner(Owner))
	{
		return;
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("keeper_commit"), Owner)).Str(TEXT("role"), DTTelNames::KeeperRole(Role))
		.Str(TEXT("path"), FDTTelemetrySources::PathName(Path)).Name(TEXT("primary_leading"), PrimaryLeading).Line();
}

void FDTTelemetryCore::RecordStatusApplied(int32 Owner, EDTStatus Status, int32 Count)
{
	FPlayerInterval* P = RecordOwner(Owner);
	if (P && Status < EDTStatus::Count && Count > 0)
	{
		P->StatusApplied[static_cast<int32>(Status)] += Count;
	}
}

void FDTTelemetryCore::RecordIntercession(int32 Owner, const FDTTelIntercession& Cast)
{
	if (!RecordOwner(Owner))
	{
		return;
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("intercession"), Owner)).Name(TEXT("base"), Cast.Base).Name(TEXT("variant"), Cast.Variant)
		.Name(TEXT("layer"), Cast.Layer).Int(TEXT("enemies_hit"), Cast.EnemiesHit).Num(TEXT("damage"), Cast.Damage).Line();
}

// ---- combat_core.md -----------------------------------------------------------------------------------------------------

void FDTTelemetryCore::RecordDash(int32 Owner, FVector2f Dir, int32 ChargesAfter)
{
	FPlayerInterval* P = RecordOwner(Owner);
	if (!P)
	{
		return;
	}
	++P->Dashes;
	++P->RunDashes;
	JsonlOut += FDTTelJson(JsonHead(TEXT("dash"), Owner)).Num(TEXT("dir_x"), Dir.X).Num(TEXT("dir_y"), Dir.Y)
		.Int(TEXT("charges_after"), ChargesAfter).Line();
}

void FDTTelemetryCore::RecordHeroDamaged(int32 Owner, const FDTHeroHit& Hit, float Amount, float HpAfter, float MaxHp)
{
	FPlayerInterval* P = RecordOwner(Owner);
	if (!P)
	{
		return;
	}
	const double Share = MaxHp > 0.0f ? static_cast<double>(FMath::Max(0.0f, Amount)) / MaxHp : 0.0;
	P->DamageTakenShare += Share;
	P->RunDamageTakenShare += Share;
	JsonlOut += FDTTelJson(JsonHead(TEXT("hero_damaged"), Owner)).Num(TEXT("amount"), Amount).Num(TEXT("hp_after"), HpAfter)
		.Num(TEXT("max_hp"), MaxHp).Str(TEXT("hit_kind"), DTTelNames::HeroHitKind(Hit.Kind)).Str(TEXT("role"), DTTelNames::EnemyRole(Hit.SourceRole))
		.Name(TEXT("attack"), Hit.AttackId).Line();
}

void FDTTelemetryCore::RecordHeroDown(int32 Owner)
{
	if (RecordOwner(Owner))
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("hero_down"), Owner)).Line();
	}
}

void FDTTelemetryCore::RecordIntercessionDenied(int32 Owner)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		++P->IntercessionDenied;
	}
}

void FDTTelemetryCore::SetInputDevice(int32 Owner, EDTTelInputDevice Device)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		P->InputDevice = Device;
		P->bHasInputDevice = true;
	}
}

// ---- enemies.md / director.md / boss.md ---------------------------------------------------------------------------------

void FDTTelemetryCore::RecordThreatAction(FVector2f Origin, EDTEnemyRole Role, FName AttackId)
{
	if (bActive)
	{
		StepThreatOrigins.Add(Origin); // role and attack are the producers' records (dir_block, boss_end); the CSV counts
	}
}

void FDTTelemetryCore::RecordDirBlock(const FDTTelDirBlock& Block)
{
	using namespace DTTelemetryRecordsLocal;
	if (!bActive)
	{
		return;
	}
	FDTTelJson ByRole;
	for (int32 Role = 0; Role < static_cast<int32>(EDTEnemyRole::Count); ++Role)
	{
		if (Block.SpawnedByRole[Role] > 0)
		{
			ByRole.Int(DTTelNames::EnemyRole(static_cast<EDTEnemyRole>(Role)), Block.SpawnedByRole[Role]);
		}
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("dir_block"), INDEX_NONE)).Int(TEXT("block"), Block.Block).Str(TEXT("phase"), Block.Phase)
		.Num(TEXT("row_start_s"), Block.RowStartSeconds).Num(TEXT("start_s"), Block.StartSeconds).Num(TEXT("end_s"), Block.EndSeconds)
		.Str(TEXT("ended_by"), Block.EndedBy).Raw(TEXT("spawned_by_role"), ByRole.Close()).Int(TEXT("skipped_at_cap"), Block.SkippedAtCap)
		.Int(TEXT("dropins"), Block.DropIns).Int(TEXT("threat_actions"), Block.ThreatActions).Int(TEXT("kills"), Block.Kills)
		.Raw(TEXT("kill_horizon_p50_uu"), Block.KillHorizonP50UU >= 0.0f ? FDTTelJson::Real(Block.KillHorizonP50UU) : FString(TEXT("null")))
		.Int(TEXT("alive_peak"), Block.AlivePeak)
		.Raw(TEXT("dmg_taken"), ByOwner<float>(Block.DamageTakenShare, [](const float& V) { return FDTTelJson::Real(V); }))
		.Raw(TEXT("dashes"), ByOwner<int32>(Block.Dashes, [](const int32& V) { return FString::FromInt(V); })).Line();
}

void FDTTelemetryCore::RecordBossStart(FName Form, int32 Wins, int32 NumPlayers)
{
	if (bActive)
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("boss_start"), INDEX_NONE)).Name(TEXT("form"), Form).Int(TEXT("wins"), Wins)
			.Int(TEXT("players"), NumPlayers).Line();
	}
}

void FDTTelemetryCore::RecordEliteDeath(const FDTTelElite& Elite)
{
	if (!RecordOwner(Elite.OwnerId))
	{
		return;
	}
	TArray<FString> Names;
	DTElite::MaskNames(Elite.AffixMask).ParseIntoArray(Names, TEXT(","));
	FString Affixes;
	for (const FString& Name : Names)
	{
		Affixes += (Affixes.IsEmpty() ? TEXT("\"") : TEXT(",\"")) + Name + TEXT("\"");
	}
	JsonlOut += FDTTelJson(JsonHead(TEXT("elite"), Elite.OwnerId)).Int(TEXT("serial"), Elite.Serial).Str(TEXT("role"), DTTelNames::EnemyRole(Elite.Role))
		.Raw(TEXT("affixes"), TEXT("[") + Affixes + TEXT("]")).Int(TEXT("source"), Elite.SourceId).Num(TEXT("alive_s"), Elite.AliveSeconds)
		.Int(TEXT("shield_absorbed"), Elite.ShieldAbsorbed).Int(TEXT("shield_left"), Elite.ShieldLeft).Num(TEXT("drained"), Elite.Drained)
		.Num(TEXT("healed"), Elite.Healed).Int(TEXT("beat"), Elite.BeatId).Line();
}

void FDTTelemetryCore::RecordBossPhase(int32 Phase, float FightTimeSeconds, float HpFrac, bool bByTime)
{
	if (bActive)
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("boss_phase"), INDEX_NONE)).Int(TEXT("phase"), Phase).Num(TEXT("fight_time_s"), FightTimeSeconds)
			.Num(TEXT("hp_frac"), HpFrac).Bool(TEXT("by_time"), bByTime).Line();
	}
}

void FDTTelemetryCore::RecordBossCalibrated(float WindowDps, float EffectiveMax, bool bClamped)
{
	if (bActive)
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("boss_calibrated"), INDEX_NONE)).Num(TEXT("window_dps"), WindowDps)
			.Num(TEXT("effective_max"), EffectiveMax).Bool(TEXT("clamped"), bClamped).Line();
	}
}

void FDTTelemetryCore::RecordBossEnd(const FDTTelBossEnd& End)
{
	using namespace DTTelemetryRecordsLocal;
	if (!bActive)
	{
		return;
	}
	const FString PerPlayer = ByOwner<FDTTelBossPlayer>(End.PerPlayer, [](const FDTTelBossPlayer& Player)
	{
		TArray<TPair<FString, float>> Attacks;
		for (const TPair<FName, float>& Attack : Player.DamageTakenByAttack)
		{
			Attacks.Emplace(Attack.Key.IsNone() ? FString(TEXT("Contact")) : Attack.Key.ToString(), Attack.Value);
		}
		Attacks.Sort([](const TPair<FString, float>& A, const TPair<FString, float>& B) { return A.Key < B.Key; });
		FDTTelJson ByAttack;
		for (const TPair<FString, float>& Attack : Attacks)
		{
			ByAttack.Num(Attack.Key, Attack.Value);
		}
		return FDTTelJson().Raw(TEXT("damage_taken_by_attack"), ByAttack.Close()).Int(TEXT("dashes"), Player.Dashes)
			.Num(TEXT("hits_per_ring"), Player.HitsPerRing).Close();
	});
	JsonlOut += FDTTelJson(JsonHead(TEXT("boss_end"), INDEX_NONE)).Str(TEXT("outcome"), End.Outcome).Num(TEXT("fight_time_s"), End.FightTimeSeconds)
		.Num(TEXT("hp_left_frac"), End.HpLeftFrac).Int(TEXT("phase"), End.Phase).Raw(TEXT("per_player"), PerPlayer).Line();
}

// ---- progression.md -----------------------------------------------------------------------------------------------------

void FDTTelemetryCore::RecordXp(int32 Owner, int32 Gems, int32 Xp)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		P->StepGems += Gems;
		P->StepXp += Xp;
		GemsCollected += Gems;
	}
}

void FDTTelemetryCore::RecordLevelUp(int32 Owner, int32 Level, int64 XpTotal, int32 PendingHands)
{
	if (RecordOwner(Owner))
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("level_up"), Owner)).Int(TEXT("level"), Level).Int(TEXT("xp_total"), XpTotal)
			.Int(TEXT("pending_hands"), PendingHands).Line();
	}
}

void FDTTelemetryCore::RecordGemsMerged(int32 Count)
{
	if (bActive && Count > 0)
	{
		StepGemsMerged += Count;
	}
}

void FDTTelemetryCore::RecordGemsSpawned(int32 Count)
{
	if (bActive && Count > 0)
	{
		GemsSpawned += Count;
	}
}

void FDTTelemetryCore::SetGemsLive(int32 Count)
{
	if (bActive)
	{
		GemsLive = FMath::Max(0, Count);
	}
}

// ---- weapons.md ---------------------------------------------------------------------------------------------------------

void FDTTelemetryCore::RecordReforge(int32 Owner, bool bEvolve, FName Id, FName Target, int32 WeaponLevel, TConstArrayView<FName> Dormant)
{
	if (RecordOwner(Owner))
	{
		TArray<FString> Names;
		for (const FName& Name : Dormant)
		{
			Names.Add(Name.ToString());
		}
		JsonlOut += FDTTelJson(JsonHead(TEXT("reforge"), Owner)).Str(TEXT("reforge_kind"), bEvolve ? TEXT("Evolve") : TEXT("Path")).Name(TEXT("id"), Id)
			.Name(TEXT("target"), Target).Int(TEXT("weapon_level"), WeaponLevel).Str(TEXT("dormant"), *FString::Join(Names, TEXT(";"))).Line();
	}
}

void FDTTelemetryCore::RecordWeaponAcquired(int32 Owner, uint16 Source)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		P->Weapons.FindOrAdd(Source);
		JsonlOut += FDTTelJson(JsonHead(TEXT("weapon_acquired"), Owner)).Str(TEXT("source"), FDTTelemetrySources::GetName(Source)).Line();
	}
}

void FDTTelemetryCore::RecordWeaponLevel(int32 Owner, uint16 Source, int32 Level)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		P->Weapons.FindOrAdd(Source).Level = Level;
		JsonlOut += FDTTelJson(JsonHead(TEXT("weapon_level"), Owner)).Str(TEXT("source"), FDTTelemetrySources::GetName(Source))
			.Int(TEXT("level"), Level).Line();
	}
}

void FDTTelemetryCore::SetWeaponState(int32 Owner, uint16 Source, int32 Level, int32 LoadRank)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		FWeaponInterval& Weapon = P->Weapons.FindOrAdd(Source);
		Weapon.Level = Level;
		Weapon.LoadRank = LoadRank;
	}
}

void FDTTelemetryCore::RecordWeaponEmissions(int32 Owner, uint16 Source, int32 Spawned, int32 Throttled)
{
	if (FPlayerInterval* P = RecordOwner(Owner))
	{
		FWeaponInterval& Weapon = P->Weapons.FindOrAdd(Source);
		Weapon.Spawned += Spawned;
		Weapon.Throttled += Throttled;
	}
}

// ---- modifiers.md -------------------------------------------------------------------------------------------------------

void FDTTelemetryCore::RecordModifierSocketed(int32 Owner, FName Weapon, FName Modifier)
{
	if (RecordOwner(Owner))
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("modifier_socketed"), Owner)).Name(TEXT("weapon"), Weapon).Name(TEXT("modifier"), Modifier).Line();
	}
}

void FDTTelemetryCore::RecordModifierDormant(int32 Owner, FName Weapon, FName Modifier, bool bDormant)
{
	if (RecordOwner(Owner))
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("modifier_dormant"), Owner)).Name(TEXT("weapon"), Weapon).Name(TEXT("modifier"), Modifier)
			.Bool(TEXT("dormant"), bDormant).Line();
	}
}

// ---- per interval and run end -------------------------------------------------------------------------------------------

void FDTTelemetryCore::WriteIntervalRecords(int32 Player)
{
	const FPlayerInterval& P = Players[Player];

	// status{applied{Status: n}} (keepers.md), only when something was applied.
	FDTTelJson Applied;
	bool bAnyStatus = false;
	for (int32 Status = 0; Status < static_cast<int32>(EDTStatus::Count); ++Status)
	{
		if (P.StatusApplied[Status] > 0)
		{
			Applied.Int(DTTelNames::Status(static_cast<EDTStatus>(Status)), P.StatusApplied[Status]);
			bAnyStatus = true;
		}
	}
	if (bAnyStatus)
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("status"), Player)).Raw(TEXT("applied"), Applied.Close()).Line();
	}

	// weapon{source, level, load_rank, spawned, throttled, hits, damage} per owned weapon (weapons.md), by source name.
	TArray<uint16> Sources;
	P.Weapons.GetKeys(Sources);
	Sources.Sort([](uint16 A, uint16 B) { return FDTTelemetrySources::GetName(A) < FDTTelemetrySources::GetName(B); });
	for (const uint16 Source : Sources)
	{
		const FWeaponInterval& Weapon = P.Weapons[Source];
		const FDTTelemetrySourceTotals* Totals = P.BySource.Find(Source);
		JsonlOut += FDTTelJson(JsonHead(TEXT("weapon"), Player)).Str(TEXT("source"), FDTTelemetrySources::GetName(Source))
			.Int(TEXT("level"), Weapon.Level).Int(TEXT("load_rank"), Weapon.LoadRank).Int(TEXT("spawned"), Weapon.Spawned)
			.Int(TEXT("throttled"), Weapon.Throttled).Int(TEXT("hits"), Totals ? Totals->Hits : 0).Int(TEXT("damage"), Totals ? Totals->Damage : 0)
			.Line();
	}

	// intercession_denied{interval, count} (combat_core.md: per-interval count per player).
	if (P.IntercessionDenied > 0)
	{
		JsonlOut += FDTTelJson(JsonHead(TEXT("intercession_denied"), Player)).Int(TEXT("interval"), IntervalIndex)
			.Int(TEXT("count"), P.IntercessionDenied).Line();
	}
}

FString FDTTelemetryCore::RunEndExtras() const
{
	using namespace DTTelemetryRecordsLocal;
	const FString PerPlayer = ByOwner<FPlayerInterval>(Players, [](const FPlayerInterval& P)
	{
		FDTTelJson Hands;
		for (int32 Kind = 0; Kind < HandKinds; ++Kind)
		{
			Hands.Int(DTTelNames::HandKind(static_cast<EDTHandKind>(Kind)), P.HandsByKind[Kind]);
		}
		return FDTTelJson().Int(TEXT("decisions"), P.Decisions).Raw(TEXT("hands_by_kind"), Hands.Close()).Int(TEXT("reserve_cards"), P.ReserveCards)
			.Int(TEXT("cards_offered"), P.CardsOffered).Num(TEXT("wall_paused_s"), P.WallPausedSeconds).Int(TEXT("level"), P.Level)
			.Int(TEXT("xp_total"), P.XpTotal).Close();
	});
	// Appended after mirror_desyncs: an object that continues the run_end record.
	FString Out = FDTTelJson(FString()).Raw(TEXT("per_player"), PerPlayer).Int(TEXT("gems_spawned"), GemsSpawned)
		.Int(TEXT("gems_collected"), GemsCollected).Int(TEXT("gems_left"), GemsLive).Close();
	Out.LeftChopInline(1); // drop the closing brace: the caller closes the record
	return Out;
}
