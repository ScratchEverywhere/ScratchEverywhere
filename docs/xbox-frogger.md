# Frogger hardware checks

The installed cs50 Frogger SB2 archive has SHA-256
`6df88eff3a23796d3abc6ebde5cf7ec9b1df611ff6937c093a6cc59eb048a662`. Its sidecar
maps arrows to both the D-pad and left stick. The original scripts and assets
remain unchanged.

On October 26, synchronized build `be4bc0b4…` was uploaded separately to
`F:/scratch-sync-20261026/default.xbe`, with full FTP readback verification.
Normal browser selection launched the game. D-pad Left, Right and Up moved the
frog; traffic collisions displayed the complete Game Over message and the
authored timer bubble. Runtime pause and Green Flag repeatedly restarted the
game. An eight-second HDMI capture contains non-clipping audio (peak 0.328125,
RMS 0.013817); this establishes signal, not subjective or long-run sound
quality.

One continuous Up crossing cleared all five traffic lanes and reached the bank
at captured screen position (360, 252). Input/video delay carried that attempt
into the river, where the frog died. Another crossing reached the river's edge,
and the frog was carried left while no direction was held: captured X moved from
360 toward the left edge at approximately the same Y. The video-guided helper
then lost tracking and released its inputs. That is not evidence of a game
crash. A later traffic collision was also observed.

The temporary helpers only send controller reports and inspect live video; they
do not modify the game or runtime. These attempts do not establish lily-pad
scoring, a full river crossing or the seven-pad win condition. Those checks
remain open. Earlier builds established stick movement, browser exit and repeat
launch; they are recorded in the qualification matrix.

Local evidence is under ignored `build-xbox/hardware/`: the installed archive
and profile copies, `upstream-sliders-deploy.json`, `frog-sync-start.jpg`,
`frog-sync-up-first.jpg`, `frog-sync-road-plan.jpg`,
`frogger-sync-audio.{wav,json}`, `frog-corridor-road-crossed.jpg`,
`frog-corridor3-road-crossed.jpg`, `frog-corridor3-08.jpg` and the corresponding
`frog-corridor*.json` controller/position traces. Video freshness was checked at
15 ms frame age with one viewer during the later attempts.
