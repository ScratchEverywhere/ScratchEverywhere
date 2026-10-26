"""Create a deterministic mono 22050 Hz SB2 sound for guest mixer checks.

The two-second signal crosses many 1024-frame output callbacks. A starts it
once at volume 50; Finished changes to 1 only after play-until-done returns.
Compare the guest's 48000 Hz PCM against continuous linear interpolation of
0.wav, allowing PCM16 quantization. No third-party assets are required.
"""
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import sys
import wave
import zipfile

out = Path(sys.argv[1])
rate = 22050
pcm = b"".join(struct.pack("<h", round(11000 * (
    0.6 * math.sin(2 * math.pi * (331 * i / rate + 79 * (i / rate) ** 2))
    + 0.3 * math.sin(2 * math.pi * 977 * i / rate)))) for i in range(rate * 2))
audio = io.BytesIO()
with wave.open(audio, "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(rate)
    wav.writeframes(pcm)
sound = audio.getvalue()
svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/></svg>'
project = dict(objName="Stage", variables=[dict(name="Finished", value=0)],
    lists=[], costumes=[dict(costumeName="test", baseLayerID=0,
        baseLayerMD5=hashlib.md5(svg).hexdigest() + ".svg", bitmapResolution=1,
        rotationCenterX=240, rotationCenterY=180)], currentCostumeIndex=0,
    sounds=[dict(soundName="chirp", soundID=0,
        md5=hashlib.md5(sound).hexdigest() + ".wav", sampleCount=rate * 2,
        rate=rate, format="")], tempoBPM=60,
    scripts=[[10, 10, [["whenGreenFlag"], ["setVar:to:", "Finished", 0]]],
        [10, 100, [["whenKeyPressed", "space"], ["stopAllSounds"],
            ["setVar:to:", "Finished", 0], ["setVolumeTo:", 50],
            ["doPlaySoundAndWait", "chirp"], ["setVar:to:", "Finished", 1]]]],
    children=[dict(target="Stage", cmd="getVar:", param="Finished", mode=1,
        visible=True, color=15629590, label="Finished", x=20, y=20)])
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    archive.writestr("0.svg", svg)
    archive.writestr("0.wav", sound)
Path(str(out) + ".json").write_text(json.dumps({"controls": {
    "space": "A", "1": "start", "0": "back"}}, indent=2) + "\n")
print(out)
