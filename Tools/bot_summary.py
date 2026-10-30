"""Summary of Bot v1 runs (Tools/BotRuns.ps1 writes the run folders; Docs/Validation/S1_bot_baseline.md). Stdlib only.

Usage:  python -I Tools/bot_summary.py <Saved/BotRuns/<stamp>> [--check T-DIR-01[,T-OFF-15]]

Each run folder <label>_s<seed>/ holds bot.csv (+ bot.csv.txt), the bot's per-minute record (DTBotSubsystem.h), and the
run's telemetry pair <seed>_<time>.csv/.jsonl copied from Saved/Playtests. Writes summary.csv and summary.md next to the
folders and prints one line per check:
  T-DIR-01: PASS|FAIL (...)   no-pick arm median death time in C.DIR.NOPICK_DEATH_BAND_S over >= C.DIR.BOT_RUNS seeds
  T-OFF-15: PASS|FAIL|n/a (...) Steered runs that reached the boss: decisions in C.DECISIONS.PER_RUN +- tolerance,
                               Reserve share <= C.OFFER.RESERVE_SHARE_MAX (bot-measured, GATED until calibrated)
Exit 1 when a requested check fails (or cannot run), else 0.
Window metrics (damage-taken share per minute, dashes per minute) are the run's telemetry read by Tools/pressure.py
(T-DIR-02): JSONL hero_damaged / dash records inside C.DIR.BAND_WINDOW_S over the time the hero was alive in it; a run
that ended before the window has none. The bot's own per-minute record (bot.csv) is kept for debugging only.
Per arm it also reports the pressure sources in that window (damage by hit kind and role, threat actions, the kill horizon),
the boss reach, the Keeper share at C.TEL.POWER_CHECK_LEVEL (`sources` records up to the level-up), and damage per Intercession
cast, each read per arm because a pooled mean hides builds that miss in opposite directions.
"""

import csv
import glob
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dtconst  # noqa: E402
import power_split  # noqa: E402
import pressure  # noqa: E402


def read_kv(path):
	out = {}
	if os.path.exists(path):
		with open(path, encoding="utf-8") as f:
			for line in f:
				if "=" in line:
					k, v = line.rstrip("\n").split("=", 1)
					out[k.strip()] = v.strip()
	return out


def read_run_end(jsonl):
	rec = None
	if jsonl:
		with open(jsonl, encoding="utf-8") as f:
			for line in f:
				if '"kind":"run_end"' in line:
					try:
						rec = json.loads(line)
					except ValueError:
						pass
	return rec


ROLES = ("Swarm", "Tank", "Rusher", "Turret")
HAZARD_KINDS = ("HazardRing", "Charge", "Sweep", "DashCharge")  # `hit_kind` values of direct attacks (combat_core.md)
ECON_T = 630.0  # C.LEVEL.L16_MIN: the economy columns read cumulative values here (or at the run end if earlier)


def drop_values(consts):
	return {r: consts[f"DROP.XP_{r.upper()}"]["value"] for r in ROLES}


def curve_cumulative(consts):
	"""[(level, xp_cumulative)] from Docs/09_Catalog/xp_curve.csv, extended past its last row by C.XP.GROWTH."""
	rows = []
	with open(os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "xp_curve.csv"), encoding="utf-8", newline="") as f:
		for row in csv.DictReader(f):
			rows.append((int(row["level"]), int(row["xp_to_next"]), int(row["xp_cumulative"])))
	out = [(lvl, cum) for lvl, _, cum in rows]
	lvl, step, cum = rows[-1]
	growth = consts["XP.GROWTH"]["value"]
	for _ in range(60):
		cum += step
		lvl += 1
		step = int(round(step * growth))
		out.append((lvl, cum))
	return out


def level_for(xp, curve):
	level = 1
	for lvl, cum in curve:
		if xp >= cum:
			level = lvl
	return level


