# Streaming audio continuity

The mixer previously decoded `floor(outputFrames * step) + 2` source frames
per callback, then restarted interpolation at zero on the next callback. It
discarded both lookahead samples and fractional position. At 22050 Hz into
48000 Hz with 1024-frame callbacks, that consumed 472 source frames for about
470.4 frames of output. The gaps accumulate and create callback-boundary
discontinuities. The final partial callback could also read past the valid
decoded data.

Each SoundStream now retains decoded lookahead and fractional position in an
AudioResampler. Only consumed whole frames are removed. Interpolation stops
at decoder EOF, preserves the final sample for interpolation, and leaves the
remaining output untouched for other streams. Short decoder reads are filled
until the required lookahead or EOF. Stream initialization resets this state;
pitch changes retain the current position. Volume, pan and summing stay in the
shared mixer. This change applies to all platforms using that mixer.

## Checks

`python3 tests/audio_resampler_test.py` runs ASan/UBSan against direct
interpolation for 160 rate/channel/callback-size/short-read combinations,
empty and short EOF, additive gains, pitch changes and reset. It also checks
a simulated minute of mono 22050-to-48000 playback. These passed, and the
NXDK build succeeded.

`python3 examples/make-audio-resampler-test.py OUTPUT.sb2` makes a deterministic
two-second mono 22050 Hz signal with no third-party assets. A starts playback
at volume 50; Finished becomes 1 after play-until-done returns. In isolated
xemu, GDB captured the actual Xbox executable's PCM immediately after
Mixer::requestSound and before SDL_PutAudioStreamData. Build `cf887b1d…`
produced 48,128 frames matching continuous interpolation: correlation
0.9999999889, maximum error 0.99988 PCM16 units, identical left/right channels.
Finished reached 1. A second playback produced byte-identical captured PCM.
This verifies many callback boundaries, EOF completion and replay reset; it
does not establish audible quality or performance on hardware.

The prior Retro Racing capture matches an offline simulation of the old
callback-reset algorithm at correlation 0.99999999997. A later whole-game
capture could not be matched to one uninterrupted source clip. Its initial
acc1 segment matches the new continuous interpolation, but the game can
schedule further clips; that mixed capture is not the continuity acceptance
test. The deterministic single-sound fixture supplies that check.

Evidence is under ignored `build-xbox/retro-racing/`: the original
`buffer-boundary-observation.json`, and `resampler/fixture-continuity.json`,
`fixture-{start,finished}.png`, `fixture-pcm.wav`, `fixture-replay-pcm.wav` and
`fixture-replay-comparison.json`. GDB tapping affects timing. Generic QEMU
wavcapture yielded silence because xemu's APU monitor uses its direct SDL
audio stream; that capture was rejected, not treated as a game failure.

Physical-console installation, sound completion/replay and game regressions
remain pending for this audio build. The earlier hardware Retro Racing checks
used build `242d1c78…` without this change.
