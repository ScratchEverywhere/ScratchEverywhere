# Optional Xbox network diagnostics

Physical Xbox logs normally live in `E:\ScratchEverywhere\debug.txt`, which
requires returning to a dashboard with FTP to retrieve. An optional UDP mirror
lets a development host receive those same log messages while the game runs.
This is useful with the existing pause-state snapshot and menu-input traces.
It provides observations, not debugger commands or game-state writes.

The default `SE_XBOX_TRACE_LOG_HOST` is empty, so normal builds contain no UDP
logging path. Enable it only in a separate diagnostic build with an explicit
unicast IPv4 destination. For example, replacing the host with the receiving
computer's address:

```sh
bash scripts/build-xbox.sh \
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
  -DSE_XBOX_FORCE_480P=ON -DSE_MENU=ON -DSE_LOADSCREEN=OFF \
  -DSE_XBOX_TRACE_LOG_HOST=192.168.1.100 \
  -DSE_XBOX_TRACE_LOG_PORT=18194 \
  -DSE_XBOX_TRACE_GAME_STATE=ON -DSE_XBOX_TRACE_MENU_INPUT=ON
```

No embedded project should be present for the normal browser workflow. Deploy
diagnostic executables in separate folders and verify their complete readback.
Setting the destination does not enable either detailed trace by itself.
Invalid IPv4 literals, multicast destinations and invalid/noncanonical ports
are rejected at configure time; hostnames are not resolved.

Start the receive-only helper on that computer before launching the Xbox app:

```sh
mkdir -p build-xbox/hardware
python3 scripts/receive-xbox-log.py build-xbox/hardware/session.jsonl \
  --bind 0.0.0.0 --peer 192.168.1.254 --duration 1800
```

Use a new output filename for each run; the helper refuses to overwrite
existing evidence. It accepts only the selected peer when `--peer` is given,
sends no replies, and stops at its duration or packet limit. Ctrl+C closes it
and writes a summary. Console output is JSON too, so control characters in a
project's log message remain escaped data. The default port is 18194. Port 0 on the receiver selects
a free local port for loopback tests, not the console's build configuration.

Each JSONL packet records the sender, receive time, sequence, Xbox tick and
message. With the game-state trace enabled, hold Start to open the runtime
pause menu. Its existing stage-variable snapshot reports values such as
Appel's `PLAYER X`, `PLAYER Y`, `LEVEL #`, `SPAWN IDX` and `APPLES`, plus list
sizes and clone populations. Inspect `GAME STATE` too: a menu scene is not a
level-completion result. Release Start, then B resumes without a dashboard
restart. An authored in-game pause is separate from this runtime pause menu.

The console opens a nonblocking socket only after its network stack is ready
and closes it before normal network shutdown. It sends no more than 1023 bytes
per datagram, including the sequence/tick prefix; longer messages are
truncated. `at_packet_limit` flags packets at that size in the receiver.
UDP can lose or reorder packets. Sequence gaps are evidence of missing
datagrams, not proof that the game failed. The file log remains authoritative
and unchanged, including startup messages emitted before networking is ready.
There is no handshake, connection requirement or incoming command handler on
the Xbox. A receiver is optional even for a diagnostic build.

Restore the ordinary build options after diagnosing:

```sh
bash scripts/build-xbox.sh \
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
  -DSE_XBOX_TRACE_LOG_HOST= \
  -DSE_XBOX_TRACE_GAME_STATE=OFF -DSE_XBOX_TRACE_MENU_INPUT=OFF
```

The receiver's loopback integration checks exercise actual UDP delivery,
multiline/invalid-UTF8 preservation, peer filtering, duration/packet limits,
no replies and refusal to overwrite:

```sh
python3 tests/xbox_log_receiver_test.py
```

## Physical Xbox verification — October 26, 2026

On the 128MB console, diagnostic XBE SHA-256
`debed504cc0dc0af54512e237adbce1fde49af9cfe504b203b9666e551a1f7ec`
booted the populated browser and launched the original Appel archive. Two
runtime-pause snapshots arrived live: `PLAYER X` changed from `128` to
`250.07256060184272` after ordinary Right input, with `GAME STATE=PLAY` in the
second snapshot. The snapshots also included `PLAYER Y`, `APPLES`, `LEVEL #`,
`SPAWN IDX`, list sizes and clone counts. No debugger writes were used.

With the receiver stopped, B resumed, Right moved the player, A jumped and
long Start opened pause again. A third snapshot recovered afterwards from
the file reported `PLAYER X=402.1210090955719`, `GAME STATE=PLAY`. Restarting
the receiver then captured the browser return and controller dashboard reset.
All 268 received messages were found in the retrieved file log; each receive
window had contiguous sequences. The deliberate receiver-off interval explains
the gap between windows. Maximum captured datagram size was 184 bytes, so this
run does not establish behavior under packet truncation or network saturation.

Ignored local evidence is in `build-xbox/network-log/`: `hardware-run1.jsonl`,
`hardware-run2.jsonl`, `diagnostic-final-runtime.log` and
`hardware-log-comparison.json`. The receiver's four loopback tests pass. Both
enabled and disabled NXDK builds pass; malformed octets, multicast, invalid
ports, leading-zero ports and a semicolon-separated host are rejected during
configuration. These are diagnostic-tool checks, not Appel level completion.

Normal XBE SHA-256
`29ed2ffc3410b0dfdfa544b1b3b203bfb2ac6d416317239f0b9d0894709c6138`
has an empty destination and both detailed traces off. Its binary contains no
UDP packet-format string. Deployed separately with full readback, it booted
the browser, launched Appel Level 1, moved/jumped and collected one apple,
opened runtime pause and returned to the browser. The listening host received
no packets from this normal-build run. Initial short menu clicks remained on
the level picker; a later click entered Level 1. The shared KVM delivery issue
remains separate from UDP logging.
