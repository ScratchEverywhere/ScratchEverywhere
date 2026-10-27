# PacMan hardware qualification

The cdaniels7088 project uses four arrow directions, mapped to the Xbox D-pad
and left stick. It has no authored sound playback. Earlier work repaired its
Game Over name handling; power-pellet/ghost interaction and a complete board
remain unverified.

## Combined pen/exact-pixel engine regression, October 27

Normal candidate `b75e9beb` selected and launched PacMan from the browser.
Holding Up at the initial corridor left the player below the cage wall at
Score 0 while ghosts continued moving. Right collected pellets for Score 20;
Up turned into the next corridor for 40, then Left reached 70. A later original
capture shows 80, followed by a visible natural Game Over. Short Start replay
restored Score 0 and the initial player position, and another Right hold
scored 20. Exit returned to the populated game list.

A fresh browser selection and launch again started at zero. Left moved through
the opening corridor and scored 20; a second Exit returned to the populated
browser. These checks used only HDMI observation
and ordinary controller reports; no debugger, watch state or game-state writes
were used.

A host helper's attempted upper-left bonus route failed. Its global yellow
bounding box included disconnected image noise and later the large yellow
Game Over illustration, so its reported waypoint positions are excluded.
Neither a bonus collection nor frightened/eaten ghosts is established by that
attempt. A replacement connected-component detector requires one player-sized
shape and rejects the ending illustration; six stored-image checks pass, but
that is host-detector validation, not new gameplay evidence.

Ignored evidence is in `hardware/pen-regression-pacman-*.jpg`, the associated
controller reports, `pen-regression-pacman-verification.json`,
`pen-regression-pacman-power-excluded.json` and
`pacman-connected-detector-check.json`. The shared intermittent KVM delivery
issue remains open.