def timeline_model_xp(consts, t_end, dt=0.05):
	"""Full-pickup model of the S1 timeline (balance/model.md method): every regular spawn, drop-in and event enemy the
	timeline asks for is killed; regular XP is the spawn rate times the mix-weighted drop value, Threat rows overlay the
	Harvest row, Breath pauses spawns and drop-ins, elites drop C.DROP.XP_ELITE_MULT times. Ignores the alive and share
	caps (they move spawns, not the request). XP up to t_end."""
	xp = drop_values(consts)
	elite = consts["DROP.XP_ELITE_MULT"]["value"]
	breath = consts["DIR.BREATH_S"]["value"]
	with open(os.path.join(dtconst.ROOT, "Docs", "09_Catalog", "timeline_arena1.csv"), encoding="utf-8", newline="") as f:
		rows = list(csv.DictReader(f))
	harvest = [r for r in rows if r["phase"] == "Harvest"]
	threats = [r for r in rows if r["phase"] == "Threat"]

	def mix(r):
		w = {k: float(r["w_" + k.lower()] or 0) for k in ROLES}
		s = sum(w.values())
		return sum(w[k] * xp[k] for k in ROLES) / s if s else 0.0

	total, t = 0.0, 0.0
	for r in threats:
		if float(r["start_s"]) < t_end:
			n = int(r["event_count"] or 0)
			total += {"EliteWave": n * mix(r) * elite, "SwarmRing": n * xp["Swarm"], "RusherStampede": n * xp["Rusher"],
				"TurretNest": n * xp["Turret"]}.get(r["event"], 0.0)
	while t < t_end - 1e-9:
		step = min(dt, t_end - t)
		h = [r for r in harvest if float(r["start_s"]) <= t + 1e-9][-1]
		active, in_breath = None, False
		for r in threats:
			s, d = float(r["start_s"]), float(r["duration_s"])
			if s <= t + 1e-9 < s + d:
				active = r
			elif s + d <= t + 1e-9 < s + d + breath:
				in_breath = True
		if not in_breath:
			src = active or h
			total += float(src["spawn_rate_per_s"]) * mix(src) * step
			if h["dropin_interval_s"]:
				interval = float(h["dropin_interval_s"]) / (consts["DIR.DROPIN_THREAT_MULT"]["value"] if active else 1)
				total += (float(h["dropin_w_rusher"]) * xp["Rusher"] + float(h["dropin_w_turret"]) * xp["Turret"]) * step / interval
		t += step
	return total


def read_economy(jsonl, consts, t_cut):
	"""XP collected (xp records), XP spawned by role (dir_block spawned_by_role x C.DROP.*, a block prorated by its overlap;
	elite bonus not included), kills, and level-up times, up to t_cut."""
	xp = drop_values(consts)
	collected, kills = 0.0, 0.0
	spawned = {r: 0.0 for r in ROLES}
	level_t = {}
	if not jsonl:
		return None
	with open(jsonl, encoding="utf-8") as f:
		for line in f:
			if '"kind":"xp"' in line or '"kind":"dir_block"' in line or '"kind":"level_up"' in line:
				rec = json.loads(line, object_pairs_hook=pressure.first_wins)
				kind, t = rec["kind"], float(rec["t"])
				if kind == "xp" and rec.get("owner") == 0 and t <= t_cut:
					collected += rec.get("xp", 0)
				elif kind == "level_up" and rec.get("owner") == 0:
					level_t.setdefault(int(rec["level"]), t)
				elif kind == "dir_block":
					s, e = float(rec["start_s"]), float(rec["end_s"])
					share = 1.0 if e <= t_cut else (max(0.0, t_cut - s) / (e - s) if e > s else 0.0)
					for role, n in (rec.get("spawned_by_role") or {}).items():
						if role in spawned:
							spawned[role] += n * xp[role] * share
					kills += rec.get("kills", 0) * share
	return {"collected": collected, "spawned": spawned, "kills": kills, "level_t": level_t}


def read_blocks(jsonl, lo, hi):
	"""Threat actions in [lo, hi) (dir_block, prorated by overlap), per minute of overlap, and the overlap-weighted median kill horizon."""
	actions, minutes, horizon = 0.0, 0.0, []
	if not jsonl:
		return None, None
	with open(jsonl, encoding="utf-8") as f:
		for line in f:
			if '"kind":"dir_block"' not in line:
				continue
			rec = json.loads(line, object_pairs_hook=pressure.first_wins)
			s, e = float(rec["start_s"]), float(rec["end_s"])
			overlap = max(0.0, min(hi, e) - max(lo, s))
			if overlap <= 0 or e <= s:
				continue
			actions += rec.get("threat_actions", 0) * overlap / (e - s)
			minutes += overlap / 60.0
			if rec.get("kill_horizon_p50_uu") is not None:
				horizon.append((rec["kill_horizon_p50_uu"], overlap))
	if minutes <= 0:
		return None, None
	h = None
	if horizon:
		horizon.sort()
		half, seen = sum(w for _, w in horizon) / 2.0, 0.0
		for v, w in horizon:
			seen += w
			if seen >= half:
				h = v
				break
	return actions / minutes, h


