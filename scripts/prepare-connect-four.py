#!/usr/bin/env python3
"""Prepare a separate controller-friendly copy of aaronzoll's SB2 Connect Four.

Initialize its authored Space state and hide the mode monitor covering the win
message. Game rules, assets, credits and controller profiles are preserved.
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
    initializers = []
    mode_monitors = []
    for child in project.get("children", []):
        if child.get("objName") == "mouse":
            for script in child.get("scripts", []):
                blocks = script[2]
                if blocks and blocks[0] == ["whenGreenFlag"]:
                    initializers += [b for b in blocks if b == ["setVar:to:", "space", 0]]
        if child.get("cmd") == "getVar:" and child.get("param") == "mode":
            mode_monitors.append(child)
    if len(initializers) != 1 or len(mode_monitors) != 1:
        parser.error("Expected exactly one Space initializer and one mode monitor; this is a different project revision.")
    initializers[0][2] = "unpressed"
    mode_monitors[0]["visible"] = False
    with zipfile.ZipFile(args.output, "w") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)

print(json.dumps({
    "source": str(args.source), "output": str(args.output),
    "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
    "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
    "changes": ["Initialize space to unpressed", "Hide mode monitor over win message"],
}, indent=2))
