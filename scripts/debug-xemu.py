#!/usr/bin/env python3
"""Start an isolated xemu guest with a local GDB stub (Python 3.11+)."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

try:
    import tomllib
except ImportError:
    sys.exit("Use Python 3.11 or newer to read xemu's TOML configuration.")

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--config", type=Path, required=True, help="Existing working xemu config; never modified")
parser.add_argument("--xemu", default=shutil.which("xemu") or "/Applications/Xemu.app/Contents/MacOS/xemu")
parser.add_argument("--iso", type=Path, default=root / "build-xbox/scratch-xbox.iso")
parser.add_argument("--work-dir", type=Path, default=root / "build-xbox/xemu-debug")
parser.add_argument("--memory", choices=(64, 128), type=int, default=128)
parser.add_argument("--port", type=int, default=1234)
parser.add_argument("--qmp", type=Path, help="Optional Unix socket for automated reset/status commands")
args = parser.parse_args()
if not 1 <= args.port <= 65535:
    parser.error("port must be between 1 and 65535")
if not Path(args.xemu).is_file() or not args.iso.is_file():
    parser.error("xemu executable and built ISO must exist")
source = tomllib.loads(args.config.read_text())
files = source.get("sys", {}).get("files", {})
work = args.work_dir.resolve()
work.mkdir(parents=True, exist_ok=True)
guest_files = {}
for key in ("bootrom_path", "flashrom_path", "hdd_path", "eeprom_path"):
    original = Path(files.get(key, "")).expanduser()
    if not original.is_file():
        parser.error(f"The source config needs a valid {key}")
    original = original.resolve()
    if key in ("hdd_path", "eeprom_path"):
        private = work / ("xbox_hdd.qcow2" if key == "hdd_path" else "eeprom.bin")
        if not private.exists():
            shutil.copy2(original, private)
        guest_files[key] = str(private)
    else:
        guest_files[key] = str(original)
guest_files["dvd_path"] = str(args.iso.resolve())
config = work / "guest.toml"
text = '[general]\nshow_welcome = false\n[sys]\nmem_limit = "' + str(args.memory) + '"\n[sys.files]\n'
text += "".join(key + " = " + json.dumps(value) + "\n" for key, value in guest_files.items())
text += '\n[net]\nenable = true\nbackend = "nat"\n'
config.write_text(text)
# -gdb with an explicit loopback address is the local-only equivalent of -s.
# The NAT backend supplies DHCP; a disconnected hub leaves nxNetInit waiting.
command = [args.xemu, "-config_path", str(config), "-snapshot", "-S",
           "-gdb", f"tcp:127.0.0.1:{args.port}"]
if args.qmp:
    command += ["-qmp", f"unix:{args.qmp.resolve()},server=on,wait=off"]
print(f"Paused xemu guest; attach GDB to 127.0.0.1:{args.port}", flush=True)
os.execv(args.xemu, command)
