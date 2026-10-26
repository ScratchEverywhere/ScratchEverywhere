#!/usr/bin/env python3
"""Prepare a separate 2048 copy whose victory panel also accepts Space/Xbox A.

The original mouse acknowledgement remains available. Grid, scoring, random
spawns, loss/replay scripts and all assets remain unchanged.
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
source_sha256 = hashlib.sha256(args.source.read_bytes()).hexdigest()
if source_sha256 != "ff11d9adfa7fb1b38cdcef5711132d060dba96d91d371853e854456f00b0607f":
    parser.error("Expected the tested original JereTheJuggler 2048 archive.")

with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    panel = [s for s in project.get("children", []) if s.get("objName") == "Sprite2"]
    hats = [s[2] for p in panel for s in p.get("scripts", [])
            if s[2] and s[2][0] == ["whenIReceive", "you win"]]
    wait = ["doWaitUntil", ["mousePressed"]]
    if len(hats) != 1 or hats[0].count(wait) != 1:
        parser.error("Expected the original mouse-only victory acknowledgement.")
    index = hats[0].index(wait)
    hats[0][index] = ["doWaitUntil", ["|", ["mousePressed"], ["keyPressed:", "space"]]]
    with zipfile.ZipFile(args.output, "x") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)

print(json.dumps({
    "source": str(args.source), "output": str(args.output),
    "source_sha256": source_sha256,
    "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
    "change": "Victory acknowledgement accepts mouse or Space (Xbox A)",
}, indent=2))
