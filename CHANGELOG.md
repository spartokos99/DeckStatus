# Changelog

## 2.1.0 · 2026-09-17

- Require Twitch viewer sign-in for new track ratings, with one vote per Twitch account and track across browsers. Preserve older anonymous votes.
- Add an administrator-only voter detail modal and a permission-aware ratings link on Full History. Keep viewer sessions separate from DeckStatus administrator and streamer/bot credentials.
- Allow 1–16 ordered actions per automation with individual targets and durations, duplication/reordering and automatic migration of existing rules. Queue multiple chat replies under the send limit.
- Move rule editing, testing and logs to **Stream → Automations**; keep Client ID and account linking in **Admin → Twitch**, with independent saves and unchanged administrator permissions.
- Add timed **Enable audio reactive** / **Disable audio reactive** automation actions for all scene component types. Restore saved reaction settings on expiry without changing capture state or saved designs.
- Improve scene layers with type icons, selected/hidden states, direct visibility toggles and ordering buttons; keep hidden components and their settings.
- Add Admin Twitch integration with Public-client device login, optional bot account, DPAPI-protected credentials and persistent opt-in connection settings.
- Add reward, chat command/text, raid and stream-status rules for chat replies and live scene visibility/text/geometry changes, with durations, cooldowns, roles, templates, a dry-run tester and runtime reset.
- Add an isolated OAuth/EventSub/Helix fixture and browser coverage. Live Twitch validation is pending; see the [setup guide](docs/twitch.md). README and technical guides now describe the 2.1.0 workflows.
- Fix opaque backgrounds around master/deck and waveform components in rendered scenes by matching the iframe colour scheme to the embedded overlay document. Preserve configured component backgrounds.
- Move shared audio input selection/start/stop to Admin; waveform settings now configure only the visualization.
- Persist the selected device and an optional capture-on-launch setting, off by default. Retain settings on Stop and report unavailable saved devices without fallback.
- Require administrator permissions for audio settings and capture mutations, retaining the remote-control gate.

- Add reusable static text, image/animated GIF and full-scene audio FX components, with standalone OBS links and shared presets.
- Add a persistent upload library and optional Wikimedia Commons search/import with retained source/credit metadata.
- Add audio-driven scale, position, rotation and opacity for scene layers, plus configurable fog and threshold-triggered flash effects.
- Share the existing explicitly started Windows audio source; stale/stalled samples return effects to rest. Loading scenes or presets never starts capture.
- Preserve existing accounts, source modes and OBS keys during media/key migration. Add native and browser coverage and an English [component guide](docs/scene-components.md).

### Upgrade and compatibility

- Stop DeckStatus and back up **DeckStatus.data** and **DeckStatus.network.json** before replacing application files with the complete ZIP. Existing accounts, keys, presets, scenes, media and ratings are preserved. Legacy one-action automations upgrade automatically; older anonymous ratings remain in totals without fabricated Twitch identities.
- Audio input controls now live under **Admin → Audio input**. Capture-on-start is opt-in; loading a scene never starts capture. Twitch account setup stays under **Admin → Twitch**; rule editing moved to **Stream → Automations**.
- New ratings require Twitch viewer sign-in. Viewer sessions expire on restart; votes persist. Streamer/bot credentials use Windows DPAPI and require relinking after moving to a different PC/Windows profile.
- Default mode is still Rekordbox. Only the author's **Rekordbox 7.2.18.0 Windows x64** installation has been live-tested. ProLink hardware, production Twitch account authorization and OBS Studio integration remain unverified in live use. Automated checks use synthetic fixtures; see [validation](docs/validation.md).

## 2.0.2 · 2026-09-16

- Add experimental CDJ-3000X, DJM-900NXS2 and XDJ-AZ (PRO DJ LINK mode) device profiles, setup guidance and synthetic packet/browser coverage. No live hardware validation is claimed.
- Request ProLink metadata directly through DBServer; remove the legacy DeviceSQL fallback to prevent incorrect OneLibrary/Device Library Plus metadata. Preserve distinct unknown media-slot identities.
- Translate all documentation under `docs/` into English while retaining English/German application translations.
- Add an optional public HTTPS domain in Network settings, persisted across restarts with backward compatibility for existing configurations.
- Support Caddy HTTP upstreams with original Host/Origin headers and Secure session/rating cookies. Apply remote permissions to domain requests even when the proxy runs locally.
- Keep localhost available alongside a selected network interface. Copied deck/master/waveform/scene URLs use `127.0.0.1` with the active port; embedded previews stay on the current origin.
- Add validation and HTTPS browser tests for login, password changes, ratings, presets, scenes, OBS rendering and persistence. Document Caddy setup in the network guide and wiki.

### Compatibility and upgrade

