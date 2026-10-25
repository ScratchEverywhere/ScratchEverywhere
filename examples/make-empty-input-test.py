"""Make a Scratch integration regression for missing Boolean/number inputs.

Expected final monitors: PASSES=3, ERRORS=0, EMPTY ADD=7.
Run the project in the actual runtime; it is not one of the ten qualification games.
"""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("Empty-Inputs-Test.sb3")
svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/><text x="30" y="150" fill="white" font-size="24">Empty input regression</text><text x="30" y="200" fill="white" font-size="18">Expected: PASSES 3, ERRORS 0, EMPTY ADD 7</text></svg>'
asset = hashlib.md5(svg).hexdigest()
costume = dict(name="test", assetId=asset, md5ext=asset + ".svg", dataFormat="svg", rotationCenterX=240, rotationCenterY=180)
num = lambda n: [1, [4, str(n)]]
var = lambda name: [3, [12, name, name]]


def block(op, nxt=None, parent=None, inputs=None, fields=None, top=False):
    result = dict(opcode=op, next=nxt, parent=parent, inputs=inputs or {}, fields=fields or {}, shadow=False, topLevel=top)
    if top:
        result.update(x=20, y=20)
    return result


def change(name, amount, parent):
    return block("data_changevariableby", parent=parent, inputs={"VALUE": num(amount)}, fields={"VARIABLE": [name, name]})


blocks = {
    "flag": block("event_whenflagclicked", "reset-passes", top=True),
    "reset-passes": block("data_setvariableto", "reset-errors", "flag", {"VALUE": num(0)}, {"VARIABLE": ["PASSES", "PASSES"]}),
    "reset-errors": block("data_setvariableto", "true-if", "reset-passes", {"VALUE": num(0)}, {"VARIABLE": ["ERRORS", "ERRORS"]}),
    # Seed the condition stack slot with true before reading an empty socket.
    "true-if": block("control_if", "empty-if", "reset-errors", {"CONDITION": [2, "true"], "SUBSTACK": [2, "pass1"]}),
    "true": block("operator_equals", parent="true-if", inputs={"OPERAND1": num(1), "OPERAND2": num(1)}),
    "pass1": change("PASSES", 1, "true-if"),
    "empty-if": block("control_if", "empty-else", "true-if", {"SUBSTACK": [2, "error1"]}),
    "error1": change("ERRORS", 1, "empty-if"),
    "empty-else": block("control_if_else", "seed-number", "empty-if", {"SUBSTACK": [2, "error2"], "SUBSTACK2": [2, "pass2"]}),
    "error2": change("ERRORS", 10, "empty-else"),
    "pass2": change("PASSES", 1, "empty-else"),
    "seed-number": block("data_setvariableto", "empty-add", "empty-else", {"VALUE": [2, "full-add"]}, {"VARIABLE": ["EMPTY ADD", "EMPTY ADD"]}),
    "full-add": block("operator_add", parent="seed-number", inputs={"NUM1": num(7), "NUM2": num(9)}),
    "empty-add": block("data_setvariableto", "check-number", "seed-number", {"VALUE": [2, "missing-add"]}, {"VARIABLE": ["EMPTY ADD", "EMPTY ADD"]}),
    "missing-add": block("operator_add", parent="empty-add", inputs={"NUM1": num(7)}),
    "check-number": block("control_if_else", "stop", "empty-add", {"CONDITION": [2, "sum-is-seven"], "SUBSTACK": [2, "pass3"], "SUBSTACK2": [2, "error3"]}),
    "sum-is-seven": block("operator_equals", parent="check-number", inputs={"OPERAND1": var("EMPTY ADD"), "OPERAND2": num(7)}),
    "pass3": change("PASSES", 1, "check-number"),
    "error3": change("ERRORS", 100, "check-number"),
    "stop": block("control_stop", parent="check-number", fields={"STOP_OPTION": ["all", None]}),
}
stage = dict(isStage=True, name="Stage", variables={name: [name, 0] for name in ["PASSES", "ERRORS", "EMPTY ADD"]}, lists={}, broadcasts={}, blocks=blocks, comments={}, currentCostume=0, costumes=[costume], sounds=[], volume=100, layerOrder=0, tempo=60, videoTransparency=50, videoState="on", textToSpeechLanguage=None)
monitors = [dict(id=name, mode="default", opcode="data_variable", params={"VARIABLE": name}, spriteName=None, value=0, width=0, height=0, x=20, y=20 + 40 * index, visible=True, sliderMin=0, sliderMax=100, isDiscrete=True) for index, name in enumerate(["PASSES", "ERRORS", "EMPTY ADD"])]
project = dict(targets=[stage], monitors=monitors, extensions=[], meta={"semver": "3.0.0", "vm": "1.1.2", "agent": "Empty input regression"})
with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    archive.writestr(asset + ".svg", svg)
print(output)
