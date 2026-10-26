#!/usr/bin/env python3
"""Receive optional Xbox UDP diagnostics without sending console commands."""

import argparse
import datetime
import ipaddress
import json
import math
from pathlib import Path
import re
import socket
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New JSONL evidence file; existing files are refused")
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=18194)
    parser.add_argument("--peer", help="Accept only this console's IPv4 address")
    parser.add_argument("--duration", type=float, default=300)
    parser.add_argument("--max-packets", type=int, default=20000)
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()
    try:
        for address in [args.bind, args.peer]:
            if address is not None and ipaddress.ip_address(address).version != 4:
                raise ValueError("IPv4 addresses are required")
    except ValueError as error:
        parser.error(str(error))
    if not 0 <= args.port <= 65535:
        parser.error("port must be between 0 and 65535; 0 selects a free receiver port")
    if not math.isfinite(args.duration) or args.duration <= 0 or args.max_packets <= 0:
        parser.error("duration and max-packets must be positive and finite")

    try:
        output = args.output.open("x", encoding="utf-8")
    except FileExistsError:
        parser.error(f"refusing to overwrite {args.output}")
    packets = ignored = 0
    pattern = re.compile(r"^<SE-UDP seq=(\d+) tick=(\d+)> ?(.*)$", re.DOTALL)
    with output, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind((args.bind, args.port))
        ready = {"event": "listener_ready", "bind": args.bind, "port": receiver.getsockname()[1], "peer": args.peer}
        print(json.dumps(ready), flush=True)
        output.write(json.dumps(ready) + "\n")
        output.flush()
        deadline = time.monotonic() + args.duration
        try:
            while packets < args.max_packets:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                receiver.settimeout(min(remaining, 0.5))
                try:
                    payload, source = receiver.recvfrom(2048)
                except socket.timeout:
                    continue
                if args.peer and source[0] != args.peer:
                    ignored += 1
                    continue
                text = payload.decode("utf-8", errors="replace")
                row = {
                    "event": "packet",
                    "received_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    "received_monotonic": time.monotonic(),
                    "peer": source[0],
                    "peer_port": source[1],
                    "bytes": len(payload),
                    "at_packet_limit": len(payload) >= 1023,
                    "raw": text,
                }
                match = pattern.match(text)
                if match:
                    row.update(sequence=int(match[1]), tick_ms=int(match[2]), message=match[3])
                output.write(json.dumps(row) + "\n")
                output.flush()
                packets += 1
                if not args.quiet:
                    print(json.dumps({"sequence": row.get("sequence"), "tick_ms": row.get("tick_ms"),
                                      "message": row.get("message", text)}, ensure_ascii=False), flush=True)
        except KeyboardInterrupt:
            pass
        summary = {"event": "listener_closed", "packets": packets, "ignored_peer_packets": ignored}
        output.write(json.dumps(summary) + "\n")
        print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
