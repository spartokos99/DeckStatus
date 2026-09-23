# Changelog

## 2.3.2 · 2026-09-23

- Add individual font size (8–200 px), normal/italic/oblique style and weight (100–900) to deck/master text elements. Overrides persist in URLs, presets and linked scenes; clearing an override restores the shared style.
- Allow each scene deck layer to select Deck 1–4 while retaining its preset link. Saving or updating the preset changes its design without replacing that layer's deck assignment.
- Show newer stable GitHub releases in the navigation and console. Check in the background at startup/every six hours, with manual checks under Admin → Updater.
- Add administrator ZIP upload and verified GitHub download, separate preparation/confirmed installation, complete data backups, restart and rollback on normal replacement/startup failures. Preserve users, ratings, presets, scenes, media, network settings and OBS keys. See [update operation and recovery](docs/updater.md).
- Includes the previously local 2.3.0 overlay/ProLink improvements and 2.3.1 exact executable profiles documented below. This is the first public release of those changes after 2.2.0.

### Upgrade and compatibility

- From versions without the updater, stop DeckStatus, back up the complete **DeckStatus.data** directory and **DeckStatus.network.json**, and extract the full 2.3.2 package while preserving those files. Include **DeckStatus.Update.ps1** beside the EXE. Future updates can be prepared under **Admin → Updater**; installation always requires confirmation.
- Existing users, ratings, scenes, presets, media and OBS keys remain valid. Login sessions and played-track history reset on restart. Keep retained update backups private. See [update operation and recovery](docs/updater.md) for limits and manual recovery.
- Live Rekordbox validation remains limited to the author's **7.2.18.0 Windows x64** installation and the two exact audited executable variants. Other hashes/versions remain unsupported. ProLink is experimental; the connection changes still require live hardware validation. Production Twitch authorization and OBS Studio validation remain pending.

## 2.3.1 · 2026-09-23

- Recognize the owner's audited patched Rekordbox 7.2.18.0 executable alongside the original file. The patch changes the PE image size and entry point, which caused the previous bridge to report it as unsupported; the sampled memory layout and all 14 code guards are unchanged.
- Select between two explicit executable profiles using PE properties and full-file SHA-256/size verification. Both share the existing address layout, loaded-code checks and per-sample pointer/type validation. Unknown patches and other versions remain unsupported. Hash the executable once per attachment, not during sampling.
- Report the selected variant when connected and distinguish unsupported PE fingerprints, file identity failures and mismatching loaded-code RVAs.
- Add native regression coverage for both profiles, altered/unreadable guards and file/hash mismatches, plus an opt-in read-only verifier for the two real files. See [profile identities](docs/rekordbox-7.2.18.md) and [validation](docs/validation.md).

### Upgrade and compatibility

- Stop DeckStatus, back up **DeckStatus.data** and **DeckStatus.network.json**, and replace the complete application package, including **DeckStatusBridge.dll**. Keep the existing data and network files. No preset, scene, account or OBS URL migration is needed from 2.3.0.
- Compatibility remains limited to the two documented Windows x64 7.2.18.0 files. A future patch with a different hash requires independent review; there is no user-editable address override or version-only bypass.
- Includes all 2.3.0 overlay, linked-preset and ProLink improvements below. ProLink, Twitch and OBS live-validation limits are unchanged. This release is prepared locally; GitHub publication is deferred.

## 2.3.0 · 2026-09-23

- Extend master/deck overlays with label, separate BPM and BPM (Current) switches, whole-number BPM, missing-data hiding, additional local fonts and independent master-history fields. Preserve legacy BPM URLs.
- Add cover placement on the left/top/right, round rotating covers and automatic content-height sizing. Add per-field colours/backgrounds/fonts/margins, content alignment, scrolling text and expanding containers, including scenes.
- Keep inserted presets linked to their scene layers. Updates propagate atomically without replacing OBS URLs or layer placement; layers can be detached/relinked, and deleted presets preserve their last design. Migrate only unique unchanged legacy copies.
- Ignore unrelated unsupported ProLink devices when connecting selected players. Add explicit deck assignment, persistent selection and automatic connection on ProLink startup once the saved devices appear. Manual disconnect pauses automatic connection. Rekordbox startup remains unchanged.
- The owner reports discovery of 3× CDJ-3000 + DJM-900NXS2 and a correctly unsupported DJS-1000. The connection fix still needs live hardware verification; automated coverage uses isolated fixtures.

### Upgrade and compatibility

