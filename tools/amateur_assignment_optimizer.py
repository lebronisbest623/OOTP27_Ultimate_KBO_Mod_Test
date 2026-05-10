#!/usr/bin/env python3
import csv
import sys

from ortools.sat.python import cp_model


def _to_int(row, key, default=0):
    try:
        return int(row.get(key, default) or default)
    except ValueError:
        return default


def _candidate_weight(row, target_player_count):
    player_tier = _to_int(row, "player_tier")
    team_tier = _to_int(row, "team_tier")
    if abs(player_tier - team_tier) > 1:
        return 0

    target_max_players = _to_int(row, "target_max_players")
    player_count = _to_int(row, "player_count")
    if target_max_players > 0 and player_count >= target_max_players:
        return 0
    if target_player_count >= 0 and player_count > target_player_count:
        return 0

    target_rep = _to_int(row, "target_reputation")
    reputation = _to_int(row, "reputation")
    distance = abs(reputation - target_rep)
    base = max(8, 96 - distance * 4)

    tier_delta = team_tier - player_tier
    if tier_delta == 0:
        multiplier = 140
    elif tier_delta > 0:
        multiplier = 65
    else:
        multiplier = 30 if player_tier >= 4 else 45

    weight = max(1, (base * multiplier) // 100)
    if target_max_players > 0 and player_count >= target_max_players - 2:
        weight = max(1, weight // 2)
    return weight


def optimize(request_path, result_path):
    with open(request_path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    if not rows:
        return 2

    current_team_id = _to_int(rows[0], "current_team_id")
    current_reputation = _to_int(rows[0], "current_reputation")
    player_tier = _to_int(rows[0], "player_tier")

    prelim = []
    target_player_count = None
    for row in rows:
        team_id = _to_int(row, "team_id")
        reputation = _to_int(row, "reputation")
        if team_id == 0 or team_id == current_team_id:
            continue
        if _to_int(row, "rejected") != 0:
            continue
        if player_tier > 0 and reputation <= current_reputation:
            continue
        if abs(player_tier - _to_int(row, "team_tier")) > 1:
            continue
        player_count = _to_int(row, "player_count")
        target_max_players = _to_int(row, "target_max_players")
        if target_max_players > 0 and player_count >= target_max_players:
            continue
        if target_player_count is None or player_count < target_player_count:
            target_player_count = player_count
        prelim.append(row)

    if target_player_count is None:
        target_player_count = -1

    candidates = []
    for row in prelim:
        weight = _candidate_weight(row, target_player_count)
        if weight > 0:
            candidates.append((row, weight))

    if not candidates:
        with open(result_path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["target_team_id", "weight", "status"])
            writer.writerow([0, 0, "no_candidate"])
        return 0

    model = cp_model.CpModel()
    variables = [model.NewBoolVar(f"team_{_to_int(row, 'team_id')}") for row, _ in candidates]
    model.Add(sum(variables) == 1)
    model.Maximize(sum(var * weight for var, (_, weight) in zip(variables, candidates)))

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = 0.25
    solver.parameters.num_search_workers = 1
    status = solver.Solve(model)

    best_index = 0
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        for i, var in enumerate(variables):
            if solver.Value(var):
                best_index = i
                break
    else:
        best_index = max(range(len(candidates)), key=lambda i: candidates[i][1])

    row, weight = candidates[best_index]
    with open(result_path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["target_team_id", "weight", "status"])
        writer.writerow([_to_int(row, "team_id"), weight, "ok"])
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: amateur_assignment_optimizer.py REQUEST_CSV RESULT_CSV")
    raise SystemExit(optimize(sys.argv[1], sys.argv[2]))
