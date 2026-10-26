#!/usr/bin/env python3
"""Prepare a separate cs50 Frogger copy with a prompt, stable seven-pad win.

Move the win broadcast out of the music wait, stop pad collision loops after
the win, and initialize shared state before starting sprite scripts on replay.
Assets, movement, scoring and occupied-pad deaths before seven remain unchanged.
The original archive is never overwritten.
"""
import argparse
import copy
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
if source_sha256 != "6df88eff3a23796d3abc6ebde5cf7ec9b1df611ff6937c093a6cc59eb048a662":
    parser.error("Expected the tested original cs50 Frogger archive; refusing another revision.")

won = [">", ["readVariable", "score"], "6"]
stop = ["stopScripts", "this script"]
with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    music = [s[2] for s in project.get("scripts", [])
             if s[2] and s[2][0] == ["whenGreenFlag"]]
    if len(music) != 1 or music[0][-1][0] != "doForever":
        parser.error("Expected Frogger's single stage music initializer.")
    loop = music[0][-1][1]
    old_win = ["doIf", won, [["broadcast:", "win"], stop]]
    if not loop or loop[0] != ["doPlaySoundAndWait", "Frogger music - "] or old_win not in loop:
        parser.error("Expected the original win broadcast after the music wait.")
    if ["setVar:to:", "score", "0"] not in music[0]:
        parser.error("Expected the original Green Flag score reset.")
    pads = {s.get("objName"): s for s in project.get("children", [])
            if s.get("objName", "").startswith("lilypad")}
    if set(pads) != {"lilypad" + str(i) for i in range(1, 8)}:
        parser.error("Expected all seven original lily pads.")
    bodies = []
    for name, pad in pads.items():
        hats = [s[2] for s in pad.get("scripts", [])
                if s[2] and s[2][0] == ["whenGreenFlag"]]
        if len(hats) != 1 or hats[0][-1][0] != "doForever":
            parser.error("Expected " + name + "'s original collision loop.")
        body = hats[0][-1][1]
        occupied = ["&", ["touching:", "frog"], ["=", ["costumeIndex"], "2"]]
        if len(body) != 2 or body[0][:2] != ["doIf", occupied] or body[1][:2] != ["doIf", ["touching:", "frog"]]:
            parser.error("Unexpected scoring/collision logic in " + name + ".")
        if ["changeVar:by:", "score", 1] not in body[1][2]:
            parser.error("Expected one-point pad scoring in " + name + ".")
        bodies.append(body)

    # Win independently of song completion. Yield once so the existing stage
    # initializer resets an old score of seven before this new hat checks it.
    loop[loop.index(old_win)][2] = [copy.deepcopy(stop)]
    project["scripts"].append([20, 700, [
        ["whenGreenFlag"], ["wait:elapsed:from:", 0],
        ["doWaitUntil", copy.deepcopy(won)], ["broadcast:", "win"],
        copy.deepcopy(stop),
    ]])
    for body in bodies:
        # The seventh pad retains the frog. Rechecking its occupied costume
        # after the one-second wait must not turn a completed game into death.
        body.insert(0, ["doIf", copy.deepcopy(won), [copy.deepcopy(stop)]])

    # Green Flag hats run concurrently. After a win or death, sprite loops can
    # see the previous score/alive state and stop before the stage resets it.
    # Start the original sprite initializers through an explicit stage message
    # only after its original shared-state resets have executed.
    ready = "xbox_frogger_ready"
    sprite_hats = 0
    for sprite in project["children"]:
        for script in sprite.get("scripts", []):
            blocks = script[2]
            if blocks and blocks[0] == ["whenGreenFlag"]:
                blocks[0] = ["whenIReceive", ready]
                sprite_hats += 1
    music[0].insert(-1, ["broadcast:", ready])

    with zipfile.ZipFile(args.output, "x") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)

print(json.dumps({
    "source": str(args.source), "output": str(args.output),
    "source_sha256": source_sha256,
    "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
    "sprite_initializers": sprite_hats,
    "changes": ["Broadcast win independently of music completion", "Stop pad loops after seven points", "Reset shared state before sprite initialization"],
}, indent=2))
