#!/usr/bin/env python3
"""Prepare a separate Will_Wam Mario copy with ordered death/restart handling.

The original starts a frame-based death animation alongside a timed sound,
then restarts after the sound. On a slower machine the old animation can
continue changing/hiding Mario in the new round. Wait for both to finish,
and re-enable hits only after the level initializer restores Mario.
Movement, collision rules, sounds, assets and credits are preserved.
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
if args.output.exists() or args.source.resolve() == args.output.resolve():
    parser.error("Choose a new output archive; the original is preserved.")
source_sha = hashlib.sha256(args.source.read_bytes()).hexdigest()
if source_sha != "93a957be2bcffda5e33df1c9e3af36ccfcad6ee37986955ab9df39378f08013d":
    parser.error("Expected the tested original Will_Wam Mario archive.")

with zipfile.ZipFile(args.source) as original:
    project = json.loads(original.read("project.json"))
    mario = next(t for t in project["targets"] if t["name"] == "Mario")
    blocks = mario["blocks"]
    flag_id = "xbox-mario-death-animation-done"
    if flag_id in mario["variables"] or any(k.startswith("xbox-mario-") for k in blocks):
        parser.error("The project already contains the death-animation repair.")
    death_end = "M{BOz!!-)sDE,43LfbaN"
    broadcast = "]YBjGFjpmU2@~b~89~93"
    restart = "j}{,G)YESB0L=?h-J)aW"
    if (blocks[death_end]["opcode"] != "looks_hide" or blocks[death_end]["next"] is not None
            or blocks[broadcast]["opcode"] != "event_broadcast"
            or blocks[broadcast]["inputs"]["BROADCAST_INPUT"][1][1] != "Ya just died boi"
            or blocks[restart]["inputs"]["BROADCAST_INPUT"][1][1] != "Restart Level"):
        parser.error("Unexpected death/restart block structure.")
    mario["variables"][flag_id] = ["_xboxDeathAnimationDone", 1]

    def block(opcode, parent, next_id=None):
        return dict(opcode=opcode, next=next_id, parent=parent, inputs={},
                    fields={}, shadow=False, topLevel=False)

    def splice_before(target, new_id, item):
        parent = blocks[target]["parent"]
        if blocks[parent]["next"] != target:
            parser.error("Expected a sequential block before " + target)
        blocks[parent]["next"] = new_id
        item["parent"] = parent
        item["next"] = target
        blocks[target]["parent"] = new_id
        blocks[new_id] = item

    reset_id = "xbox-mario-death-reset"
    item = block("data_setvariableto", None)
    item["fields"] = {"VARIABLE": ["_xboxDeathAnimationDone", flag_id]}
    item["inputs"] = {"VALUE": [1, [4, 0]]}
    splice_before(broadcast, reset_id, item)

    done_id = "xbox-mario-death-finished"
    item = block("data_setvariableto", death_end)
    item["fields"] = {"VARIABLE": ["_xboxDeathAnimationDone", flag_id]}
    item["inputs"] = {"VALUE": [1, [4, 1]]}
    blocks[death_end]["next"] = done_id
    blocks[done_id] = item

    wait_id = "xbox-mario-death-wait"
    condition_id = "xbox-mario-death-complete"
    item = block("control_wait_until", None)
    item["inputs"] = {"CONDITION": [2, condition_id]}
    splice_before(restart, wait_id, item)
    condition = block("operator_equals", wait_id)
    condition["inputs"] = {"OPERAND1": [3, [12, "_xboxDeathAnimationDone", flag_id], [10, ""]],
                           "OPERAND2": [1, [4, 1]]}
    blocks[condition_id] = condition

    # Restart hats and the hit sender previously cleared CANNOT DIE after a
    # fixed 0.2s. A slow level upload can still have Mario below the floor then,
    # causing another death before Official Start restores his position.
    # The final initializer block is the actual readiness boundary.
    tail = ["lCbsF+tMLZoMh+w|=TE8", "a).JMaw0q2cOfl;y4^Xo", "lY[X^L={}MZ6lyMn2~L$"]
    if blocks[restart]["next"] != tail[0]:
        parser.error("Unexpected post-restart invulnerability reset.")
    blocks[restart]["next"] = None
    for key in tail:
        del blocks[key]
    old_reset_hat = ["!/+WH4tFMB5.L8c]vW9_", "EE,VS.fAj_a?ZdY2;N+:", "ax_wo%m~.-7$R}%sb!)3"]
    if blocks[old_reset_hat[0]]["fields"]["BROADCAST_OPTION"][0] != "restart level":
        parser.error("Unexpected timed restart reset hat.")
    for key in old_reset_hat:
        del blocks[key]
    init_end = "P8]2PRg?qLwJ6Ug-u;Z4"
    if blocks[init_end]["opcode"] != "motion_changexby" or blocks[init_end]["next"] is not None:
        parser.error("Unexpected end of Official Start initialization.")
    ready_id = "xbox-mario-restart-ready"
    ready = block("data_setvariableto", init_end)
    ready["fields"] = {"VARIABLE": ["CANNOT DIE", "m~*aELjB+F1gM6S~A9R,"]}
    ready["inputs"] = {"VALUE": [1, [4, 0]]}
    blocks[init_end]["next"] = ready_id
    blocks[ready_id] = ready

    # Physics can queue Get Hit while the initializer still has the old death
    # position. Checking CANNOT DIE later in that queued handler is too late.
    # Keep movement/collision scripts frozen until every position/reset block
    # above has executed, then release both readiness guards together.
    unfreeze = "hT#YeZkh({)X=xIpA:Bl"
    before = blocks[unfreeze]["parent"]
    after = blocks[unfreeze]["next"]
    if blocks[unfreeze]["fields"]["VARIABLE"][0] != "Movements Frozen":
        parser.error("Unexpected Official Start movement guard.")
    blocks[before]["next"] = after
    blocks[after]["parent"] = before
    blocks[ready_id]["next"] = unfreeze
    blocks[unfreeze]["parent"] = ready_id
    blocks[unfreeze]["next"] = None

    # Stage music cleanup and Mario's death animation start together. The
    # original 0.04s wait does not guarantee Stop All Sounds has executed on
    # a slower scheduler. Signal actual cleanup completion before the cue.
    stage = next(t for t in project["targets"] if t["isStage"])
    audio_id = "xbox-mario-death-audio-ready"
    stage["variables"][audio_id] = ["_xboxDeathAudioReady", 0]
    stop_audio = "gBlo7S:J%Ut}C{[H%V1S"
    if stage["blocks"][stop_audio]["opcode"] != "sound_stopallsounds" or stage["blocks"][stop_audio]["next"] is not None:
        parser.error("Unexpected Stage death-audio cleanup.")
    audio_done = "xbox-mario-death-audio-stopped"
    done = block("data_setvariableto", stop_audio)
    done["fields"] = {"VARIABLE": ["_xboxDeathAudioReady", audio_id]}
    done["inputs"] = {"VALUE": [1, [4, 1]]}
    stage["blocks"][stop_audio]["next"] = audio_done
    stage["blocks"][audio_done] = done
    audio_reset = "xbox-mario-death-audio-reset"
    reset = block("data_setvariableto", None)
    reset["fields"] = {"VARIABLE": ["_xboxDeathAudioReady", audio_id]}
    reset["inputs"] = {"VALUE": [1, [4, 0]]}
    splice_before(broadcast, audio_reset, reset)
    delay = "?W-kWHtMU@r0efN}{2hd"
    if blocks[delay]["opcode"] != "control_wait" or blocks[delay]["inputs"] != {"DURATION": [1, [5, 0.04]]}:
        parser.error("Unexpected pre-cue delay.")
    audio_condition = "xbox-mario-death-audio-complete"
    blocks[delay]["opcode"] = "control_wait_until"
    blocks[delay]["inputs"] = {"CONDITION": [2, audio_condition]}
    condition = block("operator_equals", delay)
    condition["inputs"] = {"OPERAND1": [3, [12, "_xboxDeathAudioReady", audio_id], [10, ""]],
                           "OPERAND2": [1, [4, 1]]}
    blocks[audio_condition] = condition

    with zipfile.ZipFile(args.output, "x") as prepared:
        for info in original.infolist():
            data = json.dumps(project, separators=(",", ":")).encode() if info.filename == "project.json" else original.read(info)
            prepared.writestr(info, data)
print(json.dumps({"source": str(args.source), "output": str(args.output),
                  "source_sha256": source_sha,
                  "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
                  "change": "Order music cleanup, death cue/animation completion, and restart initialization"}, indent=2))