def keeper_share_until(jsonl, t_cut):
	"""Damage share of Keeper.* over the `sources` records up to t_cut (player 0); None when none."""
	totals = {}
	with open(jsonl, encoding="utf-8") as f:
		for line in f:
			if '"kind":"sources"' not in line:
				continue
			rec = json.loads(line, object_pairs_hook=pressure.first_wins)
			if rec.get("owner") != 0 or float(rec.get("t", 0.0)) > t_cut:
				continue
			for src, v in (rec.get("damage") or {}).items():
				totals[src] = totals.get(src, 0) + v
	total, sh = power_split.shares(totals)
	return sh["Keeper"] if total else None


def read_casts(jsonl):
	"""(casts, mean damage per cast, variant) of player 0's Intercession records."""
	damage, variant = [], ""
	with open(jsonl, encoding="utf-8") as f:
		for line in f:
			if '"kind":"intercession"' in line:
				rec = json.loads(line, object_pairs_hook=pressure.first_wins)
				if rec.get("owner") == 0 and "damage" in rec:
					damage.append(float(rec["damage"]))
					variant = rec.get("variant") or variant
	return len(damage), (statistics.mean(damage) if damage else None), variant


def summarise_run(folder, consts):
	name = os.path.basename(folder)
	label, _, seed = name.rpartition("_s")
	kv = read_kv(os.path.join(folder, "bot.csv.txt"))
	jsonls = glob.glob(os.path.join(folder, "*.jsonl"))
	jsonl = jsonls[0] if jsonls else None
	csv_path = jsonl[:-6] + ".csv" if jsonl and os.path.exists(jsonl[:-6] + ".csv") else None
	end_s = float(kv.get("end_s", "0") or 0)
	rates, _ = pressure.window_rates(jsonl, csv_path, consts)
	dmg, dashes, alive = rates.get(0, (None, None, 0.0))
	run_end = read_run_end(jsonl) or {}
	p0 = (run_end.get("per_player") or {}).get("0", {})
	offered = p0.get("cards_offered", 0)
	keeper = level = None
	if jsonl:
		players, _ = power_split.read_sources(jsonl)
		levels = power_split.read_levels(csv_path)
		data = players.get(0, {"damage": {}, "kills": {}})
		total, shares = power_split.shares(data["damage"])
		keeper = shares["Keeper"] if total else None
		level = levels.get(0)
	lo, hi = consts["DIR.BAND_WINDOW_S"]["value"]
	sources = {}
	if jsonl and alive > 0:
		sources = {k: v / alive for k, v in pressure.sources_from_jsonl(jsonl, lo, hi).items()}
	threat_per_min, horizon = read_blocks(jsonl, lo, hi)
	t_cut = min(ECON_T, end_s) if end_s > 0 else ECON_T
	econ = read_economy(jsonl, consts, t_cut) or {"collected": 0.0, "spawned": {r: 0.0 for r in ROLES}, "kills": 0.0, "level_t": {}}
	spawned_xp = sum(econ["spawned"].values())
	model_xp = timeline_model_xp(consts, t_cut)
	ref_ratio = consts["XP.REFERENCE_PICKUP_RATIO"]["value"]
	curve = curve_cumulative(consts)
	check_level = consts["TEL.POWER_CHECK_LEVEL"]["value"]
	t_check = econ["level_t"].get(check_level)
	casts, cast_damage, _ = read_casts(jsonl) if jsonl else (0, None, "")
	to_boss = consts["RUN.LENGTH_TO_BOSS"]["value"]
	return {
		"label": label,
		"seed": int(seed) if seed.isdigit() else seed,
		"outcome": kv.get("outcome", "missing"),
		"end_s": round(end_s, 1),
		"level_630": int(kv["level_630"]) if kv.get("level_630") else "",
		"level_end": level if level is not None else p0.get("level", ""),
		"decisions": p0.get("decisions", ""),
		"reserve_share": round(p0.get("reserve_cards", 0) / offered, 3) if offered else "",
		"dmg_share_per_min_window": "" if dmg is None else round(dmg, 3),
		"dashes_per_min_window": "" if dashes is None else round(dashes, 2),
		"window_alive_min": round(alive, 2),
		"keeper_share": "" if keeper is None else round(keeper, 3),
		"keeper_share_l16": "" if t_check is None or not jsonl else round(keeper_share_until(jsonl, t_check) or 0.0, 3),
		"reached_boss": int(kv.get("outcome") == "victory" or end_s >= to_boss),
		"casts": casts,
		"cast_damage": "" if cast_damage is None else round(cast_damage),
		"threat_per_min_window": "" if threat_per_min is None else round(threat_per_min, 1),
		"kill_horizon_window": "" if horizon is None else round(horizon),
		"dmg_contact_window": round(sum(v for k, v in sources.items() if k.startswith("Contact/")), 3) if sources else "",
		"dmg_bullet_window": round(sum(v for k, v in sources.items() if k.startswith("Bullet/")), 3) if sources else "",
		"dmg_hazard_window": round(sum(v for k, v in sources.items() if k.split("/")[0] in HAZARD_KINDS), 3) if sources else "",
		**{f"dmg_{hk.lower()}_window": round(sum(v for k, v in sources.items() if k.startswith(hk + "/")), 3) if sources else "" for hk in HAZARD_KINDS},
		"dmg_rusher_window": round(sum(v for k, v in sources.items() if k.endswith("/Rusher")), 3) if sources else "",
		"dmg_tank_window": round(sum(v for k, v in sources.items() if k.endswith("/Tank")), 3) if sources else "",
		"dmg_turret_window": round(sum(v for k, v in sources.items() if k.endswith("/Turret")), 3) if sources else "",
		"dmg_swarm_window": round(sum(v for k, v in sources.items() if k.endswith("/Swarm")), 3) if sources else "",
		"gem_pickup_ratio": round(run_end["gems_collected"] / run_end["gems_spawned"], 3) if run_end.get("gems_spawned") else "",
		"t_l8": round(econ["level_t"][8], 1) if 8 in econ["level_t"] else "",
		"t_l16": round(econ["level_t"][16], 1) if 16 in econ["level_t"] else "",
		"econ_t": round(t_cut, 1),
		"xp_collected": round(econ["collected"]),
		"xp_spawned": round(spawned_xp),
		**{f"xp_spawned_{r.lower()}": round(econ["spawned"][r]) for r in ROLES},
		"xp_model": round(model_xp),
		"kills_per_min": round(econ["kills"] / (t_cut / 60.0), 1) if t_cut > 0 else "",
		"xp_pickup_of_spawned": round(econ["collected"] / spawned_xp, 3) if spawned_xp else "",
		"xp_pickup_of_model": round(econ["collected"] / model_xp, 3) if model_xp else "",
		"level_ref_ratio_spawned": level_for(spawned_xp * ref_ratio, curve),
		"dash_presses": kv.get("dash_presses", ""),
		"intercession_presses": kv.get("intercession_presses", ""),
		"telemetry": os.path.basename(jsonl) if jsonl else "",
	}


