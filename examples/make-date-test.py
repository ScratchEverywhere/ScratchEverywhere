"""Build a one-shot calendar reporter fixture; compare it with the console clock.

All seven local calendar fields and UTC days since 2000 are read at the green
flag. This is a runtime regression fixture, not a qualification game.
"""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("Date-Test.sb3")
fields = ["YEAR", "MONTH", "DATE", "DAYOFWEEK", "HOUR", "MINUTE", "SECOND", "UTC DAYS"]
blocks = {}


def block(opcode, parent=None, next=None, inputs=None, fields=None):
    return dict(opcode=opcode, parent=parent, next=next, inputs=inputs or {},
                fields=fields or {}, shadow=False, topLevel=False)


blocks["flag"] = block("event_whenflagclicked", next="set0")
blocks["flag"].update(topLevel=True, x=20, y=20)
for index, name in enumerate(fields):
    setter, reporter = f"set{index}", f"read{index}"
    blocks[setter] = block("data_setvariableto", parent="flag" if index == 0 else f"set{index - 1}",
                           next=f"set{index + 1}" if index + 1 < len(fields) else None,
                           inputs={"VALUE": [2, reporter]}, fields={"VARIABLE": [name, name]})
    blocks[reporter] = block("sensing_dayssince2000" if name == "UTC DAYS" else "sensing_current",
                             parent=setter, fields={} if name == "UTC DAYS" else {"CURRENTMENU": [name, None]})

svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/><text x="20" y="310" fill="white" font-size="18">Local calendar / UTC elapsed days</text><text x="20" y="340" fill="white" font-size="15">Green Flag reads the clock again.</text></svg>'
asset = hashlib.md5(svg).hexdigest()
costume = dict(name="Calendar", assetId=asset, md5ext=asset + ".svg", dataFormat="svg", rotationCenterX=240, rotationCenterY=180)
stage = dict(isStage=True, name="Stage", variables={name: [name, -1] for name in fields}, lists={}, broadcasts={},
             blocks=blocks, comments={}, currentCostume=0, costumes=[costume], sounds=[], volume=100, layerOrder=0,
             tempo=60, videoTransparency=50, videoState="on", textToSpeechLanguage=None)
monitors = [dict(id=name, mode="default", opcode="data_variable", params={"VARIABLE": name}, spriteName=None,
                 value=-1, width=0, height=0, x=20, y=15 + 33 * index, visible=True,
                 sliderMin=0, sliderMax=100, isDiscrete=False) for index, name in enumerate(fields)]
project = dict(targets=[stage], monitors=monitors, extensions=[],
               meta={"semver": "3.0.0", "vm": "1.1.2", "agent": "Calendar regression"})
with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    archive.writestr(asset + ".svg", svg)
print(output)
