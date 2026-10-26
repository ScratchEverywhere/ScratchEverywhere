"""Create an SB2 fixture for localized stage and missing monitor targets.

Global Score=17, Local: LocalScore=99, and Frames must keep advancing.
A increments global Score. B increments LocalScore. The nonexistent target
must not crash the frame loop. Stage/local list monitors must retain scope.
"""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

out = Path(sys.argv[1])
stage_name = sys.argv[2] if len(sys.argv) > 2 else "Bühne"
svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/></svg>'
costume = dict(costumeName="test", baseLayerID=0,
               baseLayerMD5=hashlib.md5(svg).hexdigest() + ".svg",
               bitmapResolution=1, rotationCenterX=240, rotationCenterY=180)
variable = lambda name, value: dict(name=name, value=value, isPersistent=False)
local = dict(objName="Local", variables=[variable("LocalScore", 99)],
             lists=[dict(listName="LocalList", contents=["local"])],
             scripts=[[10, 10, [["whenKeyPressed", "b"], ["changeVar:by:", "LocalScore", 1]]]],
             costumes=[costume], currentCostumeIndex=0, sounds=[], visible=False,
             scratchX=0, scratchY=0, scale=1, direction=90, rotationStyle="normal")
watchers = []
for i, (target, cmd, param) in enumerate([
    (stage_name, "getVar:", "Score"),
    (stage_name, "getVar:", "Frames"),
    ("Local", "getVar:", "LocalScore"),
    (stage_name, "contentsOfList:", "GlobalList"),
    ("Local", "contentsOfList:", "LocalList"),
    ("Missing sprite", "getVar:", "MissingScore"),
]):
    watchers.append(dict(target=target, cmd=cmd, param=param, mode=1, visible=True,
                         color=15629590, label=param, x=15, y=15 + i * 40,
                         sliderMin=0, sliderMax=100, isDiscrete=True))
project = dict(objName=stage_name,
    variables=[variable("Score", 17), variable("Frames", 0)],
    lists=[dict(listName="GlobalList", contents=["red", "blue"])],
    scripts=[[10, 10, [["whenGreenFlag"], ["doForever", [
        ["changeVar:by:", "Frames", 1], ["wait:elapsed:from:", .1]]]]],
             [10, 100, [["whenKeyPressed", "space"], ["changeVar:by:", "Score", 1]]]],
    costumes=[costume], currentCostumeIndex=0, sounds=[], tempoBPM=60,
    children=[local] + watchers)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    archive.writestr("0.svg", svg)
Path(str(out) + ".json").write_text(json.dumps({"controls": {"space": "A", "b": "B", "1": "start"}}, indent=2) + "\n")
print(out)
