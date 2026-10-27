#!/usr/bin/env python3
"""Replay a Sokoban solution independently of the Xbox runtime and repair tool."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("archive", type=Path)
parser.add_argument("solution", type=Path)
args = parser.parse_args()
solution = json.loads(args.solution.read_text())
archive_sha = hashlib.sha256(args.archive.read_bytes()).hexdigest()
if archive_sha not in (solution["original_sha256"], solution["prepared_sha256"]):
    parser.error("The solution describes a different archive revision.")
with zipfile.ZipFile(args.archive) as archive:
    project = json.loads(archive.read("project.json"))
levels = next(x["contents"] for x in project["lists"] if x["listName"] == "Data")
raw = levels[solution["level"] - 1]
height, width, player_x, player_y = [int(raw[i:i + 2]) for i in range(0, 8, 2)]
if len(raw[8:]) != width * height:
    parser.error("The puzzle data does not match its dimensions.")
cells = list(map(int, raw[8:]))
floor = {(i % width, i // width) for i, tile in enumerate(cells) if tile != 0}
goals = {(i % width, i // width) for i, tile in enumerate(cells) if tile in (3, 4)}
crates = {(i % width, i // width) for i, tile in enumerate(cells) if tile in (2, 4)}
player = (player_x, player_y)
if player not in floor or player in crates or len(crates) != len(goals):
    parser.error("Invalid starting player, crates or goals.")
directions = {"U": (0, -1), "D": (0, 1), "L": (-1, 0), "R": (1, 0)}
pushes = 0
for step, direction in enumerate(solution["directions"], 1):
    if crates == goals:
        parser.error(f"Step {step} occurs after puzzle completion.")
    if direction not in directions:
        parser.error("Unknown solution direction.")
    dx, dy = directions[direction]
    destination = (player[0] + dx, player[1] + dy)
    if destination not in floor:
        parser.error(f"Step {step} ({direction}) walks into an inaccessible cell.")
    if destination in crates:
        beyond = (destination[0] + dx, destination[1] + dy)
        if beyond not in floor or beyond in crates:
            parser.error(f"Step {step} ({direction}) pushes a crate into an inaccessible cell or another crate.")
        crates.remove(destination)
        crates.add(beyond)
        pushes += 1
    player = destination
if crates != goals or pushes != solution["pushes"]:
    parser.error("The route did not fill every goal with the expected pushes.")
print(json.dumps({"archive_sha256": archive_sha, "level": solution["level"],
                  "moves": len(solution["directions"]), "pushes": pushes,
                  "goals_filled": len(goals), "verified_solution": True}, indent=2))
