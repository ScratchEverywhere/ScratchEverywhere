#!/usr/bin/env python3
"""Repeatable FlashEm performance benchmark suite for ScratchEverywhere V.Flash."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import tempfile

SE_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SDK = Path("/home/mikedev/Documents/GitHub/vflash-sdk")
DEFAULT_WORKLOADS = [
    ("simple", Path("/tmp/scratch_xform_test.sb3")),
    ("sprite_heavy", Path("/tmp/scratch_bench25.sb3")),
    ("churn", Path("/tmp/atlas_churn_5000.sb3")),
]

TEL_RE = re.compile(
    r"\[vflash-telemetry\]\s+f=(\d+)\s+elapsed=([\d.]+)ms\s+vm=([\d.]+)ms\s+"
    r"upload=([\d.]+)ms\s+render=([\d.]+)ms\s+present=([\d.]+)ms\s+"
    r"pace=([\d.]+)ms\s+fps=([\d.]+)"
    r"(?:\s+uploads=(\d+)\s+atlas=(\d+)/(\d+)\(live=(\d+),fail=(\d+)\)\s+heap=(\d+)KB)?"
)
GE_RE = re.compile(
    r"ge:\s+(\d+)\s+lists,\s+(\d+)\s+sprites,\s+(\d+)\s+fills,\s+"
    r"(\d+)\s+model calls,\s+(\d+)\s+triangles,\s+(\d+)\s+pixels total"
)


def parse_log(text):
    tel_rows = []
    for m in TEL_RE.finditer(text):
        tel_rows.append({
            "frames": int(m.group(1)),
            "elapsed_ms": float(m.group(2)),
            "vm_ms": float(m.group(3)),
            "upload_ms": float(m.group(4)),
            "render_ms": float(m.group(5)),
            "present_ms": float(m.group(6)),
            "pace_ms": float(m.group(7)),
            "fps": float(m.group(8)),
            "uploads": int(m.group(9)) if m.group(9) is not None else 0,
            "atlas_used": int(m.group(10)) if m.group(10) is not None else 0,
            "atlas_total": int(m.group(11)) if m.group(11) is not None else 0,
            "atlas_live": int(m.group(12)) if m.group(12) is not None else 0,
            "atlas_fail": int(m.group(13)) if m.group(13) is not None else 0,
            "heap_kb": int(m.group(14)) if m.group(14) is not None else 0,
        })

    ge_rows = []
    for m in GE_RE.finditer(text):
        lists = int(m.group(1))
        if lists == 0:
            continue
        ge_rows.append({
            "lists": lists,
            "sprites": int(m.group(2)),
            "fills": int(m.group(3)),
            "model_calls": int(m.group(4)),
            "triangles": int(m.group(5)),
            "pixels": int(m.group(6)),
        })
    return tel_rows, ge_rows


def run_one(name, sb3_path, mmu, frames, sdk_root, work_dir):
    build_dir = SE_ROOT / ("build-vflash-mmu" if mmu == "on" else "build-vflash")
    elf = build_dir / "scratch-vflash.elf"
    if not elf.exists():
        raise FileNotFoundError(f"Missing ELF: {elf}")

    run_dir = work_dir / f"{name}_{mmu}"
    assets_dir = run_dir / "assets"
    assets_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(sb3_path, assets_dir / "PROJECT.SB3")
    shutil.copytree(SE_ROOT / "gfx", assets_dir / "gfx", dirs_exist_ok=True)

    boot_bin = run_dir / "BOOT.BIN"
    iso_path = run_dir / "disc.iso"
    cue_path = run_dir / "disc.cue"
    log_path = run_dir / "uart.log"

    subprocess.run(
        [sys.executable, str(sdk_root / "tools/pack_boot.py"), str(elf), str(boot_bin)],
        check=True, stdout=subprocess.DEVNULL,
    )
    subprocess.run(
        [sys.executable, str(sdk_root / "tools/make_iso.py"), "--assets-dir", str(assets_dir), str(boot_bin), str(iso_path)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [sys.executable, str(sdk_root / "tools/raw_cd.py"), str(iso_path), str(cue_path)],
        check=True, stdout=subprocess.DEVNULL,
    )

    env = os.environ.copy()
    env["FLASHEM_BIOS"] = str(sdk_root / "dumps/A29L160ATV@TSOP48.BIN")
    env["VFLASH_EXIT"] = str(frames)
    with open(log_path, "w") as lf:
        subprocess.run(
            [str(sdk_root / "build/emulator/flashem"), "--headless", str(cue_path)],
            env=env, stdout=lf, stderr=subprocess.STDOUT, check=True, timeout=180,
        )

    text = log_path.read_text(errors="replace")
    tel_rows, ge_rows = parse_log(text)
    if not tel_rows:
        return {"name": name, "mmu": mmu, "windows": 0, "error": "no telemetry lines (still initializing?)"}

    steady = tel_rows[1:] if len(tel_rows) > 2 else tel_rows
    ge_steady = ge_rows[1:] if len(ge_rows) > 2 else ge_rows

    def avg(rows, key):
        return round(statistics.mean(r[key] for r in rows), 2) if rows else 0.0

    return {
        "name": name,
        "mmu": mmu,
        "windows": len(tel_rows),
        "fps": avg(steady, "fps"),
        "elapsed_ms": avg(steady, "elapsed_ms"),
        "vm_ms": avg(steady, "vm_ms"),
        "upload_ms": avg(steady, "upload_ms"),
        "render_ms": avg(steady, "render_ms"),
        "present_ms": avg(steady, "present_ms"),
        "pace_ms": avg(steady, "pace_ms"),
        "uploads_per_win": avg(steady, "uploads"),
        "atlas_used": max(r["atlas_used"] for r in tel_rows),
        "atlas_total": max(r["atlas_total"] for r in tel_rows),
        "atlas_live": max(r["atlas_live"] for r in tel_rows),
        "atlas_fail": max(r["atlas_fail"] for r in tel_rows),
        "heap_kb": max(r["heap_kb"] for r in tel_rows),
        "ge_lists_60f": avg(ge_steady, "lists"),
        "ge_sprites_60f": avg(ge_steady, "sprites"),
        "ge_fills_60f": avg(ge_steady, "fills"),
        "ge_tris_60f": avg(ge_steady, "triangles"),
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sdk-root", type=Path, default=DEFAULT_SDK)
    p.add_argument("--frames", type=int, default=1800)
    p.add_argument("--mmu", choices=["on", "off", "both"], default="on")
    p.add_argument("--out", type=Path, default=Path("/tmp/vflash_bench"))
    args = p.parse_args()

    mmus = ["off", "on"] if args.mmu == "both" else [args.mmu]
    if args.out.exists():
        shutil.rmtree(args.out)
    args.out.mkdir(parents=True, exist_ok=True)

    results = []
    for name, sb3 in DEFAULT_WORKLOADS:
        if not sb3.exists():
            print(f"Skipping {name}: {sb3} not found", file=sys.stderr)
            continue
        for mmu in mmus:
            res = run_one(name, sb3, mmu, args.frames, args.sdk_root, args.out)
            results.append(res)
            print(json.dumps(res), flush=True)

    (args.out / "summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
