# 0.1.2 prerelease validation

2026-11-07, Key 37 / ARMv7 / user-reported Hakai MPC OS 3.9.1.

- Eight self-contained CMake core tests passed on Windows: Analog, FM, waves/FX, FX editing, fast sine, decoder, DSP and instrument.
- Public module loaded and passed the real ARM parameter/playback probe with configurable external library path, MIDI offsets, PCM, Analog/FM/Wavetable/Drawbar, FX edits, bypass, reset, v1/v2 project state and shutdown.
- Open MPC benchmark at 44.1 kHz / 128 frames: WARN, worst p99 23.0%, max 26.1%. Default Solo Trumpet, up to 16 notes, idle, release tail and random parameter sweeps. Timed thread CPU, core 1 affinity.
- Upstream bench.c requires a small pre-test READY wait for Hyperion's asynchronous loader; otherwise an unpaced bench can measure unloaded silence. The exact patch is Tools/openmpc-bench-ready.patch. Timing/stages/verdict thresholds are unchanged. Its native C runtime link was used on this hardware. No audio devices opened.
- No claim of all-preset worst-case coverage or 1:1 reference matching. Heavy FM remains substantially more expensive than the default benchmark patch.
- GUI images are offline native-coordinate previews with captured parameter labels, NOT camera photographs or screenshots of MPC's running display.
- The standard release installer/uninstaller still requires an actual install/play/each-page/Q-Link/save/reload/uninstall smoke test. The developer's installed private 0.1.2 was not replaced or uninstalled by this packaging pass.
- Public source is hosted at https://github.com/seangokrazy/SGK-Hyperion. Release/catalog status must be checked on GitHub; catalog maintainer acceptance is separate from submitting the request.
