// The Warden fight, world-free (02_Systems/boss.md; S1 = form I). Plain C++ so tests drive a whole fight headless
// against a real FDTEnemyPool and FDTWorldView; UDTBossSubsystem runs it in the Enemies phase after the enemy pool.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyTypes.h"
#include "Telemetry/DTTelemetryRecords.h"

class FDTEnemyPool;

/** Warden forms (R-BOS-01). S1 ships rows for form I only. */
enum class EDTBossForm : uint8
{
	I,
	II,
	III
};

/** One phase of a form: its rows of 09_Catalog/boss_warden.csv (DTCat::BossWarden) grouped by `phase`, in file order. */
struct FDTBossPhaseDef
{
	FName Name;
	float EnterHpFrac = 1.0f;
	float LatestStart = 0.0f;
	/** Indices into DTCat::BossWarden. */
	TArray<int32> Rows;
};

/** Side effects of the core; UDTBossSubsystem forwards them to the world, tests record them. */
class IDTBossSink
{
public:
	virtual ~IDTBossSink() = default;
	/** The hull (role Boss) and the Tank adds go through the enemy system's queue (applied at the next Enemies phase). */
	virtual void QueueSpawn(const FDTEnemySpawn& Spawn) = 0;
	virtual void BossStart(FName Form, int32 Wins) {}
	/** Phase is 1-based. */
	virtual void BossPhase(int32 Phase, float FightTime, float HpFrac, bool bByTime) {}
	virtual void BossCalibrated(float WindowDps, float EffectiveMax, bool bClamped) {}
	/** ThreatAction{Role Boss, AttackId} per attack started (boss.md Telemetry). */
	virtual void ThreatAction(const FVector2f& Origin, FName AttackId) {}
	/** boss_end; the world adds what the core cannot see (per-player dashes). */
	virtual void BossEnd(FDTTelBossEnd& End) {}
	/** The hull died: one Kill to KillerOwner (R-BOS-03). */
	virtual void BossDefeated(int64 StepIndex, int32 KillerOwner) {}
};

/** What one step of the fight reads and writes. */
struct FDTBossStepIn
{
	float StepSeconds = 0.0f;
	int64 StepIndex = 0;
	FDTWorldView* View = nullptr;
	FDTEnemyPool* Pool = nullptr;
	/** The Boss stream (R-BOS-12, R-TEC-02): the crossing lane offset and the adds' placement. */
	FRandomStream* Rng = nullptr;
};

/** One ring or crossing ring as fired (tests, DT.Boss.Stats). Angles in degrees, absolute (atan2 of the plane). */
struct FDTBossRingShot
{
	FName AttackId;
	bool bCross = false;
	int64 TelegraphStep = 0;
	int64 FireStep = 0;
	FVector2f Origin = FVector2f::ZeroVector;
	/** The target hero's position when the ring was telegraphed; slot 0 flies exactly at it (R-BOS-06, R-BOS-08). */
	FVector2f TargetPos = FVector2f::ZeroVector;
	float TargetBearingDeg = 0.0f;
	float LaneCentreDeg = 0.0f;
	/** Angle between the two rounds bounding the lane. */
	float LaneGapDeg = 0.0f;
	/** Chord between the bounding rounds at C.BOSS.LANE_MEASURE_UU. */
	float LaneChordUU = 0.0f;
	/** Crossing ring: its lane's bearing minus the first ring's (signed). */
	float CrossOffsetDeg = 0.0f;
	/** What the telegraph drew: the gap and its centre. */
	float TelegraphGapDeg = 0.0f;
	float TelegraphLaneDeg = 0.0f;
	int32 Count = 0;
	int32 Fired = 0;
	TArray<float> SlotDeg;
};

/** One attack from telegraph to damage (T-BOS-05): footprint drawn vs damage dealt. */
struct FDTBossAttackRec
{
	FName AttackId;
	EDTTelegraphShape Shape = EDTTelegraphShape::Band;
	int32 Phase = 0; // 1-based
	int64 TelegraphStep = 0;
	int64 LandStep = -1; // the first step damage can land; -1 while pending
	/** Band: length and width; Arc: radius and degrees; ring shapes: radius and gap degrees; hazard: radius. */
	float FootprintLength = 0.0f;
	float FootprintWidth = 0.0f;
	/** The same two numbers for what the attack actually did (charge: travelled + hull diameter; sweep: radius and arc). */
	float DamageLength = 0.0f;
	float DamageWidth = 0.0f;
};

/** Counters for DT.Boss.Stats and tests. */
struct FDTBossCounters
{
	TMap<FName, int32> AttacksStarted;
	int32 RingsFired = 0;
	int32 RoundsFired = 0;
	int32 RoundsRefused = 0;
	int32 SpiralEmissions = 0;
	int32 SpiralSkipped = 0;
	/** Smallest angle between an emitted spiral round and the lane centre (deg); 180 until the spiral fires. */
	float SpiralMinLaneDeg = 180.0f;
	int32 HazardHits = 0;
	int32 AddsQueued = 0;
	/** R-BOS-01: fights whose form had no rows and played form I. */
	int32 FormFallbacks = 0;
	int32 AttacksStartedInShift = 0;
};

