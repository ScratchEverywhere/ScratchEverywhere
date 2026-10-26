"""Prepare a separate LVRS Breakout copy with direct controller paddle steering.

Usage: prepare-breakout.py ORIGINAL.sb2 NEW.sb2
Mouse steering remains active until the first Left/Right press. Green Flag
restores mouse mode. Only paddle input/bounds change; assets and ball, scoring,
lives and ending scripts are preserved.
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
profile = Path(str(args.output) + ".json")
if args.source.resolve() == args.output.resolve() or args.output.exists() or profile.exists():
    parser.error("Choose new output paths; the original archive is preserved.")

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
