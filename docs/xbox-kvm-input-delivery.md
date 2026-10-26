# KVM input delivery and unexpected browser launches

Two physical Xbox reproductions explain the intermittent game reload after
Exit: a continuously commanded A hold arrives at the runtime as A, neutral,
then A again. The browser arms on neutral and accepts the second press. This
does not establish the precise fault in the KVM-to-Xbox delivery path, and the
shared handoff issue remains unresolved.

Both runs used diagnostic XBE SHA-256
`55ed072f07a1614d2ab38774fdf7f13dc916e84aaf6118ad381be48987bfa635`,
with `SE_XBOX_TRACE_MENU_INPUT=ON`, and the normal prepared 2048 archive
`088e8fdc2bc4339ca3f0244b149cb151d713906533decba209f65624e83c78e0`.
They used one continuous video viewer, 100 ms raw controller refreshes and
500 ms WebSocket pings for cached route status. Neither test directly polled
the S3 adapter HTTP servers. All controller reports were released afterwards.

## Continuous client input, interrupted Xbox input

The bounded test launches 2048 from the browser, holds Start for 6.5 seconds
to open pause, then holds A on Exit for 2.2 seconds. It stops if the final
screen is not the browser.

| Observation | Player One | Player Four |
| --- | --- | --- |
| Successful cycles before first failure | 1 | 3 |
| Failed A hold duration | 2202 ms | 2203 ms |
| Raw reports during that hold | 23, all A | 23, all A |
| Largest report interval during that hold | 103 ms | 102 ms |
| Sent frames / control acknowledgements, entire run | 328 / 328 | 644 / 644 |
| Largest acknowledgement round trip, entire run | 8 ms | 50 ms |
| First A / neutral / second A, Xbox tick ms | 258873 / 259373 / 259723 | 57689 / 58356 / 58856 |
| Browser accepted 2048, Xbox tick ms | 259725 | 58858 |

There is no logged active-controller switch within either failed handoff.
The existing menu release gate observes the neutral interval before accepting
the new A edge; these occurrences are not unexplained browser activation.
The KVM protocol acknowledges each received WebSocket frame before dispatch.
The acknowledgement counts therefore establish prompt receipt at the main KVM,
not successful radio or USB delivery to the Xbox.

## Cached routing evidence

During the Player One failure, four cached routing samples continued to label
A held. The main KVM's successful-send counter advanced from 670157 to 670202,
and its send-error counter stayed at 2275. Adapter diagnostic/completion data
aged from 744 to 2246 ms without a refreshed sample. Successful UDP enqueueing
does not prove receipt at the dongle.

During the Player Four failure, the route also continued to label A held, but
its send-error counter rose from 3303 to 3331. Its successful-send counter
advanced only from 686379 to 686382 across the first three cached samples,
then to 686397 in the fourth. Diagnostic/completion sample age reached 3020 ms.
This adds a main-KVM routing failure correlated with the interrupted hold.
The counter combines packet-signing and UDP-send failures in the inspected
source; it does not identify an errno or prove a particular Wi-Fi, queue or
USB fault. The inspected source is context, not verification of the exact
firmware installed on every device.

Changing controller ports is therefore not a demonstrated workaround. No KVM
firmware was flashed, and no game-key debounce or arbitrary browser delay was
added. The 500 ms Player One interval is consistent with input-expiry timers
in the inspected KVM source, but consistency alone does not isolate its origin.

## Local evidence and remaining checks

Ignored evidence is under `build-xbox/hardware/`:

- `held-exit-trace-1-{client,observer,analysis}.json` and
  `held-exit-trace-1-xbox.log` preserve the Player One reproduction.
- `held-exit-p4-trace-1-{client,observer,analysis}.json` and
  `held-exit-p4-trace-1-xbox.log` preserve the Player Four comparison.
- Each prefix also has pause, after-Exit and screen-transition captures.

The two runs do not establish how a directly attached physical controller
behaves during the same transition. KVM transport diagnostics and a direct-pad
comparison remain necessary before claiming reliable delivery or a universal
menu-handoff fix. Full normal-play 2048 victory and the other checks in the
[twenty-game qualification matrix](xbox-game-qualification.md) remain open.
