#!/usr/bin/env python3
"""Generate a small, redistributable Scratch 3 (.sb3) test project for V.Flash bring-up."""
import hashlib
import json
import math
import struct
import zipfile
import zlib
from pathlib import Path


def make_png(width: int, height: int, rgba_bytes: bytes) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(rgba_bytes[y * stride:(y + 1) * stride])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")


def make_stage_png() -> bytes:
    w, h = 480, 360
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            if x < 8 or x >= w - 8 or y < 8 or y >= h - 8:
                r, g, b = 40, 180, 99
            elif ((x // 40) + (y // 40)) % 2 == 0:
                r, g, b = 30, 60, 110
            else:
                r, g, b = 22, 46, 88
            buf[idx:idx + 4] = bytes((r, g, b, 255))
    return make_png(w, h, bytes(buf))


def make_sprite_png() -> bytes:
    w, h = 64, 64
    buf = bytearray(w * h * 4)
    cx, cy = 32, 32
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            dx, dy = x - cx, y - cy
            d2 = dx * dx + dy * dy
            if d2 <= 28 * 28:
                # Eyes
                if (abs(dx - 10) <= 4 and abs(dy + 6) <= 5) or (abs(dx + 10) <= 4 and abs(dy + 6) <= 5):
                    buf[idx:idx + 4] = bytes((255, 255, 255, 255))
                elif d2 >= 24 * 24:
                    buf[idx:idx + 4] = bytes((255, 220, 60, 255))
                else:
                    buf[idx:idx + 4] = bytes((230, 70, 50, 255))
            else:
                buf[idx:idx + 4] = bytes((0, 0, 0, 0))
    return make_png(w, h, bytes(buf))


def make_beep_wav(sample_rate: int = 22050, duration_s: float = 0.25, freq: float = 440.0) -> bytes:
    num_samples = int(sample_rate * duration_s)
    pcm = bytearray()
    for i in range(num_samples):
        env = 1.0 - (i / num_samples)
        val = int(16000 * env * math.sin(2.0 * math.pi * freq * i / sample_rate))
        pcm.extend(struct.pack("<h", max(-32768, min(32767, val))))
    data_size = len(pcm)
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", 36 + data_size, b"WAVE",
        b"fmt ", 16, 1, 1, sample_rate, sample_rate * 2, 2, 16,
        b"data", data_size,
    )
    return header + bytes(pcm)


def main() -> None:
    out_path = Path(__file__).resolve().parent.parent / "test-project" / "project.sb3"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    stage_png = make_stage_png()
    sprite_png = make_sprite_png()
    beep_wav = make_beep_wav()

    stage_md5 = hashlib.md5(stage_png).hexdigest()
    sprite_md5 = hashlib.md5(sprite_png).hexdigest()
    beep_md5 = hashlib.md5(beep_wav).hexdigest()

    project = {
        "targets": [
            {
                "isStage": True,
                "name": "Stage",
                "variables": {
                    "var_score": ["score", 0]
                },
                "lists": {},
                "broadcasts": {},
                "blocks": {},
                "comments": {},
                "currentCostume": 0,
                "costumes": [
                    {
                        "name": "backdrop1",
                        "bitmapResolution": 1,
                        "dataFormat": "png",
                        "assetId": stage_md5,
                        "md5ext": f"{stage_md5}.png",
                        "rotationCenterX": 240,
                        "rotationCenterY": 180,
                    }
                ],
                "sounds": [],
                "volume": 100,
                "layerOrder": 0,
                "tempo": 60,
                "videoTransparency": 50,
                "videoState": "on",
                "textToSpeechLanguage": None,
            },
            {
                "isStage": False,
                "name": "Sprite1",
                "variables": {},
                "lists": {},
                "broadcasts": {},
                "blocks": {
                    "b_flag": {
                        "opcode": "event_whenflagclicked",
                        "next": "b_goto",
                        "parent": None,
                        "inputs": {},
                        "fields": {},
                        "shadow": False,
                        "topLevel": True,
                        "x": 50,
                        "y": 50,
                    },
                    "b_goto": {
                        "opcode": "motion_gotoxy",
                        "next": "b_setscore",
                        "parent": "b_flag",
                        "inputs": {
                            "X": [1, [4, "0"]],
                            "Y": [1, [4, "0"]],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_setscore": {
                        "opcode": "data_setvariableto",
                        "next": "b_forever",
                        "parent": "b_goto",
                        "inputs": {
                            "VALUE": [1, [10, "0"]],
                        },
                        "fields": {
                            "VARIABLE": ["score", "var_score"],
                        },
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_forever": {
                        "opcode": "control_forever",
                        "next": None,
                        "parent": "b_setscore",
                        "inputs": {
                            "SUBSTACK": [2, "b_changex"],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_changex": {
                        "opcode": "motion_changexby",
                        "next": "b_if",
                        "parent": "b_forever",
                        "inputs": {
                            "DX": [1, [4, "4"]],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_if": {
                        "opcode": "control_if",
                        "next": "b_incscore",
                        "parent": "b_changex",
                        "inputs": {
                            "CONDITION": [2, "b_gt"],
                            "SUBSTACK": [2, "b_setx"],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_gt": {
                        "opcode": "operator_gt",
                        "next": None,
                        "parent": "b_if",
                        "inputs": {
                            "OPERAND1": [3, "b_xpos", [10, ""]],
                            "OPERAND2": [1, [10, "150"]],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_xpos": {
                        "opcode": "motion_xposition",
                        "next": None,
                        "parent": "b_gt",
                        "inputs": {},
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_setx": {
                        "opcode": "motion_setx",
                        "next": "b_wrap_sound",
                        "parent": "b_if",
                        "inputs": {
                            "X": [1, [4, "-150"]],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_wrap_sound": {
                        "opcode": "sound_play",
                        "next": None,
                        "parent": "b_setx",
                        "inputs": {
                            "SOUND_MENU": [1, "b_wrap_sound_menu"],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_wrap_sound_menu": {
                        "opcode": "sound_sounds_menu",
                        "next": None,
                        "parent": "b_wrap_sound",
                        "inputs": {},
                        "fields": {
                            "SOUND_MENU": ["beep", None],
                        },
                        "shadow": True,
                        "topLevel": False,
                    },
                    "b_incscore": {
                        "opcode": "data_changevariableby",
                        "next": None,
                        "parent": "b_if",
                        "inputs": {
                            "VALUE": [1, [4, "1"]],
                        },
                        "fields": {
                            "VARIABLE": ["score", "var_score"],
                        },
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_space": {
                        "opcode": "event_whenkeypressed",
                        "next": "b_sound",
                        "parent": None,
                        "inputs": {},
                        "fields": {
                            "KEY_OPTION": ["space", None],
                        },
                        "shadow": False,
                        "topLevel": True,
                        "x": 50,
                        "y": 400,
                    },
                    "b_sound": {
                        "opcode": "sound_play",
                        "next": "b_changey",
                        "parent": "b_space",
                        "inputs": {
                            "SOUND_MENU": [1, "b_sound_menu"],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                    "b_sound_menu": {
                        "opcode": "sound_sounds_menu",
                        "next": None,
                        "parent": "b_sound",
                        "inputs": {},
                        "fields": {
                            "SOUND_MENU": ["beep", None],
                        },
                        "shadow": True,
                        "topLevel": False,
                    },
                    "b_changey": {
                        "opcode": "motion_changeyby",
                        "next": None,
                        "parent": "b_sound",
                        "inputs": {
                            "DY": [1, [4, "20"]],
                        },
                        "fields": {},
                        "shadow": False,
                        "topLevel": False,
                    },
                },
                "comments": {},
                "currentCostume": 0,
                "costumes": [
                    {
                        "name": "costume1",
                        "bitmapResolution": 1,
                        "dataFormat": "png",
                        "assetId": sprite_md5,
                        "md5ext": f"{sprite_md5}.png",
                        "rotationCenterX": 32,
                        "rotationCenterY": 32,
                    }
                ],
                "sounds": [
                    {
                        "name": "beep",
                        "assetId": beep_md5,
                        "dataFormat": "wav",
                        "format": "",
                        "rate": 22050,
                        "sampleCount": int(22050 * 0.25),
                        "md5ext": f"{beep_md5}.wav",
                    }
                ],
                "volume": 100,
                "layerOrder": 1,
                "visible": True,
                "x": 0,
                "y": 0,
                "size": 100,
                "direction": 90,
                "draggable": False,
                "rotationStyle": "all around",
            },
        ],
        "monitors": [
            {
                "id": "var_score",
                "mode": "default",
                "opcode": "data_variable",
                "params": {"VARIABLE": "score"},
                "spriteName": None,
                "value": 0,
                "width": 0,
                "height": 0,
                "x": 5,
                "y": 5,
                "visible": True,
                "sliderMin": 0,
                "sliderMax": 100,
                "isDiscrete": True,
            }
        ],
        "extensions": [],
        "meta": {
            "semver": "3.0.0",
            "vm": "0.2.0",
            "agent": "vflash-test-generator",
        },
    }

    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("project.json", json.dumps(project, indent=2))
        zf.writestr(f"{stage_md5}.png", stage_png)
        zf.writestr(f"{sprite_md5}.png", sprite_png)
        zf.writestr(f"{beep_md5}.wav", beep_wav)

    print(f"Generated {out_path} ({out_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
