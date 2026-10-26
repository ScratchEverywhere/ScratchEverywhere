"""Generate an SB2 regression for computed list selectors (PASSES=19, ERRORS=0).

Run with bytecode both enabled and disabled. This is not a qualification game.
"""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("SB2-Dynamic-Lists.sb2")
svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/><text transform="translate(20 230)" fill="white" font-size="22">Computed Scratch 2 list selectors</text><text transform="translate(20 275)" fill="white" font-size="18">Expected: PASSES 19 / ERRORS 0</text></svg>'
asset = hashlib.md5(svg).hexdigest()
costume = dict(costumeName="test", baseLayerID=0, baseLayerMD5=asset + ".svg", bitmapResolution=1,
               rotationCenterX=240, rotationCenterY=180)


def name(s):
    return ["concatenate:with:", s, ""]


bucket = ["concatenate:with:", "bucket", ["readVariable", "selector"]]


def check(reporter, expected):
    return ["doIfElse", ["=", reporter, expected],
            [["changeVar:by:", "PASSES", 1]], [["changeVar:by:", "ERRORS", 1]]]


script = [
    ["whenIReceive", "test"], ["setVar:to:", "selector", 1],
    ["deleteLine:ofList:", "all", bucket],
    check(["lineCountOfList:", bucket], 0),
    ["append:toList:", "A", bucket], ["insert:at:ofList:", "B", 1, bucket],
    ["setLine:ofList:to:", 2, bucket, "C"],
    check(["getLine:ofList:", 1, bucket], "B"),
    check(["getLine:ofList:", 2, bucket], "C"),
    check(["lineCountOfList:", bucket], 2),
    check(["contentsOfList:", bucket], "BC"),
    ["doIfElse", ["list:contains:", bucket, "C"], [["changeVar:by:", "PASSES", 1]], [["changeVar:by:", "ERRORS", 1]]],
    ["deleteLine:ofList:", 1, bucket], check(["getLine:ofList:", "last", bucket], "C"),
    ["insert:at:ofList:", "D", "last", bucket], ["setLine:ofList:to:", "last", bucket, "E"],
    check(["contentsOfList:", bucket], "CE"),
    ["deleteLine:ofList:", "last", bucket], check(["lineCountOfList:", bucket], 1),
    ["deleteLine:ofList:", "all", bucket], check(["lineCountOfList:", bucket], 0),
    ["setVar:to:", "selector", 2], ["append:toList:", "other", bucket],
    check(["getLine:ofList:", 1, bucket], "other"),
    check(["lineCountOfList:", name("bucket1")], 0),
    check(["lineCountOfList:", "bucket2"], 1),  # Keep the static bytecode path covered.
    check(["getLine:ofList:", 1, name("shared")], "local"),
    check(["getLine:ofList:", 1, name("stage-only")], "stage"),
    check(["lineCountOfList:", name("missing")], 0),
    check(["getLine:ofList:", 1, name("missing")], ""),
    ["append:toList:", "ignored", name("missing")],
    check(["lineCountOfList:", name("missing")], 0),
    check(["getLine:ofList:", 1, "shared"], "local"),
    ["showList:", name("bucket1")], ["hideList:", name("bucket2")],
]


def lst(label, values):
    return dict(listName=label, contents=values, isPersistent=False, visible=False)


actor = dict(objName="Checker", variables=[], lists=[lst("bucket1", ["stale"]), lst("bucket2", []), lst("shared", ["local"])],
             scripts=[[10, 10, script]], costumes=[costume], sounds=[], currentCostumeIndex=0,
             scratchX=0, scratchY=0, scale=1, direction=90, rotationStyle="normal", isDraggable=False, visible=False)
stage_script = [["whenGreenFlag"], ["setVar:to:", "PASSES", 0], ["setVar:to:", "ERRORS", 0],
                ["doBroadcastAndWait", "test"], ["stopScripts", "all"]]
monitors = [dict(target="Stage", cmd="getVar:", param=n, color=15629590, label=n, mode=1,
                 sliderMin=0, sliderMax=100, isDiscrete=True, x=20, y=20 + i * 40, visible=True)
            for i, n in enumerate(["PASSES", "ERRORS"])]
monitors += [dict(target="Checker", listName=n, contents=[], x=270, y=20 + i * 110,
                  width=160, height=100, visible=bool(i)) for i, n in enumerate(["bucket1", "bucket2"])]
project = dict(objName="Stage", variables=[dict(name=n, value=0, isPersistent=False) for n in ["PASSES", "ERRORS", "selector"]],
               lists=[lst("shared", ["stage-shadow"]), lst("stage-only", ["stage"])], scripts=[[10, 10, stage_script]],
               costumes=[costume], sounds=[], currentCostumeIndex=0, penLayerID=-1, tempoBPM=60, videoAlpha=.5,
               children=[actor] + monitors, info=dict(swfVersion="v461", spriteCount=1, scriptCount=2))
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    archive.writestr("0.svg", svg)
print(out)