- Live-tested only with the author's **Rekordbox 7.2.18.0 Windows x64 installation**. No ProLink model or firmware combination has been tested on real hardware.
- XDJ-AZ requires **PRO DJ LINK → Connect to CDJ/XDJ/DJM**. Only announced player endpoints are supported; standalone four-deck mode and its USB 2 metadata are not supported.
- ProLink metadata now requires a successful direct DBServer query. The old DeviceSQL export.pdb fallback is disabled for all ProLink devices to prevent incorrect OneLibrary/Device Library Plus matches. If the query is unavailable, metadata, artwork and possibly timelines remain unknown. See the [ProLink guide](docs/prolink.md).
- Stop DeckStatus and preserve **DeckStatus.data** and **DeckStatus.network.json** before replacing the application files with the complete 2.0.2 ZIP. Existing accounts, ratings, presets, scenes and keys remain valid. Rekordbox remains the default startup mode.
- To use a reverse proxy, save its hostname under **Connections → Network → Public domain**, then restart DeckStatus. Keep the original Host/Origin headers. Local access through localhost/127.0.0.1 and generated local OBS links remain available.

### Validation

- Release build, all nine native CTest cases and the Java ProLink model/UTF-8 pipe tests passed.
- All six browser suites passed, including real-EXE authentication, presets, scenes, ratings, persistence and HTTPS reverse-proxy checks with Secure cookies and local OBS links.
- Network smoke checks passed for the saved interface/domain configuration, simultaneous LAN/localhost access, both source modes and CLI overrides. EXE/DLL versions are 2.0.2 / 2.0.2.0; all nine EXE icon sizes remain embedded.
- Production Rekordbox injection, ProLink device discovery/connections and audio capture remained off during release validation.

## 2.0.1 · 2026-09-15

### Scenes, accounts and audience feedback

- Add a persistent, shared library of named deck, master and waveform presets, with load/save/update/delete, revision conflicts and migration of existing portal stores.
- Insert saved presets directly into scenes as independent layers, retaining their settings and names. Keep optional default components and existing per-layer customization.
- Group Deck overlays, Master overlay and Waveform in a Scene Components dropdown. Rename navigation groups to Start and Stream, move Scene editor and Full History into Stream, and make Admin standalone. Support keyboard, mobile and EN/DE navigation.
- Keep the current mode visible on deck/master settings pages and refresh the English screenshots and technical guide.
- Add sign-in, a random initial administrator password, mandatory first password change, administrator/operator roles and user management. Password changes, user edits, logout and restart revoke sessions.
- Make Full History public, with editable 1–5-star browser ratings and persistent per-track averages, counts and distributions in Admin.
- Add persistent monitor-sized scenes with deck/master/waveform layers, drag/resize, exact geometry, stacking, visibility, opacity, presets and live updates to one OBS URL. Detect conflicting editor saves.
- Protect OBS renderers with scoped, revocable read keys. Persist users, ratings, presets and scenes in `DeckStatus.data`, support `--data-dir`, and keep private data out of packages/Git.
- Replace the README with a concise English guide and provide a standalone GitHub Wiki page. Keep English/German application translations.
- Add shared Network settings for local-only access, all IPv4 interfaces or one adapter, HTTP port and optional remote audio/ProLink controls. The initial listener remains localhost; enabling LAN access leaves remote source controls off by default.
- Persist validated settings in `DeckStatus.network.json` with atomic replacement; show active/saved values, restart notices and server URLs. Add `--bind`, `--network-config` and `--allow-remote-control` startup overrides.
- Preserve Host/Origin checks for LAN requests and restrict network configuration changes to local administrators on the DeckStatus PC. Disable remote audio/ProLink controls according to network policy while retaining authenticated overlay customization.
- Extend automated tests for account permissions, persistence, data migration, preset reuse, scene rendering, ratings, navigation and network configuration. Refresh all public screenshots in English.

### Upgrade

1. Stop DeckStatus. Back up `DeckStatus.data` and `DeckStatus.network.json` if you have used a preview build; preserve them when upgrading.
2. Extract the complete `DeckStatus-2.0.1-win-x64.zip`. Keep the EXE, DLL, `web` and `prolink` folders together. Existing browser preferences remain available.
3. Start `DeckStatus.exe` for Rekordbox or `Start-ProLink.cmd` for ProLink. A new store creates an `admin` account with a random temporary password printed in the console; change it after the first login. Preview users can keep their existing credentials.
4. **When upgrading from v1.4.0, generate new OBS URLs after signing in.** Overlays now require a scoped read key. Existing saved scene/preset data and keys from preview builds are preserved.

Full History remains public at `/history`; all dashboard and configuration pages require sign-in. Played-track history resets on restart, while ratings, presets and scenes persist. For LAN access, use a trusted network: the built-in HTTP listener is not encrypted.

### Compatibility and validation

- **Live-tested only with the author's Rekordbox 7.2.18.0 Windows x64 installation.** Default Rekordbox source behavior and its version-specific profile are unchanged.
- **ProLink remains experimental and has not been tested on real CDJ-3000 / DJM-A9 hardware.** Automated tests do not establish additional device compatibility.
- Validation covers nine native tests, Java ProLink model tests, six browser suites, authenticated demo/idle-ProLink package checks, embedded EXE icons/version and package/dependency hashes. Tests use isolated stores and synthetic data; production injection and audio capture remain off.

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
