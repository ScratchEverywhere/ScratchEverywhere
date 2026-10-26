"""Create an Xbox pause-input SB2 fixture and its adjacent controller profile.

Menu navigation, held A/X Green Flag confirmation, and B/Y resume must not
increment game events. A fresh A press during play must increment SPACE.
FLAGS counts actual Green Flag executions and persists across replay.
"""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("Pause-Input.sb2")
svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/><text transform="translate(18 310)" fill="white" font-size="19">Menu input must leave all event counts at zero</text></svg>'
names = ["FLAGS", "SPACE", "A", "B", "X", "Y", "NAV"]
costume = dict(costumeName="test", baseLayerID=0, baseLayerMD5=hashlib.md5(svg).hexdigest() + ".svg",
               bitmapResolution=1, rotationCenterX=240, rotationCenterY=180)
scripts = [[["whenGreenFlag"], ["changeVar:by:", "FLAGS", 1]]]
for key, name in [("space", "SPACE"), ("a", "A"), ("b", "B"), ("x", "X"), ("y", "Y"), ("down arrow", "NAV")]:
    scripts.append([["whenKeyPressed", key], ["changeVar:by:", name, 1]])
monitors = [dict(target="Stage", cmd="getVar:", param=n, color=15629590, label=n, mode=1,
                 sliderMin=0, sliderMax=100, isDiscrete=True, x=20, y=15 + i * 36, visible=True)
            for i, n in enumerate(names)]
project = dict(objName="Stage", variables=[dict(name=n, value=0, isPersistent=False) for n in names],
               lists=[], scripts=[[10, 10, s] for s in scripts], costumes=[costume], sounds=[],
               currentCostumeIndex=0, penLayerID=-1, tempoBPM=60, videoAlpha=.5,
               children=monitors, info=dict(swfVersion="v461", spriteCount=0, scriptCount=len(scripts)))
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    archive.writestr("0.svg", svg)
profile = {"controls": {"space": "A", "b": "B", "a": "X", "x": "X", "y": "Y",
                        "up arrow": ["dpadUp", "LeftStickUp"],
                        "down arrow": ["dpadDown", "LeftStickDown"],
                        "left arrow": ["dpadLeft", "LeftStickLeft"],
                        "right arrow": ["dpadRight", "LeftStickRight"]}}
Path(str(out) + ".json").write_text(json.dumps(profile, indent=2) + "\n")
print(out)
