#!/usr/bin/env python3
"""Host disassembly and wavefront simulation checks for Xbox 360 costume shader."""
from __future__ import annotations

import colorsys
import math
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "include/renderers/xbox360/costume_shaders_xbox360.hpp"
TOOLCHAIN = pathlib.Path("/home/mikedev/xsdk/xenos-toolchain")
sys.path.insert(0, str(TOOLCHAIN))

from xdis.simulate import run_group  # noqa: E402


def load_shader():
    text = HEADER.read_text(encoding="utf-8")
    m = re.search(r"costume_frag_container\[\]\s*=\s*\{(.*?)\};", text, re.S)
    assert m, "Missing costume_frag_container"
    words = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
    blob = struct.pack(f">{len(words)}I", *words)
    literals: dict[int, list[float]] = {}
    for lm in re.finditer(r"\{\s*(\d+),\s*\{\s*([^}]+)\}\s*\}", text):
        reg = int(lm.group(1))
        literals[reg] = [float(x.rstrip("f ")) for x in lm.group(2).split(",")]
    return blob, literals


def ref_frag(
    u: float,
    v: float,
    effects: tuple[float, float, float, float] = (1.0, 0.0, 0.0, 0.0),
    distort: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0),
    texinfo: tuple[float, float, float, float] = (64.0, 64.0, 0.0, 0.0),
):
    opacity, brightness, color_fx, mosaic = effects
    pixelate, whirl, fisheye, _ = distort
    tw, th = texinfo[0], texinfo[1]
    if abs(mosaic) > 0.001:
        tiles = min(max(float(int(round((abs(mosaic) + 10.0) / 10.0 + 1e-3))), 1.0), 512.0)
        u = (u * tiles) % 1.0
        v = (v * tiles) % 1.0
    if abs(pixelate) > 0.001:
        amt = max(abs(pixelate) / 10.0, 1e-3)
        pxw, pxh = tw / amt, th / amt
        u = (math.floor(u * pxw) + 0.5) / pxw
        v = (math.floor(v * pxh) + 0.5) / pxh
    cx, cy = u - 0.5, v - 0.5
    if abs(whirl) > 0.001:
        rad = whirl * 3.14159265 / 180.0
        d = math.hypot(cx, cy)
        f = max(1.0 - d / 0.5, 0.0)
        wa = rad * f * f
        s, c = math.sin(wa), math.cos(wa)
        cx, cy = c * cx - s * cy, s * cx + c * cy
    if abs(fisheye) > 0.001:
        amt = max(0.0, (fisheye + 100.0) / 100.0)
        vx, vy = cx / 0.5, cy / 0.5
        ln = math.hypot(vx, vy)
        if ln > 0.0001:
            r = (min(ln, 1.0) ** amt) * max(1.0, ln)
            cx, cy = r * (vx / ln) * 0.5, r * (vy / ln) * 0.5
    nu, nv = cx + 0.5, cy + 0.5
    killed = nu < 0.0 or nu > 1.0 or nv < 0.0 or nv > 1.0
    r, g, b, a = nu, nv, 0.4, 0.8
    if abs(color_fx) > 0.001:
        h, s, val = colorsys.rgb_to_hsv(r, g, b)
        min_l, min_s = 0.11 / 2.0, 0.09
        if val < min_l:
            h, s, val = 0.0, 1.0, min_l
        elif s < min_s:
            h, s, val = 0.0, min_s, val
        h = (h + color_fx / 200.0) % 1.0
        r, g, b = colorsys.hsv_to_rgb(h, s, val)
    ro = min(max(r + brightness, 0.0), 1.0)
    go = min(max(g + brightness, 0.0), 1.0)
    bo = min(max(b + brightness, 0.0), 1.0)
    return killed, (ro, go, bo, a * opacity)


def main() -> int:
    blob, literals = load_shader()
    with tempfile.NamedTemporaryFile(suffix=".bin") as tf:
        tf.write(blob)
        tf.flush()
        dis = subprocess.run(
            ["python3", str(TOOLCHAIN / "xdis/xdis.py"), tf.name],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
    assert "dp4" not in dis, "Unexpected dp4 instruction in vec2 costume shader disassembly"

    pts = [
        (0.5, 0.5),
        (0.50002, 0.5),
        (0.6, 0.5),
        (0.75, 0.6),
        (0.2, 0.2),
        (0.95, 0.95),
        (0.05, 0.05),
        (0.99, 0.5),
        (-0.01, 0.5),
        (1.01, 0.5),
    ]
    cases = [
        ("baseline", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 0.0)),
        ("whirl_+180", (1.0, 0.0, 0.0, 0.0), (0.0, 180.0, 0.0, 0.0)),
        ("whirl_-180", (1.0, 0.0, 0.0, 0.0), (0.0, -180.0, 0.0, 0.0)),
        ("whirl_+360", (1.0, 0.0, 0.0, 0.0), (0.0, 360.0, 0.0, 0.0)),
        ("fisheye_+100", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 100.0, 0.0)),
        ("fisheye_+200", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 200.0, 0.0)),
        ("fisheye_-50", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -50.0, 0.0)),
        ("fisheye_-100", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -100.0, 0.0)),
        ("fisheye_-150", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -150.0, 0.0)),
        ("whirl90+fish50", (1.0, 0.0, 0.0, 0.0), (0.0, 90.0, 50.0, 0.0)),
        ("all_combined", (0.7, 0.15, 50.0, 20.0), (20.0, 120.0, 80.0, 0.0)),
    ]

    def tfetch(slot, coord):
        return [coord[0], coord[1], 0.4, 0.8]

    overall_err = 0.0
    for zw in [(0.0, 1.0), (0.0, 0.0)]:
        for label, fx, dist in cases:
            consts = dict(literals)
            consts[0] = list(fx)
            consts[1] = list(dist)
            consts[2] = [64.0, 64.0, 0.0, 0.0]
            regs = [{0: [u, v, zw[0], zw[1]]} for u, v in pts]
            results = run_group(blob, regs, constants=consts, texture_fetch=tfetch)
            for (u, v), res in zip(pts, results):
                rk, rrgba = ref_frag(u, v, fx, dist)
                assert res.killed == rk, f"{label} zw={zw} ({u},{v}): sim kill={res.killed} != ref {rk}"
                if not res.killed:
                    err = max(abs(a - b) for a, b in zip(res.exports[0], rrgba))
                    overall_err = max(overall_err, err)
                    assert err < 1e-4, f"{label} zw={zw} ({u},{v}): err={err}"

    print(f"PASS: costume_frag disassembly (0 dp4) & 220 wavefront checks match GLCore (max_err={overall_err:.6f})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
