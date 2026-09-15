# Changelog

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