/**
 * The fight brain (boss.md Data FDTBossBrain): form, phases that only advance, the Charge/Sweep body loop, rings with
 * a dodge lane and their crossing ring, the two-arm spiral with a rotating lane, the shrinking hazard ring, Tank adds,
 * HP calibration and telemetry. Every angle that places a round is a pure function of the fight step and the target
 * bearing (R-BOS-06); the only draws are the crossing-lane offset and the adds' angles, from the Boss stream.
 */
class DUSKTRACER_API FDTBossCore
{
public:
	FDTBossCore();

	// --- rules as pure functions -----------------------------------------------------------------------------------
	/** R-BOS-01: form by cumulative wins (Heat and the run never change it). */
	static EDTBossForm FormForWins(int32 Wins);
	static const TCHAR* FormName(EDTBossForm Form);
	/** True when the catalogue has rows for Form. */
	static bool HasRows(EDTBossForm Form);
	/** R-BOS-07: the lane's minimum chord between the bounding rounds' centres: hero diameters plus two round diameters. */
	static float LaneChordRequired();
	/** The angle that chord subtends at C.BOSS.LANE_MEASURE_UU (deg). */
	static float LaneAngleRequiredDeg();
	/** Contiguous empty slots a ring of Count leaves for its lane (at least one). */
	static int32 LaneSlots(int32 Count);
	/** Chord of an angle GapDeg at Distance. */
	static float ChordAt(float GapDeg, float Distance);
	/** Signed difference of two bearings in (-180, 180]. */
	static float WrapDeg(float Deg);
	static int32 NumPhases();
	static const FDTBossPhaseDef& GetPhaseDef(int32 PhaseIndex);
	/** The row of Attack in a phase, or INDEX_NONE. */
	static int32 FindRow(int32 PhaseIndex, FName Attack);

	// --- fight --------------------------------------------------------------------------------------------------------
	/** Forgets any fight (a new run). */
	void Reset();
	/** Starts a fight: queues the hull (HP scale C.BOSS.HP_MAX_MULT: the reserve, never the director's multiplier). */
	void Start(int32 Wins, TConstArrayView<FVector2f> Heroes, IDTBossSink& Sink);
	/** One fixed step (EDTSimPhase::Enemies, after the enemy pool): adopts the hull, then runs the fight. */
	void Step(const FDTBossStepIn& In, IDTBossSink& Sink);
	/** Ends the fight from outside (every hero down, a new run): writes boss_end with Outcome. */
	void End(const FString& Outcome, IDTBossSink& Sink);
	/** Debug (DT.Boss.Phase): advances to Phase (1-based), entering each phase on the way in order; never goes back. */
	void ForcePhase(int32 Phase, IDTBossSink& Sink);

	bool IsStarted() const { return bStarted; }
	bool IsActive() const { return bStarted && !bEnded; }
	bool HasHull() const { return bAdopted && !bEnded; }
	bool IsEnded() const { return bEnded; }
	const FString& GetOutcome() const { return Outcome; }
	EDTBossForm GetForm() const { return Form; }
	/** 1-based; 0 before the hull is in the pool. */
	int32 GetPhase() const { return Phase + 1; }
	float GetFightTime() const { return FightTime; }
	float GetShiftLeft() const { return ShiftLeft; }
	FDTEnemyHandle GetHull() const { return Hull; }
	float GetReserveHp() const { return ReserveHp; }
	float GetEffectiveMax() const { return EffectiveMax; }
	float GetDamageDealt() const { return Dealt; }
	/** The effective HP fraction the phases and the bar read (R-BOS-09). */
	float GetHpFrac() const { return HpFrac; }
	bool IsCalibrated() const { return bCalibrated; }
	bool IsHazardOn() const { return Hazard.bOn; }
	bool IsHazardShrinking() const { return Hazard.bOn && Hazard.WindLeft <= 0.0f; }
	float GetHazardRadius() const { return Hazard.Radius; }
	const FDTBossCounters& GetCounters() const { return Counters; }
	TConstArrayView<FDTBossRingShot> GetRingShots() const { return RingShots; }
	TConstArrayView<FDTBossAttackRec> GetAttacks() const { return Attacks; }

private:
	enum class EBody : uint8
	{
		Walk,
		Windup,      // charge or dash charge: still, band telegraph
		Charge,      // straight along the band
		Recover,
		SweepWindup  // arc telegraph, lands at its end
	};

	struct FBody
	{
		EBody State = EBody::Walk;
		float Left = 0.0f;
		float Total = 0.0f;
		int32 Row = INDEX_NONE;
		FVector2f Dir = FVector2f(1.0f, 0.0f);
		FVector2f Start = FVector2f::ZeroVector;
		float Travel = 0.0f;
		float Moved = 0.0f;
		uint32 HitMask = 0;
		int32 Rec = INDEX_NONE;
		bool bLastWasCharge = false;
	};