- Stop DeckStatus and back up the complete **DeckStatus.data** directory and **DeckStatus.network.json** before extracting the full package over the application files. Preserve accounts, ratings, media and existing OBS keys; restore the backup to downgrade.
- Older unchanged scene layers link automatically only when their name, type and settings match one unique preset. Customized or ambiguous layers stay independent; use the source-preset selector to link them without reinserting. Future preset edits update linked designs while preserving placement and visibility.
- Old URLs with one BPM switch retain both original and current values. New designs select **BPM** and **BPM (Current)** independently. Additional fonts use Windows system fonts; no font download is required.
- In ProLink mode, connecting saves player/deck assignments. Automatic launch connection waits for all saved player numbers and model names; no saved selection means no automatic networking. **Save for next start** does not start discovery. Manual disconnect pauses automatic connection until Connect or restart.
- Default startup remains Rekordbox, with live compatibility limited to the author's **Rekordbox 7.2.18.0 Windows x64** installation. ProLink connection/data, production Twitch authorization and OBS Studio integration still need live validation. See [validation](docs/validation.md).

## 2.2.0 · 2026-09-22

- Add a server-wide **master hold time**: a new deck must stay the source's tempo master for a configurable period before DeckStatus publishes the handover. Brief changes no longer flip the master overlay, deck badges or session history.
- Configure it under **Admin → Master detection**, 0–30 seconds, default 4 seconds; 0 restores the previous immediate switch. Administrators only, subject to the existing remote-control policy.
- Apply the filter once, between the source and every consumer, so `/api/state`, the overlays, the dashboard and Full History always report the same confirmed master in both Rekordbox and ProLink mode.
- Delay only a change away from an already confirmed master. The first master after a start or a reconnect, a different track on the confirmed master deck, and a replacement for a deck that lost its track are all published without delay. Disconnected or stale states still publish no master. This filters tempo-master observations; it does not detect audible playback.
- Persist the value in `portal.json` as `masterSettings.holdMs`; it is read at startup and applies to the running server as soon as it is saved. Existing stores use the default until it is changed.

- Refresh the application interface: a shared `web/theme.css` defines the colour, spacing, radius and type tokens that every page now uses, so the dashboard, settings, admin, history and scene editor share one visual language instead of eleven hand-written palettes. Layouts are denser and fit more on a laptop screen.
- Leave every renderer untouched. Overlay, waveform, scene and component documents keep their own styling, so saved presets, scenes and OBS browser sources look exactly as before.
- Load only the language actually being displayed. Renderer documents now fetch one translation file instead of two; application pages prefetch the second in the background so switching stays instant.
- Add `web/poll.js`: refresh loops pause while a tab is hidden, back off after failures and can no longer overlap. Device state on the Admin page is only requested while the Audio tab is open, and the ProLink/Rekordbox pages no longer stack requests behind a slow reply.
- Serve public scripts, styles, icons and translations with a validator, so a repeat visit revalidates with a 304 instead of transferring the file again. HTML documents remain uncacheable.
- Rebuild the dashboard only where values actually changed and keep the OBS links out of the 2 Hz sample loop.
- Merge `twitch.css` into `portal.css` and `network-settings.css` into `connection.css`; most pages now load fewer stylesheets than before despite the added theme layer.
- Store uploaded images and GIFs as content-addressed files in `DeckStatus.data/media/`, migrating inline media automatically. Saving ratings, scenes or presets no longer rewrites image bytes.
- Allow concurrent portal readers and perform password hashing and media file I/O outside the store lock, reducing contention for API and overlay requests.
- Require an explicit access level for every HTTP route, preserving public history, Twitch-only voting, administrator permissions and scoped OBS keys.
- Derive native/API/resource versions and test expectations from `CMakeLists.txt` as the single version source.

### Upgrade and compatibility

- Stop DeckStatus and back up **DeckStatus.data** and **DeckStatus.network.json** before replacing application files with the complete ZIP. Accounts, ratings, presets, scenes, media and OBS keys are preserved.
- On first start, inline images/GIFs from older stores migrate into **DeckStatus.data/media/**. Back up and transfer the entire data directory, not only `portal.json`. Older builds cannot read the migrated media store; to downgrade, restore the complete pre-upgrade backup while DeckStatus is stopped.
- The master hold time defaults to **4 seconds** in both source modes. Set it to 0 under **Admin → Master detection** for immediate handovers. The first master after startup or reconnect still appears immediately.
- Saved overlay designs, presets, scenes and OBS browser source URLs are unchanged. The renderer documents were deliberately left untouched, so what your viewers see is identical apart from the delayed master switch.
- The application pages look different: one shared theme, denser layout. No saved setting controls this, and nothing needs to be reconfigured.
- Browsers may hold the previous stylesheets after the upgrade, because static assets are now revalidated rather than never stored. Reload once with Ctrl+F5 if a settings page looks broken.
- Default mode is still Rekordbox. Only the author's **Rekordbox 7.2.18.0 Windows x64** installation has been live-tested. ProLink hardware, production Twitch account authorization and OBS Studio integration remain unverified in live use. The master hold filter has only been exercised against synthetic state, never against an actual tempo-master handover on hardware. See [validation](docs/validation.md).

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
