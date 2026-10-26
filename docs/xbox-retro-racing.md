# Retro Racing checks

Griffpatch's Retro Racing uses arrows for acceleration, braking/reverse and
steering; A/Space is the handbrake and Y/P is its authored pause control. The
installed profile also binds the left stick to arrows. Hold Start for the
runtime menu, whose Green Flag resets the race and Exit returns to the browser.

## Xbox executable in xemu

Build `242d1c78…` launched from the normal browser with the car, road, HUD and
map rendered. Acceleration reached 64 MPH, D-pad Right and left-stick Left
steered in opposite directions, and grass slowed the car. A braking reduced 39
MPH to 11. Green Flag restored zero speed/time. GDB counted 191 clones before
and after replay, then zero after exit. A fresh browser launch reached 40 MPH
and exited again. These emulator runs use the existing GDB-only network
initialization bypass. They do not measure console performance.

## Physical Xbox, October 26

The same `242d1c78…` build launched twice through the browser. Acceleration
reached 108 MPH; A braking reduced it to 27. D-pad Right changed the car/road
heading, and left-stick Left steered back. Off-road slowdown occurred. Green
Flag restarted the race, both held-confirmation exits returned to the populated
browser, and the Player One controller dashboard combo restored XBMC. The
retrieved log confirms both cleanups, browser activation and the dashboard
reset. An eight-second HDMI capture contains non-clipping engine audio.

After the [audio continuity repair](xbox-audio-resampling.md), build `cf887b1d…`
launched the game and reached 100 MPH. Its 8.09-second HDMI capture also
contains non-clipping audio. Y paused the authored game clock at 17.50 across
captures three seconds apart; Y resumed play and the clock advanced. Runtime
Green Flag then visibly restored 0 MPH and time 00:00.00.

Local evidence is in ignored `build-xbox/hardware/`, including
`retro-hardware-{start,speed,brake,right-turn,stick-left}.jpg`,
`retro-hardware-{exit-browser,second-speed,second-exit-browser}.jpg`,
`retro-hardware-audio.{wav,json}`, `breakout-retro-fixed.log`,
`retro-audio-build.{wav,json}`, `retro-audio-y-{pause,still-paused,resume}.jpg`
and `retro-audio-green-zero.jpg`. The Y-resume capture is taken while the button
remains held; subsequent live video establishes the advancing clock.

Full-lap completion, traffic/collision, deliberate drifting and subjective or
long-run audio remain unverified. Two bounded driving attempts advanced through
the opening bend and later track sections, then stopped when the road-following
helper lost sight of the road. No lap-completion result appeared. This is not a
complete game qualification.

After the driving attempts, runtime Exit returned to the populated browser. The
controller dashboard combo restored XBMC. The final retrieved log records
cleanup, browser activation and the reset. Evidence: ignored
`build-xbox/hardware/retro-audio-exit-browser.jpg` and `audio-fixed-final.log`.
