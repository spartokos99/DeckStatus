# Audio waveform overlay

DeckStatus 1.3.1 adds a Windows WASAPI audio source and a Canvas overlay. Audio capture is independent of the injected Rekordbox metadata bridge. It also works when the host is running in `--demo` mode; demo tracks never generate audio.

## Setup

1. Open `/waveform/settings` on the running local DeckStatus server.
2. Choose an **audio input** first. Windows recording endpoints include microphones, line inputs and interfaces. **Output loopback** entries capture a playback endpoint's shared Windows mix.
3. Press **Start / switch source**. Selecting a device alone does not activate it.
4. Choose a preset and customize the visualization. Copy its URL into an OBS browser source with the displayed width/height.
5. Stop capture with **Stop capture** or by exiting DeckStatus. Closing a browser tab leaves capture running.

There is one shared source for the server session. Visual URLs contain only appearance options; they do not activate microphones or select a source. No source is restored on application restart. Other local clients can explicitly select/stop the source via the API.

Windows microphone privacy settings must permit desktop applications to access a recording device. An unplugged device produces an error; refresh the device list and select a source again. No fallback microphone is opened automatically.

Loopback captures the whole shared mix of its device, including other applications. It does not isolate Rekordbox or follow MASTER. ASIO and exclusive-mode routing can bypass this mix. For those configurations, route the desired signal to a Windows-visible input or a suitable shared playback endpoint. The app neither installs an audio driver nor changes Windows/Rekordbox routing.

## Visual controls

| Group | Controls |
|---|---|
| Presets | Mint line, Neon spectrum, Sunset mirror, Minimal white, Orbit, Signal ribbon |
| Shape | Line, filled waveform, spectrum bars, mirrored spectrum, radial spectrum, amplitude history |
| Size | Width 160–2560 px, height 80–1440 px |
| Colour | Primary/secondary colour, gradient, background colour and 0–100% opacity |
| Response | 0.1–10× gain, smoothing, amplitude noise gate, hide on silence |
| Channels | Stereo mix, left or right; first stereo pair of multichannel inputs |
| Spectrum | Minimum/maximum frequency, clamped to the selected device's Nyquist frequency |
| Detail | Line width, 8–160 bars/spokes, bar gap/rounding, glow and trails |
| Presentation | Grid, centre line, 2–30 seconds of sampled amplitude history, 30/60 FPS limit |

Line width affects waveform lines and radial spokes; bar options affect the spectrum; frequency limits affect spectral modes; history duration affects the amplitude-history mode. A 1,024-point Hann-windowed FFT computes the spectrum. Sensitivity and the gate apply before spectral analysis. The line/filled modes show a short rolling sample window, not a precomputed track waveform. Their horizontal scale depends on the audio sample rate.

The host retains only the most recent **1,024 samples per channel** in memory. Browser polling waits 40 ms after each response. History is a series of sampled RMS amplitudes; it is not a continuous audio recording and may miss short transients between samples. Rendering interpolates amplitudes for smooth visual changes; it does not synthesize playback progress. Freshness expires after 250 ms on the server and after 300 ms without an update in the renderer. Silence, stopped capture, errors and stale input clear the signal; a configured background can remain visible.

Visual settings are stored under `deckstatus.waveform.options`. Generated URLs normalize and bound each supported option. After changing a design, replace its URL in OBS. The audio stream is never played by the overlay, stored in files or uploaded.

## Local API

All routes use the existing loopback listener, Host/Origin validation, no-store headers and bounded requests.

### `GET /api/audio/devices`

Returns `devices: [{id, name, kind}]` where `kind` is `input` or `loopback`, plus an `error` translation key or `null`. IDs are opaque Windows endpoint IDs. Names are rendered as text, never markup. Enumeration does not start capture.

### `GET /api/audio/state`

Returns `status` (`stopped`, `starting`, `capturing`, `error`), `error`, `deviceId`, `deviceName`, `sampleRate`, `sequence`, `sampleAgeMs`, `fresh`, `left` and `right`. Both channel arrays contain exactly 1,024 finite floats in −1…1. Mono is duplicated; multichannel capture uses the first two channels. When unavailable/stale, sample arrays contain zeroes. No packet received yet means `sampleAgeMs: null` and `fresh: false`.

### `POST /api/audio/source`

Requires `Content-Type: application/json` and exactly one string field:

```json
{"deviceId":"<ID returned by /api/audio/devices>"}
```

An empty ID stops capture. Invalid schema/device ID returns 400; an unsupported content type returns 415. A successful request returns the capture state, which may still be `starting`; poll for actual activation or a device error. GET requests cannot change capture state. No endpoint controls Rekordbox playback.

## Implementation and validation

The capture worker owns its COM/WASAPI objects and uses shared mode. It supports PCM 8/16/24/32-bit and IEEE float32, including compatible extensible formats. It releases every acquired WASAPI buffer, handles silent/discontinuous packets, clamps samples and stops its audio client before releasing it. Source changes stop/join the previous worker before opening another.

- Six native CTest tests and both browser suites passed locally for 1.3.1.
- Audio conversion tests cover signed PCM limits, stereo/mono, clipping, NaN, buffer bounds, silence and ring ordering.
- Default CTest never opens an audio device; it only enumerates endpoints and tests invalid selection/stop.
- The opt-in `audio_test.exe --loopback-smoke` successfully opened, stopped and reopened a local Windows output-loopback endpoint. It saves no audio files and selects no microphone.
- Synthetic browser tests cover all six render modes, FFT peak/amplitude, channels, gating, opacity with trails, configuration, EN/DE and failure states.
- No separate validation of microphone/interface capture, actual Rekordbox-to-loopback signal fidelity or an OBS waveform session has been performed.

**Rekordbox compatibility remains limited to the author's tested 7.2.18.0 Windows x64 executable.** This audio feature does not expand the injected bridge's supported builds.

Primary API references: [Microsoft: capturing a stream](https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream), [loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording), [GetMixFormat](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getmixformat).
