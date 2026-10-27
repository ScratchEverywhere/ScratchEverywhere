#!/usr/bin/env python3
"""Prepare a separate Gammastr20 Sokoban copy with a solvable tenth puzzle.

Open three floor cells that provide access around two trapped crates. Keep
the level dimensions, player start, crates, goals, scripts and assets intact.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
if args.source.resolve() == args.output.resolve() or args.output.exists():
    parser.error("Choose a new output archive; the original is preserved.")
source_sha = hashlib.sha256(args.source.read_bytes()).hexdigest()
if source_sha != "6b2ea446a14651efe37bc6589172a05309371d1c8db1d9ce8a267848827e5013":
    parser.error("Expected the tested original Gammastr20 Sokoban archive.")

changes = [(12, 3), (12, 4), (7, 5)]
with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    data_lists = [x for x in project.get("lists", []) if x.get("listName") == "Data"]
    if len(data_lists) != 1 or len(data_lists[0]["contents"]) != 13:
        parser.error("Expected the original thirteen-puzzle Data list.")
    levels = data_lists[0]["contents"]
    level = levels[9]
    if level[:8] != "08151300" or len(level[8:]) != 8 * 15:
        parser.error("Unexpected tenth-puzzle dimensions or player start.")
    cells = list(level[8:])
    for x, y in changes:
        index = y * 15 + x
        if cells[index] != "0":
            parser.error("Expected an inaccessible cell at the repair location.")
        cells[index] = "1"
    levels[9] = level[:8] + "".join(cells)

    with zipfile.ZipFile(args.output, "x") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)

print(json.dumps({
    "source": str(args.source), "output": str(args.output),
    "source_sha256": source_sha,
    "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
    "change": "Open three access cells in puzzle ten; preserve crates and goals",
    "floor_cells_zero_based": changes,
}, indent=2))
