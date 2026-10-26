#!/usr/bin/env python3
"""Prepare a separate Maze Starter copy whose win message resets on replay.

Yield once before Goal's collision loop so Ball's Green Flag reset can run.
The original archive, controls, maze, collision rules and assets are preserved.
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
    parser.error("Choose a new output archive; the original is never overwritten.")

with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    targets = {t["name"]: t for t in project.get("targets", [])}
    if not {"Ball", "Goal"}.issubset(targets):
        parser.error("Expected Maze Starter's Ball and Goal targets.")
    blocks = targets["Goal"]["blocks"]
    hats = [(key, block) for key, block in blocks.items()
            if block.get("opcode") == "event_whenflagclicked"]
    if len(hats) != 1:
        parser.error("Expected Goal's single Green Flag collision loop.")
    hat_id, hat = hats[0]
    loop_id = hat.get("next")
    loop = blocks.get(loop_id, {})
    if loop.get("opcode") != "control_forever" or loop.get("parent") != hat_id:
        parser.error("Goal's startup has changed; review this revision manually.")
    ball_blocks = targets["Ball"]["blocks"]
    resets = [ball_blocks.get(b.get("next"), {}) for b in ball_blocks.values()
              if b.get("opcode") == "event_whenflagclicked"]
    if not any(b.get("opcode") == "motion_gotoxy" and
               b.get("inputs") == {"X": [1, [4, "-205"]], "Y": [1, [4, "147"]]}
               for b in resets):
        parser.error("Expected Ball's original Green Flag position reset.")
    wait_id = "xbox-maze-wait-for-reset"
    if wait_id in blocks:
        parser.error("The project already contains the replay preparation.")
    # Goal may run before Ball at the old winning position. A zero-second wait
    # yields one scheduler turn without changing either sprite or game rules.
    blocks[wait_id] = {
        "opcode": "control_wait", "next": loop_id, "parent": hat_id,
        "inputs": {"DURATION": [1, [4, "0"]]}, "fields": {},
        "shadow": False, "topLevel": False,
    }
    hat["next"] = wait_id
    loop["parent"] = wait_id
    with zipfile.ZipFile(args.output, "x") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)

print(json.dumps({
    "source": str(args.source), "output": str(args.output),
    "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
    "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
    "changes": ["Yield one scheduler turn before Goal's collision loop"],
}, indent=2))
