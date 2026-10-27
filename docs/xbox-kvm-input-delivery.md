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

## Appel timing observations — October 26, 2026

The separate Appel diagnostic run also observed delayed transitions during
platform play. For the spike-column approach, the host commanded Right for
602 ms; Xbox trace timestamps span 964 ms from Right to neutral. A combined
Right+A command began at 00:08:45.409 UTC; its A/Space transition was received
in the diagnostic stream at 00:08:46.108, about 699 ms later. An older Right
state was observed first. This timing can change a jump into walking off a
ledge, but the measurement includes controller transport, console polling and
UDP transit; it does not isolate a firmware defect.

Comparing each inner SDL trace tick with the outer Xbox send tick gives only
0–17 ms of excess logging time in this interval, above the minimum clock-origin
offset. File logging alone therefore does not explain the 699 ms measurement.
Four later Player Two Down/A presses were observed after approximately 195,
102, 148 and 123 ms. That small sample is not proof that switching ports fixes
the shared fault. Both routes still need broader delivery testing.

The S3 direct-report endpoint returned HTTP 409 while the main KVM owned input;
no route configuration or firmware was changed to bypass it. Local evidence is
`build-xbox/network-log/appel-column-input-delay.json`,
`appel-player2-input-latency.json` and `s3-direct-report-check.json`. All 2,750
messages across the two Appel receive windows were found in the retrieved
console file log, with contiguous sequences within and between those windows.

## Frogger held-Start interruption — October 26 evening

Player Two reproduced the interrupted hold during a separate Frogger run on
diagnostic XBE `431b16c7…`. The host commanded Start continuously for 5,025 ms:
all 51 raw reports contained Start, and their largest interval was 103 ms.
The Xbox instead logged Start at tick 2591618, neutral at 2593611, Start again
at 2594109, and final neutral at 2596635. The intervening 498 ms release split
the hold into intervals shorter than the three seconds required to open pause.
That attempt did not open the pause menu. A later bounded ten-second Start
command did, exposing the already-ended round's score six and `alive=0`.

This extends the delivery evidence to Player Two and a non-confirmation button.
It does not isolate the radio/adapter/USB cause or establish that the same gap
caused the traffic death preceding the pause request. Longer host holds and
checking the actual pause snapshot are test-harness recovery measures, not a
runtime or firmware fix. Evidence is ignored
`build-xbox/hardware/frogger-start-delivery-gap.json`, the corresponding
`frog-video-pad7-client.json`, and
`build-xbox/variable-watch/hardware-run5.jsonl`.


## Breakout fast-steering follow-up — October 27

With the normal `b75e9beb` build and prepared `e6f533ce` archive, the first
physical round ended normally at Score 640. Near its final loss, the visible
paddle stayed at the right limit while fresh frames showed the ball crossing
the field. The host sent Left reports for more than a second. This establishes
a command/movement mismatch, not whether the cause is remote delivery, SDL
input selection or project-thread execution. Cached adapter labels were too
old to establish the actual USB state during that segment.

A separate bounded Game Over probe used fresh Player Two `/status` reads and
four two-second direction holds, separated by neutral intervals. Every hold
showed the expected adapter state and moved the paddle to its corresponding
edge; neutral stopped it. This small probe sampled the adapter HTTP server,
unlike the ordinary gameplay run, and does not rule out intermittent failures.
No KVM firmware or route configuration was changed. Removing redundant
immediate host reports while retaining 100ms refreshes did not establish a fix:
the next native Green Flag replay restored 0/three lives but ended at Score 60.

Original video, client reports, adapter samples and summaries are in ignored
`hardware/breakout-paddle-v3-vision-run{1,2}` and
`hardware/breakout-paddle-v3-delivery`. A separate diagnostic build is being
prepared to correlate received input transitions with read-only Ball/Paddle
positions and the controller-mode variable. Normal gameplay acceptance remains
separate from that timing-sensitive diagnostic run.


The first correlated input/sprite trace ended naturally at Score 200. Its
console-file comparison verified all 990 relevant input/watch records and
1,036 UDP packets, with contiguous sequence numbers. Across 144 sampled
intervals with a stable received directional input, the interior paddle neither
stayed stationary nor moved opposite to that input. This did not reproduce the
earlier stall. Most longer host-command transitions appeared in the received
trace after roughly 40–115ms; one commanded release appeared about 521ms later.
That measurement includes the controller path and console polling; it does not
isolate a radio, queue, USB or SDL fault. Input and watch clock origins were
aligned explicitly, rather than treating the outer UDP tick and inner SDL
timestamp as the same clock. Raw evidence and analysis remain in ignored
`paddle-input-trace-20261027/run1-analysis.json`, `latency-probes.json` and
`hardware-run1-file-verification.json`.
