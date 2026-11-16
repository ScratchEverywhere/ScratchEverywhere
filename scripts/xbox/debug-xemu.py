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
parser.add_argument("--keyboard-controller", action="store_true", help="Bind the test guest's player-one controller to the keyboard")
parser.add_argument("--persist-disk", action="store_true", help="Keep writes to the private HDD copy between test runs")
args = parser.parse_args()
if not 1 <= args.port <= 65535:
    parser.error("port must be between 1 and 65535")
if not Path(args.xemu).is_file() or not args.iso.is_file():
    parser.error("xemu executable and built ISO must exist")
source = tomllib.loads(args.config.read_text())
files = source.get("sys", {}).get("files", {})
work = args.work_dir.resolve()
work.mkdir(parents=True, exist_ok=True)
source_paths = {}
for key in ("bootrom_path", "flashrom_path", "hdd_path", "eeprom_path"):
    original = Path(files.get(key, "")).expanduser()
    if not original.is_file():
        parser.error(f"The source config needs a valid {key}")
    source_paths[key] = original.resolve()
config = work / "guest.toml"
private_paths = {"hdd_path": work / "xbox_hdd.qcow2", "eeprom_path": work / "eeprom.bin"}
# Validate every output before copying or writing; cross-file links could otherwise
# let an EEPROM write damage the source HDD, or overwrite the source config.
protected = [args.config.resolve(), args.iso.resolve(), *source_paths.values()]
for output in [config, *private_paths.values()]:
    if output.exists() and any(os.path.samefile(output, original) for original in protected):
        parser.error(f"The test output {output.name} must not alias a source input")
guest_files = {}
for key, original in source_paths.items():
    if key in private_paths:
        private = private_paths[key]
        if not private.exists():
            shutil.copy2(original, private)
        guest_files[key] = str(private)
    else:
        guest_files[key] = str(original)
guest_files["dvd_path"] = str(args.iso.resolve())
text = '[general]\nshow_welcome = false\n[sys]\nmem_limit = "' + str(args.memory) + '"\n[sys.files]\n'
text += "".join(key + " = " + json.dumps(value) + "\n" for key, value in guest_files.items())
text += '\n[net]\nenable = true\nbackend = "nat"\n'
if args.keyboard_controller:
    text += '\n[input]\nauto_bind = false\n[input.bindings]\nport1 = "keyboard"\n'
config.write_text(text)
# -gdb with an explicit loopback address is the local-only equivalent of -s.
# The NAT backend supplies DHCP; a disconnected hub leaves nxNetInit waiting.
command = [args.xemu, "-config_path", str(config), "-S",
           "-gdb", f"tcp:127.0.0.1:{args.port}"]
if not args.persist_disk:
    command.append("-snapshot")
if args.qmp:
    command += ["-qmp", f"unix:{args.qmp.resolve()},server=on,wait=off"]
print(f"Paused xemu guest; attach GDB to 127.0.0.1:{args.port}", flush=True)
os.execv(args.xemu, command)
