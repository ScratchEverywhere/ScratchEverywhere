#!/usr/bin/env python3
"""Allow an NXDK XBE to use expanded RAM without changing other header flags."""
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("xbe", type=Path)
args = parser.parse_args()
data = bytearray(args.xbe.read_bytes())
if len(data) < 0x128 or data[:4] != b"XBEH":
    parser.error("Expected a complete XBE header; file was not modified")
flags = struct.unpack_from("<I", data, 0x124)[0]
struct.pack_into("<I", data, 0x124, flags & ~0x4)
temporary = args.xbe.with_suffix(args.xbe.suffix + ".memory.tmp")
temporary.write_bytes(data)
temporary.replace(args.xbe)
print(f"XBE memory flags: {flags:#x} -> {flags & ~0x4:#x}")
