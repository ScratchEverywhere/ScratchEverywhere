"""Create a separate Xbox-friendly Jumper copy with A to play and jump.

Usage: prepare-jumper.py ORIGINAL.sb2 ORIGINAL.sb2.json NEW.sb2
The new archive and adjacent profile must not already exist. Gameplay rules,
assets and credits are preserved. The menu-start key waits for the authored
entrance animation to finish before broadcasting Start.
Black retains R (restart level); Y+Black supplies G+R (restart game).
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("profile", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
profile_out = Path(str(args.output) + ".json")
if args.output.exists() or profile_out.exists() or args.source.resolve() == args.output.resolve():
    parser.error("Choose new output paths; the original archive and profile are preserved.")

with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    children = {child.get("objName"): child for child in project.get("children", []) if child.get("objName")}
    expected = {"Play", "Player", "Line", "Platform"}
    variables = {v["name"] for v in project.get("variables", [])}
    if not expected.issubset(children) or not {"Start", "Highscore?!"}.issubset(variables):
        parser.error("This is not the expected Jumper project.")
    if any(script[2] and script[2][0][0] == "whenKeyPressed" for script in project.get("scripts", [])):
        parser.error("The stage already has keyboard hats; review this project revision manually.")
    ready = "_xboxMenuReady"
    if ready in variables:
        parser.error("The project already contains the Xbox menu-ready variable.")
    play_back = [s[2] for s in children["Play"]["scripts"] if s[2][0] == ["whenIReceive", "Back"]]
    if len(play_back) != 1 or play_back[0][4][:2] != ["doRepeat", 25]:
        parser.error("Expected Play's original 25-frame entrance animation.")
    project["variables"].append({"name": ready, "value": 0, "isPersistent": False})
    # A controller can be pressed before the mouse-only Play button becomes
    # clickable. Buffer that press until the original entrance has completed;
    # broadcasting Start earlier lets the menu animation race gameplay.
    play_back[0].insert(5, ["setVar:to:", ready, 1])
    play_back[0].insert(1, ["setVar:to:", ready, 0])
    project["scripts"].append([10, 450, [["whenGreenFlag"], ["setVar:to:", ready, 0]]])
    for message in ("Start", "player color"):
        project["scripts"].append([10, 475, [["whenIReceive", message], ["setVar:to:", ready, 0]]])
    menu = ["&", ["=", ["readVariable", "Start"], 0],
                 ["=", ["readVariable", "Highscore?!"], 0]]
    is_ready = ["=", ["readVariable", ready], 1]
    # The player's ground-jump script already reads Up. Clear readiness before
    # broadcasting so another held-key event cannot start the game twice.
    project["scripts"].append([10, 500, [
        ["whenKeyPressed", "up arrow"],
        ["doIf", menu, [
            ["doWaitUntil", ["|", is_ready, ["not", menu]]],
            ["doIf", ["&", menu, is_ready], [
                ["setVar:to:", ready, 0], ["broadcast:", "Start"]]],
        ]],
    ]])
    profile = json.loads(args.profile.read_text())
    controls = profile["controls"]
    if controls.get("space") != "A":
        parser.error("Expected the original A-to-Space profile.")
    up = controls.get("up arrow")
    if not isinstance(up, list) or "A" in up:
        parser.error("Expected the original D-pad/stick Up binding array.")
    if controls.get("y") != "Y" or "g" in controls:
        parser.error("Expected the original unused Y binding for the G+R shortcut.")
    del controls["space"]
    controls["up arrow"] = up + ["A"]
    del controls["y"]
    controls["g"] = "Y"
    with zipfile.ZipFile(args.output, "w") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)
profile_out.write_text(json.dumps(profile, indent=2) + "\n")
print(json.dumps({"source": str(args.source), "output": str(args.output),
                  "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
                  "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
                  "profile_sha256": hashlib.sha256(profile_out.read_bytes()).hexdigest()}, indent=2))