def fmt_band(v):
	return f"{v[0]}-{v[1]}" if isinstance(v, list) else str(v)


def mean(xs):
	xs = [x for x in xs if x != "" and x is not None]
	return round(statistics.mean(xs), 3) if xs else ""


def main(argv):
	if len(argv) < 2:
		print(__doc__)
		return 1
	root = argv[1]
	checks = []
	if "--check" in argv:
		checks = [c for c in argv[argv.index("--check") + 1].split(",") if c]
	consts = dtconst.load()
	runs = [summarise_run(d, consts) for d in sorted(glob.glob(os.path.join(root, "*_s*"))) if os.path.isdir(d)]
	if not runs:
		print(f"BOTRUNS: no run folders in {root}")
		return 1
	fields = list(runs[0].keys())
	with open(os.path.join(root, "summary.csv"), "w", encoding="utf-8", newline="") as f:
		w = csv.DictWriter(f, fieldnames=fields)
		w.writeheader()
		w.writerows(runs)

	dmg_band = consts["DMG_TAKEN.BAND_MIN10_13"]["value"]
	dash_band = consts["DASHES.BAND_MIN10_13"]["value"]
	keeper_need = consts["POWER.KEEPER_SHARE_L16"]["value"]
	nopick_band = consts["DIR.NOPICK_DEATH_BAND_S"]["value"]
	bot_runs = consts["DIR.BOT_RUNS"]["value"]
	per_run = consts["DECISIONS.PER_RUN"]["value"]
	tol = consts["OFFER.DECISIONS_TOLERANCE"]["value"]
	reserve_max = consts["OFFER.RESERVE_SHARE_MAX"]["value"]

	lines = ["# Bot runs summary", "", f"Folder: `{root}`", "",
		f"Bands: damage taken {fmt_band(dmg_band)} share/min, dashes {fmt_band(dash_band)} /min (window {fmt_band(consts['DIR.BAND_WINDOW_S']['value'])} s), "
		f"Keeper share >= {keeper_need} at L16, no-pick death {fmt_band(nopick_band)} s, decisions {per_run} +- {tol}.", "",
		"| Arm | Runs | Deaths | Wins | Median end s | Level @630 | Decisions | Dmg share/min (window) | Dashes/min (window) | Keeper share | Reserve share | Gem pickup ratio |",
		"|---|---|---|---|---|---|---|---|---|---|---|---|"]
	labels = []
	for r in runs:
		if r["label"] not in labels:
			labels.append(r["label"])
	for label in labels:
		rs = [r for r in runs if r["label"] == label]
		lines.append("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
			label, len(rs), sum(r["outcome"] == "death" for r in rs), sum(r["outcome"] == "victory" for r in rs),
			round(statistics.median(r["end_s"] for r in rs), 1), mean(r["level_630"] for r in rs), mean(r["decisions"] for r in rs),
			mean(r["dmg_share_per_min_window"] for r in rs), mean(r["dashes_per_min_window"] for r in rs),
			mean(r["keeper_share"] for r in rs), mean(r["reserve_share"] for r in rs), mean(r["gem_pickup_ratio"] for r in rs)))
	ref_ratio = consts["XP.REFERENCE_PICKUP_RATIO"]["value"]
	lines += ["", f"XP economy at {ECON_T:.0f} s (or the run end): collected and spawned XP from telemetry (`xp`, `dir_block` x `C.DROP.*`), "
		f"model = full-pickup timeline model; reference ratio {ref_ratio} of the model; L8 / L16 targets "
		f"{consts['LEVEL.L8_MIN']['value']} / {consts['LEVEL.L16_MIN']['value']} s.", "",
		"| Arm | t L8 s (n) | t L16 s (n) | XP collected | XP spawned (Swarm/Tank/Rusher/Turret) | XP model | Kills/min | Collected / spawned | Collected / model | Level at ref ratio of spawned |",
		"|---|---|---|---|---|---|---|---|---|---|"]
	for label in labels:
		rs = [r for r in runs if r["label"] == label]
		l8 = [r["t_l8"] for r in rs if r["t_l8"] != ""]
		l16 = [r["t_l16"] for r in rs if r["t_l16"] != ""]
		lines.append("| {} | {} ({}) | {} ({}) | {} | {} ({}) | {} | {} | {} | {} | {} |".format(
			label, mean(l8), len(l8), mean(l16), len(l16), mean(r["xp_collected"] for r in rs), mean(r["xp_spawned"] for r in rs),
			"/".join(str(round(mean(r[f"xp_spawned_{x.lower()}"] for r in rs) or 0)) for x in ROLES), mean(r["xp_model"] for r in rs),
			mean(r["kills_per_min"] for r in rs), mean(r["xp_pickup_of_spawned"] for r in rs), mean(r["xp_pickup_of_model"] for r in rs),
			mean(r["level_ref_ratio_spawned"] for r in rs)))
	lines += ["", f"Pressure sources in {fmt_band(consts['DIR.BAND_WINDOW_S']['value'])} s (share of max HP per alive minute; threat actions per minute; kill horizon uu), and boss reach:", "",
		"| Arm | n in window | Dmg | Dashes | Contact | Rounds | Direct attacks | Swarm | Tank | Rusher | Turret | Threat/min | Horizon | Reached boss | Wins |",
		"|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
	for label in labels:
		rs = [r for r in runs if r["label"] == label]
		lines.append("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {}/{} | {} |".format(
			label, sum(r["dmg_share_per_min_window"] != "" for r in rs), mean(r["dmg_share_per_min_window"] for r in rs),
			mean(r["dashes_per_min_window"] for r in rs), mean(r["dmg_contact_window"] for r in rs), mean(r["dmg_bullet_window"] for r in rs),
			mean(r["dmg_hazard_window"] for r in rs), mean(r["dmg_swarm_window"] for r in rs), mean(r["dmg_tank_window"] for r in rs),
			mean(r["dmg_rusher_window"] for r in rs), mean(r["dmg_turret_window"] for r in rs), mean(r["threat_per_min_window"] for r in rs),
			mean(r["kill_horizon_window"] for r in rs), sum(r["reached_boss"] for r in rs), len(rs), sum(r["outcome"] == "victory" for r in rs)))
	check_level = consts["TEL.POWER_CHECK_LEVEL"]["value"]
	arch_band = consts["POWER.ARCHETYPE_BAND"]["value"]
	cast_band = consts["INTERCESSION.BAND"]["value"]
	power = {}
	for label in labels:
		v = mean(r["keeper_share_l16"] for r in runs if r["label"] == label)
		if v != "" and label.lower().startswith("steered"):
			power[label] = v
	arm_mean = statistics.mean(power.values()) if power else 0.0
	per_keeper = {}
	for label in labels:
		v = mean(r["cast_damage"] for r in runs if r["label"] == label)
		parts = label.split("-")
		if v != "" and len(parts) > 1:
			per_keeper.setdefault(parts[1], {})[label] = v
	lines += ["", f"Keeper share at L{check_level} (need >= {keeper_need}; archetypes within +-{arch_band} of the arm mean {arm_mean:.3f}) and damage per Intercession cast "
		f"(within +-{cast_band} of its Keeper's arm mean; attribution, not the marginal R-KEE-15 reading):", "",
		"| Arm | Runs at L16 | Keeper share at L16 | Pass | Archetype band | Casts/run | Damage/cast | Cast band |", "|---|---|---|---|---|---|---|---|"]
	for label in labels:
		rs = [r for r in runs if r["label"] == label]
		l16 = [r["keeper_share_l16"] for r in rs if r["keeper_share_l16"] != ""]
		share = mean(l16)
		cast = mean(r["cast_damage"] for r in rs)
		keeper_arms = per_keeper.get(label.split("-")[1], {}) if len(label.split("-")) > 1 else {}
		keeper_mean = statistics.mean(keeper_arms.values()) if keeper_arms else 0.0
		lines.append("| {} | {} | {} | {}/{} | {} | {} | {} | {} |".format(label, len(l16), share, sum(x >= keeper_need for x in l16), len(l16),
			"" if share == "" or label not in power else ("IN" if abs(share - arm_mean) <= arch_band else "OUT"),
			mean(r["casts"] for r in rs), cast,
			"" if cast == "" or not keeper_mean else ("IN" if abs(cast / keeper_mean - 1.0) <= cast_band else f"OUT ({cast / keeper_mean:.2f}x)")))
	lines += ["", "Per run: `summary.csv`."]

	verdicts = []
	rc = 0
	if "T-DIR-01" in checks:
		rs = [r for r in runs if r["label"].lower() in ("none", "nopick")]
		if len(rs) < bot_runs:
			verdicts.append(f"T-DIR-01: FAIL (only {len(rs)} no-pick runs, need {bot_runs})")
			rc = 1
		else:
			med = statistics.median(r["end_s"] for r in rs)
			ok = nopick_band[0] <= med <= nopick_band[1]
			alive = sum(r["outcome"] != "death" for r in rs)
			verdicts.append(f"T-DIR-01: {'PASS' if ok else 'FAIL'} (median end {med:.1f} s over {len(rs)} seeds, band {fmt_band(nopick_band)} s"
				+ (f"; {alive} run(s) did not die" if alive else "") + ")")
			rc |= 0 if ok else 1
	if "T-OFF-15" in checks:
		to_boss = consts["RUN.LENGTH_TO_BOSS"]["value"]
		rs = [r for r in runs if r["label"].lower().startswith("steered") and (r["outcome"] == "victory" or r["end_s"] >= to_boss)]
		if not rs:
			verdicts.append("T-OFF-15: n/a (no Steered run reached the boss)")
			rc = 1
		else:
			bad = [r for r in rs if not (per_run - tol <= (r["decisions"] or 0) <= per_run + tol) or (r["reserve_share"] or 0) > reserve_max]
			verdicts.append(f"T-OFF-15: {'FAIL' if bad else 'PASS'} ({len(rs) - len(bad)}/{len(rs)} runs at the boss with decisions "
				f"{per_run}+-{tol} and Reserve share <= {reserve_max}; mean decisions {mean(r['decisions'] for r in rs)})")
			rc |= 1 if bad else 0
	if verdicts:
		lines += ["", "## Checks", ""] + [f"- {v}" for v in verdicts]
	with open(os.path.join(root, "summary.md"), "w", encoding="utf-8") as f:
		f.write("\n".join(lines) + "\n")
	print("\n".join(lines))
	for v in verdicts:
		print(v)
	return rc


if __name__ == "__main__":
	sys.exit(main(sys.argv))
