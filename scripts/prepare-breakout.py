"""Prepare a separate LVRS Breakout copy with controller steering and brick erasure.

Usage: prepare-breakout.py ORIGINAL.sb2 NEW.sb2
Mouse steering remains active until the first Left/Right press. Green Flag
restores mouse mode. Erase only brick cells touched by a real ball collision,
including shallow-angle hits. Keep the original assets, ball speed, score
increments, lives and ending behavior. Requires pen-aware color sensing.
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
profile = Path(str(args.output) + ".json")
if args.source.resolve() == args.output.resolve() or args.output.exists() or profile.exists():
    parser.error("Choose new output paths; the original archive is preserved.")
source_sha = hashlib.sha256(args.source.read_bytes()).hexdigest()
if source_sha != "944130addd4e809eeb992d6e4a5d6e9619d5d9c11af7026b238a348fd4a03c1d":
    parser.error("Expected the tested original LVRS Atari Breakout archive.")

with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    paddles = [c for c in project.get("children", []) if c.get("objName") == "Paddle"]
    if len(paddles) != 1:
        parser.error("Expected one Paddle sprite.")
    paddle = paddles[0]
    starts = [s[2] for s in paddle.get("scripts", []) if s[2][0] == ["whenIReceive", "Start"]]
    if len(starts) != 1 or starts[0][-1] != ["doForever", [["comeToFront"], ["xpos:", ["mouseX"]]]]:
        parser.error("Expected the original mouse-only paddle loop.")
    costumes = paddle.get("costumes", [])
    if len(costumes) != 1 or costumes[0].get("bitmapResolution") != 2 or costumes[0].get("rotationCenterX") != 78:
        parser.error("Expected the original 78-unit-wide paddle.")
    mode = "_xboxPaddleKeys"
    if any(v["name"] == mode for v in project.get("variables", [])):
        parser.error("This project already contains controller preparation.")
    project["variables"].append(dict(name=mode, value=0, isPersistent=False))
    scripts = project.setdefault("scripts", [])
    scripts.append([10, 10, [["whenGreenFlag"], ["setVar:to:", mode, 0]]])
    for i, key in enumerate(["left arrow", "right arrow"]):
        scripts.append([10, 100 + i * 80, [["whenKeyPressed", key], ["setVar:to:", mode, 1]]])
    starts[0][-1][1][1] = ["doIfElse", ["=", ["readVariable", mode], 0],
        [["xpos:", ["mouseX"]]], [
            ["changeXposBy:", ["*", 8, ["-", ["keyPressed:", "right arrow"], ["keyPressed:", "left arrow"]]]],
            ["doIf", [">", ["xpos"], 201], [["xpos:", 201]]],
            ["doIf", ["<", ["xpos"], -201], [["xpos:", -201]]],
        ]]

    # The original eraser rounds the ball center to a 22-unit cell. An 8-unit
    # ball can hit a row while its center still rounds to the row below it.
    # Probe the original brick cells at the contact instead of widening the
    # eraser or changing the ball, scoring or life rules.
    ball = next(c for c in project["children"] if c.get("objName") == "Ball")
    eraser = next(c for c in project["children"] if c.get("objName") == "MarkOut")
    ball_starts = [s[2] for s in ball["scripts"] if s[2][0] == ["whenIReceive", "Start"]]
    movement = next(s[-1][1] for s in ball_starts if s[1] == ["wait:elapsed:from:", 1])
    collision = next(s[-1][1][-1] for s in ball_starts if s[1] == ["show"])
    if movement[0] != ["forward:", 10] or collision[0] != "doIf" or collision[2] != [
        ["wait:elapsed:from:", 0.001], ["changeVar:by:", "Score", 20],
        ["heading:", ["-", 180, ["heading"]]], ["wait:elapsed:from:", 0.5],
    ]:
        parser.error("Unexpected original ball movement or brick-collision script.")
    pending = "_xboxErasePending"
    event = "_xboxEraseBrickHit"
    project["variables"].append(dict(name=pending, value=0, isPersistent=False))
    reset = next(s[2] for s in scripts if s[2][0] == ["whenGreenFlag"])
    reset.append(["setVar:to:", pending, 0])
    original_movement = copy.deepcopy(movement)
    movement[:] = [["doIf", ["=", ["readVariable", pending], 0], original_movement]]
    collision[2][:0] = [["setVar:to:", pending, 1], ["doBroadcastAndWait", event], ["setVar:to:", pending, 0]]
    eraser_start = next(s[2] for s in eraser["scripts"] if s[2][0] == ["whenIReceive", "Start"])
    eraser_start[:] = [["whenIReceive", "Start"], ["hide"]]
    base_x, column = "_xboxBrickBaseX", "_xboxBrickColumn"
    eraser.setdefault("variables", []).extend(dict(name=n, value=0, isPersistent=False) for n in [base_x, column])
    # Touching-sprite reporters deliberately ignore hidden sprites. Make the
    # probe visible only inside this warp procedure; hide it before any frame
    # is drawn, so it remains an invisible eraser during ordinary play.
    erase = [["procDef", "Erase touched bricks", [], [], True], ["show"],
             ["setVar:to:", base_x, ["*", ["rounded", ["/", ["getAttribute:of:", "x position", "Ball"], 22]], 22]]]
    for row_y, color in [(66, -16758074), (88, -15690752), (110, -1587712), (132, -36047), (154, -46526)]:
        erase.extend([
            ["setVar:to:", column, ["-", ["readVariable", base_x], 22]],
            ["doRepeat", 3, [
                ["gotoX:y:", ["readVariable", column], row_y],
                ["doIf", ["&", ["touching:", "Ball"], ["touchingColor:", color]], [["stampCostume"]]],
                ["changeVar:by:", column, 22],
            ]],
        ])
    erase.append(["hide"])
    eraser["scripts"].extend([[400, 100, [["whenIReceive", event], ["call", "Erase touched bricks"]]], [400, 300, erase]])
    with zipfile.ZipFile(args.output, "w") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)

profile.write_text(json.dumps({"controls": {
    "left arrow": ["dpadLeft", "LeftStickLeft"],
    "right arrow": ["dpadRight", "LeftStickRight"],
    "up arrow": ["dpadUp", "LeftStickUp"],
    "down arrow": ["dpadDown", "LeftStickDown"],
    "space": "A", "b": "B", "y": "Y", "l": "shoulderL", "r": "shoulderR",
    "1": "start", "0": "back",
}}, indent=2) + "\n")
print(json.dumps({"source": str(args.source), "output": str(args.output),
                  "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
                  "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
                  "profile_sha256": hashlib.sha256(profile.read_bytes()).hexdigest()}, indent=2))
