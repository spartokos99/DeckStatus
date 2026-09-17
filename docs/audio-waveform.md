# Audio waveform overlay

Introduced in DeckStatus 1.3.1, this feature provides a Windows WASAPI audio source and a Canvas overlay. Audio capture is independent of the injected Rekordbox metadata bridge. It also works when the host is running in `--demo` mode; demo tracks never generate audio.

## Setup

1. Sign in as an administrator and open **Admin → Audio input** (`/admin#audio`).
2. Choose an **audio input** and click **Save**. Windows recording endpoints include microphones, line inputs and interfaces. **Output loopback** entries capture a playback endpoint's shared Windows mix.
3. Press **Start / switch source** when you want to capture. Choosing/saving an input does not activate it or switch a running capture. Optionally enable and save **Start audio capture automatically when DeckStatus starts** for future launches.
4. Open **Scene Components → Waveform**, choose a preset and customize the visualization. Copy its URL into an OBS browser source with the displayed width/height.
5. Stop capture with **Stop capture** or by exiting DeckStatus. Closing a browser tab leaves capture running.

There is one shared source for the server session. Visual URLs contain only appearance options; they do not activate microphones or select a source. The chosen endpoint ID/name and autostart flag persist in `DeckStatus.data/portal.json`. Autostart is off by default; when enabled it opens only that saved device at application startup, after HTTP sockets bind and before login. A missing device is reported in Admin without choosing a fallback. Stop leaves the saved device and autostart policy intact; disable and save autostart to prevent capture next time. Only administrators can save settings or start/switch/stop capture; remote requests also require Allow remote controls.

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

## HTTP API

All routes use the configured listeners, Host/Origin validation, authentication/scoped read keys, no-store headers and bounded requests. Administration and capture mutations require an administrator session; remote mutations additionally require Allow remote controls.

### `GET /api/audio/devices`

Returns `devices: [{id, name, kind}]` where `kind` is `input` or `loopback`, plus an `error` translation key or `null`. IDs are opaque Windows endpoint IDs. Names are rendered as text, never markup. Enumeration does not start capture.

### `GET /api/audio/state`

Returns `status` (`stopped`, `starting`, `capturing`, `error`), `error`, `deviceId`, `deviceName`, `sampleRate`, `sequence`, `sampleAgeMs`, `fresh`, `left` and `right`. Both channel arrays contain exactly 1,024 finite floats in −1…1. Mono is duplicated; multichannel capture uses the first two channels. When unavailable/stale, sample arrays contain zeroes. No packet received yet means `sampleAgeMs: null` and `fresh: false`.

### `GET/POST /api/admin/audio`

GET returns `{settings:{deviceId,deviceName,autoStart},state,controlError,canControl}`. The saved device and the currently active device may differ until Start / switch source is pressed. POST accepts one of these JSON commands:

```json
{"action":"save","deviceId":"<endpoint ID>","autoStart":false}
{"action":"start"}
{"action":"stop"}
```

Save atomically stores the selection/policy without opening a device. Start uses the saved input; Stop does not erase settings. Autostart errors appear as `controlError`; errors from an opened WASAPI worker appear in `state.error`. There is no automatic fallback or retry loop for an unavailable endpoint. Refresh the device list and start manually after reconnecting it. Operators and OBS keys cannot use this endpoint.

### `POST /api/audio/source`

Requires `Content-Type: application/json` and exactly one string field:

```json
{"deviceId":"<ID returned by /api/audio/devices>"}
```

This compatibility endpoint is now administrator-only. A valid non-empty ID is remembered and starts capture. An empty ID stops capture without clearing the saved selection or autostart option. Invalid schema/device ID returns 400; an unsupported content type returns 415. A successful request returns the capture state, which may still be `starting`; poll for actual activation or a device error. GET requests cannot change capture state. No endpoint controls Rekordbox playback.

## Implementation and validation

The capture worker owns its COM/WASAPI objects and uses shared mode. It supports PCM 8/16/24/32-bit and IEEE float32, including compatible extensible formats. It releases every acquired WASAPI buffer, handles silent/discontinuous packets, clamps samples and stops its audio client before releasing it. Source changes stop/join the previous worker before opening another.

- Six native CTest tests and both browser suites passed locally for 1.3.1. The current admin/persistence changes have separate coverage in `portal_access`, `browser_admin_audio_test.cjs`, `browser_portal_test.cjs` and `audio_startup_smoke.cjs`; see the validation log.
- Audio conversion tests cover signed PCM limits, stereo/mono, clipping, NaN, buffer bounds, silence and ring ordering.
- Default CTest never opens an audio device; it only enumerates endpoints and tests invalid selection/stop.
- The opt-in `audio_test.exe --loopback-smoke` successfully opened, stopped and reopened a local Windows output-loopback endpoint. It saves no audio files and selects no microphone.
- Synthetic browser tests cover all six render modes, FFT peak/amplitude, channels, gating, opacity with trails, configuration, EN/DE and failure states.
- No separate validation of microphone/interface capture, actual Rekordbox-to-loopback signal fidelity or an OBS waveform session has been performed.

**Rekordbox compatibility remains limited to the author's tested 7.2.18.0 Windows x64 executable.** This audio feature does not expand the injected bridge's supported builds.

Primary API references: [Microsoft: capturing a stream](https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream), [loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording), [GetMixFormat](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getmixformat).
