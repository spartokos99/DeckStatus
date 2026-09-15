# Changelog

## 1.4.0 · 2026-09-15

### 🔌 ProLink

- Add explicit `--mode prolink` / `--prolink` startup and `Start-ProLink.cmd`. The default remains Rekordbox, with its original process/database options and injection profile.
- Add a CDJ-3000 / DJM-A9 network adapter using Beat Link 8.0.0, with a bundled, checksum-pinned Temurin runtime. No Java installation is needed for the portable package.
- Add ProLink setup: passive discovery, selected-player connection, mapping up to four players (including numbers 5/6), disconnect, interface/device details and Playing/Sync/On-Air indicators.
- Feed the existing dashboard, overlays, timing and full session MASTER history from either source. Preserve distinct identities for different media sources and helper restarts. Clear stale/disconnected live data.
- Keep all navigation functions visible in Monitor, Stream overlays and Connections groups. Show the current mode, disable inactive connection links and reject inactive APIs. Add a Rekordbox diagnostics page.
- Keep Windows audio waveforms available in both modes. Mixer faders/EQ/FX and analysed track-waveform overlays are not implemented. No device playback, loading, sync or tempo-control commands are exposed.

### 🧪 Validation and compatibility

- Seven native tests, including ProLink IPC/artwork/lifecycle/freshness/restart and HTTP mode gates; Java model tests with synthetic CDJ packets; four browser suites including mobile navigation and EN/DE.
- Verify the actual bundled helper's UTF-8 pipe output under Windows, including a non-UTF-8 console default, and check the portable application's mode gates, discovery/ports-busy diagnostics and shutdown without connecting to players or recording audio.
- **ProLink is experimental and has not been tested on real CDJ-3000/DJM-A9 hardware.** Rekordbox-exported USB media is the primary target. Other models, firmware combinations, streaming and Device Library Plus-only media are not validated.
- **Rekordbox mode remains tested only with the author's 7.2.18.0 Windows x64 installation.** No additional Rekordbox executable is supported by the existing bridge.
- Full release includes dependency source JARs/POMs and runtime legal notices. Public screenshots use English and labelled synthetic examples.

### 📦 Upgrade

Extract the complete `DeckStatus-1.4.0-win-x64.zip` into a new directory. Keep `web`, `prolink` and `DeckStatusBridge.dll` beside the EXE. Run `DeckStatus.exe` for Rekordbox or `Start-ProLink.cmd` for ProLink. Open ProLink setup and explicitly discover/connect your devices. Previous overlay URLs and designs remain usable.


## 1.3.2 · 2026-09-15

- Add Waveform and Full History links to the dashboard and consistent navigation across settings pages.
- Use the EXE icon's SVG artwork as the logo throughout the web interface.
- Show track position/duration timelines on all four dashboard decks, including missing and stale states.
- Add complete session MASTER history beyond the overlay's 50-track limit: artwork, timestamps, deck/key, captured/original BPM, late metadata and stable 100-row pagination.
- Keep the last observed track after disconnect without a live badge. History resets on app restart; MASTER changes do not prove audible playback.
- Regenerate every README screenshot in English with English synthetic data; add a reproducible generator that checks the language before saving.
- Add native archive/cursor/API tests and a dashboard/history browser suite. Keep web assets current on incremental builds.
- Replace the native demo's German track names with English examples: **Night Drive "Live"** by **Orbit & Friends** and **First Light** by **Studio North**, independent of the UI language.
- Give all six public screenshots explicit `-en.png` filenames. Check English track titles and artists before capture and keep German regression screenshots separate.
- Update app and bridge version resources and the portable Windows x64 package to **1.3.2**.

**Validation:** All six native CTest tests, all three browser suites, English screenshot generation checks and the EXE icon/version resource check passed. Browser data and audio signals are synthetic; production injection and audio capture were not started for this update.

**Compatibility:** Tested only with the author's **Rekordbox 7.2.18.0 installation on Windows x64**. No additional Rekordbox version or build is validated by this release.

**Install / upgrade:** Download `DeckStatus-1.3.2-win-x64.zip`, extract the whole archive, stop the previous instance and run `DeckStatus.exe`. Keep `DeckStatusBridge.dll` and the complete `web` folder beside it.

## 1.3.1 · 2026-09-15

### ✨ Added

- A simple mint DeckStatus **D + audio signal** app icon, embedded in the Windows EXE at nine resolutions from 16 to 256 px; matching browser favicon and editable SVG source.
- A dedicated **waveform overlay** and settings page at `/waveform` and `/waveform/settings`.
- Audio source selection as the first setting: Windows recording inputs and output loopback, with explicit start/switch/stop and a live input meter.
- Six presets and six visualizations: line, filled waveform, spectrum bars, mirrored spectrum, radial spectrum and amplitude history.
- Colours/gradient, transparency, dimensions, gain, smoothing, gate, channel selection, frequency range, line/bar styling, glow, trails, grid, centre line, history window and 30/60 FPS limit.
- Live preview, saved visual configuration, generated OBS URLs and complete English/German waveform UI.
- Local audio device/state endpoints plus the explicit JSON audio-source selection endpoint.

### 🔧 Behaviour

- Audio capture runs in the host through WASAPI, starts disabled and never saves recordings or plays sound.
- One selected source is shared by all waveform overlays; visual URLs do not open audio devices. Presets only change appearance.
- Stopped, stale, missing and disconnected audio clears its signal. No microphone fallback or simulated audio is used.
- Waveform preview transparency and trail/background composition are checked by rendering tests.
- App and bridge version resources are **1.3.1**. Existing deck/master overlay URLs and settings remain supported.

### 🧪 Validation

- All **six native CTest tests** passed, including audio conversion/bounds/silence and HTTP source-control checks.
- Existing deck/master browser suite and the new synthetic waveform browser suite passed.
- Native endpoint enumeration and an opt-in live WASAPI output-loopback open/stop/restart smoke test passed.
- Compiled EXE icon/version resources and the portable Windows package were checked.

### ⚠️ Compatibility

**Tested only with the author's Rekordbox 7.2.18.0 installation on Windows x64.** No other Rekordbox build is validated or newly supported by this release.

Output loopback shows the shared Windows mix of the selected device, not a MASTER-isolated signal. ASIO/exclusive output may bypass it. Microphones may require Windows privacy permission. No separate microphone/interface, actual Rekordbox-to-loopback fidelity or OBS waveform session validation has been performed. Browser rendering tests use synthetic signals.

### 📦 Install / upgrade

Download `DeckStatus-1.3.1-win-x64.zip`, extract the whole archive, stop the previous instance and run `DeckStatus.exe`. Keep `DeckStatusBridge.dll` and the complete `web` folder beside it. Open `http://127.0.0.1:18740/waveform/settings`, choose a source and press Start. Do not overwrite an actively running version.