	struct FPendingRing
	{
		int32 Row = INDEX_NONE;
		int32 Shot = INDEX_NONE; // index into RingShots
		int32 Rec = INDEX_NONE;
		int32 Count = 0;
		int32 EmptyFrom = 0;     // first empty slot (mod Count)
		int32 Empty = 0;         // contiguous empty slots
		float SlotPhaseDeg = 0.0f;
		float Left = 0.0f;
		float Total = 0.0f;
		int32 Target = INDEX_NONE;
	};

	struct FCrossPlan
	{
		int32 Row = INDEX_NONE;
		float StartLeft = 0.0f;
		float FirstLaneDeg = 0.0f;
	};

	struct FSpiral
	{
		bool bOn = false;
		int32 Row = INDEX_NONE;
		int32 Rec = INDEX_NONE;
		float WindLeft = 0.0f;
		float Total = 0.0f;
		float ArmDeg = 0.0f;
		float LaneDeg = 0.0f;
		float EmitAccum = 0.0f;
	};

	struct FHazard
	{
		bool bOn = false;
		int32 Row = INDEX_NONE;
		int32 Rec = INDEX_NONE;
		float WindLeft = 0.0f;
		float Total = 0.0f;
		float Radius = 0.0f;
		TArray<float> OutsideFor;
		TArray<float> TickAccum;
		TArray<bool> bHitOnce;
	};

	bool FindHull(const FDTEnemyPool& Pool);
	void Adopt(const FDTBossStepIn& In, IDTBossSink& Sink);
	void Finish(const FString& InOutcome, int64 StepIndex, IDTBossSink& Sink);
	void UpdateHp(FDTEnemyPool& Pool, IDTBossSink& Sink);
	void EnterPhase(int32 PhaseIndex, bool bByTime, IDTBossSink& Sink);
	void StartAttack(const FDTBossStepIn& In, int32 Row, IDTBossSink& Sink);
	int32 BeginRec(FName AttackId, EDTTelegraphShape Shape, int64 Step, float Length, float Width);
	void StepBody(const FDTBossStepIn& In, IDTBossSink& Sink);
	void StartRing(const FDTBossStepIn& In, int32 Row, bool bCross, float FirstLaneDeg, IDTBossSink& Sink);
	void FireRing(const FDTBossStepIn& In, FPendingRing& Ring);
	void StepRings(const FDTBossStepIn& In, IDTBossSink& Sink);
	void StepSpiral(const FDTBossStepIn& In, IDTBossSink& Sink);
	void StepHazard(const FDTBossStepIn& In, IDTBossSink& Sink);
	void StepAdds(const FDTBossStepIn& In, IDTBossSink& Sink);
	void Walk(const FDTBossStepIn& In);
	void PublishTelegraphs(FDTWorldView& View, const FDTEnemyPool& Pool) const;
	void TallyHeroHits(const FDTWorldView& View, int32 From);
	void AddHeroHit(FDTWorldView& View, int32 Player, float Damage, FName AttackId, const FVector2f& At);
	int32 NearestHero(const FVector2f& At, const FDTWorldView& View) const;
	FVector2f HullPos(const FDTEnemyPool& Pool) const;
	FVector2f Facing(const FDTEnemyPool& Pool) const;
	/** Furthest the hull can travel from From along Dir inside the arena and the hazard ring (charges). */
	float TravelLimit(const FVector2f& From, const FVector2f& Dir) const;
	FVector2f KeepInside(const FVector2f& At) const;
	bool IsShifting() const { return ShiftLeft > 0.0f; }

	// Fight state (boss.md Data FDTBossBrain + HP state).
	bool bStarted = false;
	bool bAdopted = false;
	bool bEnded = false;
	FString Outcome;
	EDTBossForm Form = EDTBossForm::I;
	int32 Wins = 0;
	FDTEnemyHandle Hull;
	int64 FightStartStep = 0;
	int32 Phase = -1;
	float ShiftLeft = 0.0f;
	float FightTime = 0.0f;
	float ReserveHp = 0.0f;
	float HpBase = 0.0f;       // the pool HP the damage is measured from: the reserve, then the effective max
	float EffectiveMax = 0.0f;
	float Dealt = 0.0f;
	float HpFrac = 1.0f;
	bool bCalibrated = false;
	float RingTimer = 0.0f;
	float DashTimer = 0.0f;
	float AddsTimer = 0.0f;
	FBody Body;
	TArray<FPendingRing> Rings;
	TArray<FCrossPlan> Crosses;
	FSpiral Spiral;
	FHazard Hazard;

	// Telemetry and test records.
	TArray<TMap<FName, float>> DamageByAttack; // by player
	TArray<int32> RingHits;                    // by player
	FDTBossCounters Counters;
	TArray<FDTBossRingShot> RingShots;
	TArray<FDTBossAttackRec> Attacks;
};
