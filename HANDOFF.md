# DeckStatus project handoff

Updated on **2026-11-16** for the **v2.4.0 Windows release**. Rekordbox 7.2.19.0 has a separate exact profile and the owner reports a successful live test; 7.2.18.0 remains supported by its unchanged original and patched profiles. Preserve older archives and published tags. Read [AGENTS.md](AGENTS.md) first. This file records the state at handoff; check Git and the code for later changes.

## Current state

- **2.4.0 release package (2026-11-16):** `build/DeckStatus-2.4.0-win-x64.zip`, **93,865,217 bytes**, SHA-256 `cf673eb5e4b189a8671bfcc1811f699a3ea668e26a425c2d0ec261122b25e6fb`, with an adjacent `.zip.sha256` sidecar. Fresh extraction into a path with spaces passed portable EN/DE demo/idle-ProLink, resources and audio-startup smoke. All **451 files** match source/build, **33 dependency archives** match pinned hashes and **315 runtime files** match the original pinned runtime. Private data/network/update files are excluded. [Release URL](https://github.com/spartokos99/DeckStatus/releases/tag/v2.4.0). The owner requested publication of the accumulated detection/BPM changes as 2.4.0; preserve all previous tags/assets.
- **2.4.0 master detection:** both previous methods are replaced by `src/master_detector.h`, one ProlinkTools-style engine in both modes with 128 smart beats, 16 interruption beats, end-only off and ProLink On-Air gating on. Only the sampler advances it; HTTP reads cannot alter history. Already reported outgoing tracks cannot become fallback masters. Rekordbox infers transport from audited timeline positions and has no On-Air signal; the UI explains this limitation. Bounded ten-second recovery and display-only overlay caching preserve cards through brief source loss; a source-session marker prevents reuse after a server restart. Old store settings migrate atomically. Bridge IPC is version 4; ship matching host/DLL. See [detection](docs/master-detection.md). This is not new live hardware certification.
- **2.4.0 BPM display:** deck/master settings and independent scene layers select whole numbers, one or two decimal places. `bpmDecimals` is persisted and validated as 0–2; old `bpmInteger` URLs/presets remain readable (true → 0, false/missing → 2). Decimal modes retain trailing zeros with locale-specific separators for current/original BPM and master history.
- **2.4.0 validation:** full native/Java build, 12 native CTest passes (optional database integration skipped), all 18 browser suites plus HTTPS, EXE/DLL resources, audio/network smoke and the complete isolated updater suite passed. The local `build/Release` contains 2.4.0. See [validation](docs/validation.md). No production hardware or audio capture was started.
- **2.3.4 release package (2026-10-13):** `build/DeckStatus-2.3.4-win-x64.zip`, **93,855,656 bytes**, SHA-256 `80af4ac4bde92e85ff46beb26e3ece60e0c1de6be6c98840f971d5f06f925c8b`, with a `.zip.sha256` sidecar. Full x64 Release build, 11 native CTest passes (one optional database test skipped), ProLink helper tests, all 16 browser/application suites, fresh extraction and isolated demo smoke passed. The currently running `build/Release/DeckStatus.exe` was left untouched; use the release ZIP or isolated release build for v2.3.4. [Release URL](https://github.com/spartokos99/DeckStatus/releases/tag/v2.3.4).
- **2.3.3 release package (2026-09-26):** `build/DeckStatus-2.3.3-win-x64.zip`, **93,850,452 bytes**, SHA-256 `95c263fbbae5f89bba47a96c3c4183a1e147e2ffa9866146cbbaa86f9fe0ea15`, with an adjacent `.sha256` sidecar. [Release URL](https://github.com/spartokos99/DeckStatus/releases/tag/v2.3.3). A fresh extraction in a path with spaces passed portable EN/DE demo/idle-ProLink, audio-startup and resource checks. All **450 files** match source/build, **33 dependency archives** match pinned hashes and all **315 runtime files** remain unmodified. Private stores/network configuration/update jobs are excluded.
- Historical 2.3.3 validation: complete native/Java build, **11 native CTest passes** plus the optional database case passing separately against a synthetic collection, all **17 browser suites**, the HTTPS portal variant, resources, audio/network smoke and the complete isolated updater restart/rollback test. No production hardware was started; no task-owned app/helper remained. See the newer 2.4.0 entry for current results and limits.
- Rekordbox **7.2.19.0 original x64** has a separate per-build layout and 14 code guards. Its installed executable's exact PE identity, SHA-256 and every guard passed a read-only static check on 2026-10-13; the owner subsequently reported a successful live test. The 7.2.18.0 profiles remain unchanged. See `docs/rekordbox-7.2.19.md` and the latest validation entry.
- The owner's `build/Release/DeckStatus.exe` was running and locked during packaging. The final 2.3.3 binaries were built from matching isolated sources in `build/release-2.3.3-source/build/Release`; do not assume the original `build/Release` EXE contains this release. The running owner process was left active. The authoritative deliverable is the verified ZIP, extracted for checks into `build/release 2.3.3 verify 20260926-212419`.

- **2.3.3:** optional ProLink beat/On-Air master detection in `src/mix_detector.h` and Admin → Master detection, preserving default tempo-master/Rekordbox behavior. Track timeline designs add segment/ring/scrolling and overview analysis views. Local `.EXT` PWV3/PWV5 parsing lives in `src/track_waveform.h`; ArtworkResolver reads only collection-confined analysis, outside its metadata lock. Beat Link transfers bounded waveform packets to an eight-track native cache. Scoped current-track APIs and `web/track-timeline.js` keep arrays out of state/history polls. See [detection](docs/master-detection.md), [timelines](docs/track-timelines.md) and the latest [validation](docs/validation.md). The new method and real track waveforms still need live hardware/OBS validation.

- **2.3.2:** per-field font size/style/weight for deck/master overlays, editable Deck 1–4 on linked scene layers (preserved across preset saves), background stable-release checks and Admin → Updater. The updater stages uploaded/downloaded ZIPs, checks paths/versions/hashes, requires explicit install confirmation, backs up complete private data, restarts with the same arguments and rolls back normal failures. See [docs/updater.md](docs/updater.md). Preserve all user data and settings; no reset function was added.
- Previous release **2.3.2**: `DeckStatus-2.3.2-win-x64.zip` with an adjacent `.sha256` sidecar: [GitHub release](https://github.com/spartokos99/DeckStatus/releases/tag/v2.3.2). It includes all previously local 2.3.0/2.3.1 changes. The old 2.3.1 ZIP below does not include the updater and remains untouched. Do not move or overwrite existing published tags/assets.
- Previous local ZIP: `build/DeckStatus-2.3.2-win-x64.zip`, **93,824,345 bytes**, SHA-256 `c8d0359503b1f7672316a6d90c8af71560cb1821945b74820b8ae6aeced4f492`. Fresh extraction in a path with spaces passed portable EN/DE demo/idle-ProLink, audio-startup and resource checks. All **447 files** match source/build, **33 dependency archives** match pinned hashes, and **315 runtime files** remain unmodified. No private stores, network configuration, update jobs or downloaded test artifacts are packaged.
- Updater implementation: `src/updater.{h,cpp}`, `tools/DeckStatus.Update.ps1` (copied beside the EXE), `web/admin-updater.js`, explicit admin HTTP routes and navigation summary. Jobs/backups live in ignored `DeckStatus.update`. Automatic checks are off in demo mode or with `DECKSTATUS_NO_UPDATE_CHECK=1`; tests exercise manual checks with a fixture transport. Never test installation against the user's actual running copy; `tests/updater_test.cjs` builds an isolated complete package and starts only owned demo processes.
- Latest **2.3.2** validation: full native/Java build passed (10 native cases, optional artwork case skipped), all 16 browser suites plus the HTTPS portal variant, resource checks, audio-startup/network smoke and the complete isolated updater test. Real same-version restart and locked-DLL rollback preserved the private store/network byte for byte. Downloads use a fixture transport in tests. See the newest [validation entry](docs/validation.md). No production hardware was started and no task-owned app/helper remains running.

- Completed local **2.3.1** release: `build/DeckStatus-2.3.1-win-x64.zip`, **93,781,380 bytes**, SHA-256 `421d1ee8ea91910b4e73ac480742130b5924e66a5f7578ea5504d674a00e128d`. Adjacent `.zip.sha256` and `DeckStatus-2.3.1-ReleaseNotes.md` files are ready. A fresh extraction in `build/release 2.3.1 verification` passed portable smoke and the complete file audit: 444 source/build matches, 33 pinned dependency archives and 315 unchanged runtime files. Private stores/network settings, Rekordbox binaries and probe/test artifacts are excluded. Nothing was uploaded or committed. No task-owned bridge or application process remains running.
- The earlier 2.3.1 request was for a local ZIP only, adding support for the patched Rekordbox 7.2.18.0 EXE while preserving the original. The later explicit **2.3.2** release request authorizes publication of the accumulated changes; it does not authorize changing previous release assets. See CHANGELOG.md and docs/validation.md.
- The rejection was a changed PE SizeOfImage, not a runtime SHA check. New `src/rekordbox_profile.h` recognizes two audited SHA-256/size/PE identities sharing the existing layout and 14 loaded-code guards. Unknown variants fail closed. The compiled verifier accepts both actual files; read-only checks also confirm the running patched process's PE/guards. After the previous user-owned DeckStatus process exited, an isolated probe attached the final 2.3.1 DLL: patched profile selected, connected, 35 fresh samples, four loaded decks/four timelines. The probe unloaded its DLL through normal IPC shutdown; Rekordbox remained responsive and no bridge IPC/DLL was left. Original backup verification was static; no new tempo-control/UI comparison was performed. Full native/Java build passed (10 native cases, optional database case skipped), as did resource, EN/DE dashboard/history and staged portable checks. See validation.md for precise limits.
- Previous local release: `build/DeckStatus-2.3.0-win-x64.zip`, 93,777,267 bytes; SHA-256 `5785abc9a0f2b679450e5b988f529939525d8e513d15ab93a78fe93a316ff9b7`. Adjacent `.sha256` and `DeckStatus-2.3.0-ReleaseNotes.md` files are ready. Fresh extraction in a path with spaces passed portable smoke: all 444 package files match source/build, 33 dependency archives match pinned hashes and all 315 runtime files are unmodified. Private data/network settings are excluded. EXE/DLL resources are 2.3.0 / 2.3.0.0. All 15 browser suites, HTTPS portal, full native/Java build and audio/network smoke passed with the documented skips. Nothing was uploaded to GitHub.
- 2.3.0 includes extended deck/master metadata and styling, linked presets in scenes, explicit ProLink deck assignment, ignoring unrelated unsupported devices, and persisted automatic ProLink connection.
- Earlier local preview: `build/DeckStatus-components-preview-2026-09-22-win-x64.zip`, 93,776,544 bytes, SHA-256 `97f06f62d39bccd8e9620e669709a0193ca500661e158ea214573c1f5702a25d`. This retains the older 2.2.0 resource version and is superseded by the versioned 2.3.0 package.
- Prior 2.3.0 validation: full native/Java build, all 15 browser suites, direct/HTTPS portal tests and portable/audio-startup/network smoke passed. The optional local artwork database test and four unavailable LAN-bound native sub-checks were skipped, as recorded in docs/validation.md. Hardware was not started. ProLink connection fixes still need a live retest; the owner's report confirms discovery only.

- Previous public package: [DeckStatus 2.2.0](https://github.com/spartokos99/DeckStatus/releases/tag/v2.2.0), `DeckStatus-2.2.0-win-x64.zip`, 93,748,063 bytes; SHA-256 `8fdaa49baf82ac24022802099754551acb2c29e313935e6cade0ebca0f047202`. A checksum sidecar is included. The September 19 EXE, DLL and Java helper were reused byte for byte; the ZIP contains updated documentation. All 443 packaged files matched source/build, 33 dependency archives matched pinned hashes and 315 runtime files matched the pinned runtime. Fresh extraction passed portable and audio-startup smoke; see docs/validation.md for the complete results and limitations.
- Current version: **v2.4.0**. Repository [spartokos99/DeckStatus](https://github.com/spartokos99/DeckStatus), branch main. This release adds shared now-playing detection, the interruption cache and 0/1/2-decimal BPM display. Exact Rekordbox profiles, creative components, shared audio, Twitch automations, authenticated ratings and existing saved designs remain supported.
- The owner explicitly requested publication of the existing v2.2.0 build and documentation updates on 2026-09-22. Preserve the README structure when updating individual details.
- Published 2.3.4 and earlier packages used a four-second master hold. Version 2.4.0 replaces that behavior; do not use historical hold notes as the current specification.
- Uploaded images/GIFs migrate automatically from inline Base64 to content-addressed files in DeckStatus.data/media. Back up the complete data directory before upgrading; downgrade requires restoring that pre-upgrade backup. Preserve the migration, orphan cleanup and failed-save tests.
- All HTTP registrations require an explicit Access value. CMakeLists.txt alone defines the product version; generated native resources and tools/version.cjs consume it. Portal readers share a lock; password derivation and media I/O stay outside it.
- Full History remains publicly readable; new votes require a separate Twitch viewer session. Viewer tokens are memory-only, revalidated before voting, and never grant portal roles. Votes use hashed Twitch IDs with admin-only usernames; older anonymous votes are retained. See src/twitch_viewers.h and docs/twitch.md.
- Automations use 1–16 ordered actions with individual targets/durations and shared cooldowns. Legacy single-action stores migrate atomically. Stream → Automations is admin-only; Admin → Twitch holds Client ID/account linking. Split saves preserve unrelated settings and reject revision conflicts. Chat output is queued with bounds and send limits.
- Scene Components includes text, images/GIFs and audio FX. Media is persistent; Commons imports are opt-in. Shared audio configuration, endpoint persistence and opt-in launch capture live in Admin → Audio input. Loading designs/presets never starts capture.
- Scene iframe transparency depends on matching the iframe colour scheme to the embedded overlay document. Preserve the explicit transparent background and dark scheme in scene-shared.js.
- The native WinHTTP transport and production Twitch account permissions have not been validated with real Twitch accounts. The owner reported successful discovery of 3× CDJ-3000 + DJM-900NXS2 and correct unsupported classification of DJS-1000. The connection fix still needs a hardware retest; live ProLink metadata and OBS Studio remain unvalidated. Rekordbox 7.2.18.0 and 7.2.19.0 have separate exact profiles; the owner reports live success with both.
- The local DeckStatus.vcxproj additions for AGENTS.md/HANDOFF.md and untracked rb_inj.sln predate these changes. Preserve them; they are not part of the release commit. The CMake build is authoritative.
- A native OBS Studio plugin was discussed but has not been commissioned or implemented. Current integration uses Browser Sources.

## First steps on another PC

```powershell
git clone https://github.com/spartokos99/DeckStatus.git
cd DeckStatus
git status --short
git log -3 --oneline
```

Open the repository in the IDE. Install Windows x64 Visual Studio Desktop development with C++ and CMake, JDK 21+ (`javac` and `jar` on PATH), Node.js 22+ and Microsoft Edge. The previous machine used Visual Studio 2026 / MSVC 19.51 and Node.js 24.13.0. Build tool paths are machine-specific; `build.ps1` discovers Visual Studio's CMake when it is not on PATH.

The Java build verifies `prolink/dependencies.lock.json` and downloads the pinned dependencies/runtime when missing. First build needs network access. End users receive the bundled JRE and do not need Java installed separately.

Do not copy an old `build/` tree as the new machine's build configuration. It contains generated projects, cached absolute paths, test artifacts and downloaded tools. In particular, the old `build/publish-*.ps1` and package-audit scripts were local release helpers, **not tracked build tooling**; a fresh clone will not contain them. The `build/` directory in the current working copy was carried over from the project's earlier `C:\Users\Nico\RiderProjects\rb_inj` location; on 2026-09-19 its stale `CMakeCache.txt` still pointed at that source path and CMake refused to configure. Deleting only `build\CMakeCache.txt` and `build\CMakeFiles` fixed it and preserved the pinned ProLink downloads and the published release artifacts in the same folder. Leftover `*.dir\Release\` contents from that older generation still cause harmless MSB8028 warnings.

If moving the running application's data too, stop DeckStatus and privately transfer `DeckStatus.data` and `DeckStatus.network.json` from the actual deployment directory. Finish the initial password change before moving accounts: the temporary bootstrap password uses Windows DPAPI tied to the creator. Normal password hashes and persisted records are portable afterwards. Saved sessions expire at restart. Adjust the saved bind address to the new PC's interface. These private files and browser-local preferences are not transferred by Git.

## Product behavior

DeckStatus serves live DJ metadata and stream overlays from either the default Rekordbox source or an optional PRO DJ LINK source. Both feed the same dashboard, API and renderers.

- Four deck cards: title, artist, album, key, artwork, current/original BPM and track timelines.
- Deck overlays, master overlay with configurable history/scale/alignment and smooth transitions, and Windows-input audio waveform overlays with six styles.
- One server-wide now-playing detector (**Admin → Master detection**) uses playback timing, interruption allowance, optional end-only reporting and ProLink On Air. The state API, all overlays, dashboard and Full History share its confirmed result.
- Shared saved component presets and a monitor-sized scene editor. Inserted presets retain a presetId; saves atomically update linked designs without changing geometry, visibility or keys. Detach a layer for local edits. Unique unchanged older copies migrate automatically; ambiguous/customized copies remain independent. A saved scene uses one OBS Browser Source URL.
- New reusable text/image/FX components; bounded persistent media library and opt-in Wikimedia Commons search/import. Audio-reactive transforms work on every scene layer. One shared reaction loop per scene uses the Windows audio source configured in Admin (manual start or explicitly enabled launch autostart); no device starts when loading content.
- Accounts with administrator/operator roles, initial password change, user management and server-side authorization.
- Public Full History and aggregate ratings, with Twitch sign-in required for new 1–5-star votes. One vote per verified Twitch account/track; admins can see usernames and individual stars. Legacy anonymous votes remain. Ratings persist across sessions; played-track history does not.
- Configurable local/LAN access, optional public HTTPS domain behind a reverse proxy, and separately gated remote audio/ProLink controls.
- English default UI plus German translations. Navigation: Start, Stream (Scene editor, Automations, Full History, Scene Components dropdown), Connections and standalone Admin. Inactive source-mode functions remain visible but disabled.

## Source map

| Area | Main files | Responsibility |
|---|---|---|
| Startup | `src/deckstatus.cpp` | CLI, selected source, lifecycle and host startup |
| Injection/profile | `src/injector.cpp`, `src/deckstatus_bridge.cpp`, `src/rekordbox_profile.h`, `src/deckstatus_protocol.h`, `src/scanner.h` | DLL attachment, three exact executable identities across separate 7.2.18/7.2.19 layouts, bounded reads and IPC |
| Library/artwork | `src/artwork.cpp` | Read-only local library enrichment and bounded artwork access/cache |
| HTTP | `src/server.cpp`, `src/portal_http.h` | Fixed routes, source gates, authorization, request validation, cookies, local/domain listeners |
| Network | `src/network.cpp`, `src/network.h` | Validated persisted listener/domain options, interfaces and peer policy |
| Persistent portal | `src/portal.cpp`, `src/portal.h` | Users/passwords, sessions, ratings, presets, scenes, revision checks and OBS keys |
| Master detection | `src/master_detector.h`, `web/admin-master.js` | Shared now-playing episodes, timing, source recovery and settings |
| History | `src/master_history.h` | Shared master observations, overlay history and paginated full session history |
| Overlay timeline | `web/master-options.js`, `web/master-settings.html`, `web/overlay-shared.js`, `web/overlay.css` | Master timeline design, independent width and left/center/right alignment; browser regression in `tests/browser_track_timeline_test.cjs` |
| Windows audio | `src/audio_capture.cpp`, `src/audio_samples.h`, `src/audio_control.h`, `web/admin-audio.js` | WASAPI input/loopback, saved admin settings, opt-in startup and sample processing |
| Native ProLink host | `src/prolink.cpp` | Owned JVM process, bounded JSON IPC, freshness, artwork and restart handling |
| Java ProLink | `prolink/src/com/deckstatus/prolink/Main.java`, `DeviceSupport.java` in the same directory | Discovery/connection, exact model profiles, status/metadata adaptation |
| Shared web UI | `web/theme.css`, `web/navigation.js`, `web/auth.js`, `web/i18n.js`, `web/poll.js`, `web/locales/` | Design tokens, navigation, authentication, translations and refresh loops |
| Overlay links/designs | `web/broadcast.js`, `web/settings.js`, `web/master-options.js`, `web/overlay-shared.js`, `web/track-controls.js` | Scoped URLs, deck/master options and common rendering |
| Presets/scenes | `web/component-presets.js`, `web/scene-editor.js`, `web/scene.js`, `web/scene-shared.js` | Shared preset library, editing and live scene rendering |
| Creative components | `src/scene_components.h`, `web/creative-*.js`, `web/audio-reactivity.js`, `web/media-library.js` | Media validation, text/image/canvas FX, shared audio reactions and opt-in Commons imports |
| Waveforms | `web/waveform-settings.js`, `web/waveform.js`, `web/waveform-renderer.js`, `web/waveform-options.js` | Device controls, visualization options and canvas output |
| Build/resources | `CMakeLists.txt`, `build.ps1`, `prolink/build.ps1`, `src/deckstatus.rc` | Native/Java build, packaging inputs, version information and icon |

## Decisions that are easy to accidentally undo

### Rekordbox compatibility

The author's Rekordbox **7.2.18.0 Windows x64** installation and, per the owner's report, the original **7.2.19.0 Windows x64** installation have been live-tested successfully. Each exact build has its own identity, layout and loaded-code guards; the two 7.2.18.0 variants share their layout. The hash is computed once per attachment; unknown variants fail closed. The 7.2.18.0 patched process also passed a short new-DLL sampling/unload smoke test (not a full playback/UI comparison). IPC is version 3. The separate host enriches metadata through the local read-only database; Rekordbox DLLs are not distributed.

Read [the memory profile](docs/rekordbox-7.2.18.md), [metadata/artwork sources](docs/artwork-sources.md) and [the validation record](docs/validation.md). Original BPM/key come from metadata; current BPM comes from live deck state. The Rekordbox profile does not provide play/pause, fader position or a live transposed key.

### ProLink limitations and metadata policy

- Implemented profiles: CDJ-3000, CDJ-3000X and XDJ-AZ player endpoints; DJM-A9 and DJM-900NXS2 mixers. The owner reports discovery of 3× CDJ-3000 + DJM-900NXS2, but connection was blocked by an unrelated DJS-1000. The fix and full live data still need hardware verification. Synthetic packets are not firmware certification.
- XDJ-AZ requires **PRO DJ LINK → Connect to CDJ/XDJ/DJM**. Only announced endpoints are exposed; standalone four-deck/lighting mode is not implemented. Multiple player numbers may share one IP. Standalone USB 2 metadata is unsupported; unknown raw slot numbers remain distinct in track identities.
- One to four selected players map explicitly to distinct dashboard Decks 1–4; player numbers 5 and 6 are selectable. Mixers are automatic, not track decks. Unselected unsupported devices do not block connection. Duplicate selected numbers remain ambiguous and rejected. Saved player number/model/deck mappings live in portal.json; only ProLink startup calls configure(...,true) and maintain() for saved automatic connection. Manual Disconnect/Find devices pauses it; saving preferences alone does not start discovery.
- Metadata is requested directly through Beat Link 8.0.0 DBServer. DeviceSQL `export.pdb` IDs can resolve to unrelated OneLibrary/Device Library Plus tracks, so the old fallback is disabled for the entire ProLink session, including legacy/mixed networks.
- Omitting `CrateDigger.start()` is insufficient: Beat Link can initialize it indirectly through OpusProvider and register an automatic start listener. `Main.configureMetadata()` removes that hook through the public lifecycle API before networking. The Java regression test verifies this against the pinned dependency and preserves unrelated listeners.
- CDJ-3000X causes the virtual monitor to prefer an available standard player number (1–4) for metadata queries. No free number can mean missing metadata. Playback/loading/sync/tempo-control commands remain disabled.
- Missing metadata, artwork or position remains unknown. Streaming/cloud sources, shared-IP DBServer concurrency, firmware combinations and timelines need real device validation. Do not describe this as full standalone XDJ-AZ support.

See [docs/prolink.md](docs/prolink.md) for detailed setup, upstream references and boundaries.

### Domains, loopback and OBS links

- Default HTTP is `127.0.0.1:18740`. Saving network/domain options requires restart. Binding a specific LAN interface also opens loopback on the same port; the listeners share application state.
- Optional `publicDomain` accepts one normalized domain, not a URL/path. HTTPS terminates at Caddy or another proxy; DeckStatus's upstream remains HTTP. The proxy preserves original Host and Origin. Forwarding headers do not grant local privileges.
- Domain requests use remote permissions even if the proxy runs on localhost. Network configuration changes require a local admin using IP/localhost. Domain session/voter cookies are Secure; local HTTP remains usable.
- `/api/app.obsBaseUrl` supplies `http://127.0.0.1:<port>`. Displayed/copied/opened OBS links remain local even when settings are accessed through a domain. Embedded previews and their API calls remain same-origin. For OBS on another PC, the user replaces the copied origin while preserving path/options/key.
- Cross-site document navigation is allowed only for explicitly supported renderers with a matching scoped read key. It is not a blanket exception for admin/API requests.
- Windows HTTP rejection previously caused connection resets when a request body was unread. Preserve bounded body reading before rejecting POSTs so clients receive the intended HTTP error.

See [docs/network.md](docs/network.md). Do not solve a Host rejection by broadly disabling validation or trusting arbitrary forwarded headers.

### Master detection and interruption cache

`MasterDetector` in `src/master_detector.h` is applied in `src/deckstatus.cpp` **only by the sampler**, before `MasterHistory`. HTTP reads return its mutex-protected published snapshot, never advance it. Do not reintroduce the removed hold/method controls or invoke detection in a request callback. Settings are shared server-wide; they are not design options.

Rules that are easy to lose: elapsed playback time uses current BPM; short interruptions retain the start; already reported outgoing decks never become fallback candidates. Missing samples freeze timers and retain episode identity rather than manufacturing stop/unload transitions. Rekordbox transport is inferred from repeated plausible timeline movement; repeated `sampleTick` values are not pause evidence. No On-Air state is invented. The separate browser `TrackCache` retains cards for at most ten seconds, visibly marks them cached and hides timelines; it clears on known end/unload or 401/403. Static metadata stays tied to source track identity, never title. Settings migrate in the existing atomic portal transaction without resetting private data. See [master detection](docs/master-detection.md).

### Application theme and refresh loops

`web/theme.css` is the single source of colour, spacing, radius and type tokens, and it carries the base styles for headings, links, buttons, inputs and the `.panel`/`.setting` primitives. Every application page loads it first; the page-specific stylesheets only add layout. Do not reintroduce literal colours into the page stylesheets — that is exactly the drift the token layer replaced.

Renderer documents (`overlay.html`, `master-overlay.html`, `waveform.html`, `scene.html`, `creative.html`) deliberately do **not** load `theme.css`. Their appearance belongs to the user's saved overlay options, presets and scenes, and the master overlay's geometry is asserted with measured pixels in `browser_master_test.cjs`. Restyling them changes what viewers see on stream and invalidates saved designs.

`web/i18n.js` awaits only the language being displayed. Application pages (anything with a `header` or a `[data-language]` selector) prefetch the other language in the background so `setLanguage` can apply in the same task; renderer documents never fetch a second file. `diagnostic()` needs the English dictionary and builds its reverse index once instead of scanning every translation per render.

`web/poll.js` owns every application refresh loop: it pauses while the tab is hidden, resumes on `visibilitychange`, backs off after consecutive failures and never lets two runs overlap. The renderers keep their own fixed cadence because an OBS browser source is never usefully "hidden" and its interval is part of the on-air latency.

Static assets carry an ETag derived from the embedded body and are served with `Cache-Control: no-cache`, so a repeat visit costs a 304. The default header is `no-store`, and `Response::set_header` appends to a multimap, so the handler erases the default before replacing it. HTML documents get no validator: they are access-controlled and must not sit in a browser cache.

### Persistent data and access

Access levels live with the routes: `PortalServer::Get/Post/...` require an `Access` value, so a handler cannot be registered without a policy and unknown combinations cannot fall through to a permissive default. Renderer routes are `Keyed`, documents use the `*Page` levels so anonymous visitors are redirected instead of receiving 401, and `/api/auth/password` plus `/account/password` are the only routes reachable while a password change is pending.

The portal lock is a `shared_mutex`: readers (identity, overlay key scopes, presets, scenes, ratings) share it, writers hold it exclusively. PBKDF2 derivation for login, password changes and user edits happens outside the lock, and media file I/O never runs under it. Keep it that way; a single expensive operation under this lock stalls every overlay and API request.

The private store contains users, password hashes, votes, presets, scenes and scoped keys. Store writes are atomic, use revisions where appropriate and preserve prior data on failure. Invalid stores are not silently replaced. OBS keys authorize only their renderer/read routes; they cannot administer users or source controls. Public history must not expose unobserved library data or private settings. Preserve migration coverage and the last-admin protections.

### Creative components

- New types: `text`, `image`, `fx`; old preset types and source modes remain intact. Scene layers allow static `rotation` and audio reaction options. Text is literal, images reference a local SHA-256 asset ID, FX uses procedural canvas drawing.
- Media files live in `DeckStatus.data/media/`, one file per asset named by its SHA-256; `portal.json` keeps only metadata. Stores from 2.1.0 and older are migrated on the first start (payloads written out, `data` removed), so an older build no longer finds those images. Limits are unchanged: 8 MiB / 4096×4096 per file, 100 files and 32 MiB Base64-equivalent across the library. Original animated GIF bytes are retained. A failed save leaves no referenced file behind, and unreferenced files are removed at startup. Deletion rejects references in saved scenes/presets. Atomic persistence, migration and read scopes are covered by tests.
- Commons search/import happens only on explicit user action in the browser. Only `/components/image` allows the two Wikimedia origins through CSP. No external URLs/keys enter saved renderer options; imported assets render locally. Attribution/source metadata remains in the library.
- Media upload bodies can reach 12 MiB and authenticated updater chunks 4 MiB; other routes retain 64 KiB. Scene keys expose visible referenced assets only; standalone image keys can fetch any known media ID but not list the library. Text/image/FX keys can read samples, never control capture.
- New FX layers fill the canvas; after changing scene resolution use Fill scene. Flash triggers on rising threshold crossings with cooldown; fog/motion uses attack/release. Stale and stalled buffers clear effects. Analysis is approximate (1,024-sample FFT), not beat-grid detection.
- New stores and existing stores receive media storage and three extra read keys. Existing credentials and OBS keys are retained. No hardware testing was added.

## Validation commands

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
node tests/browser_master_test.cjs
node tests/browser_track_design_test.cjs
node tests/browser_waveform_test.cjs
node tests/browser_dashboard_history_test.cjs
node tests/browser_prolink_test.cjs
node tests/browser_network_test.cjs
node tests/browser_portal_test.cjs
node tests/browser_creative_test.cjs
node tests/browser_creative_portal_test.cjs
node tests/browser_admin_audio_test.cjs
node tests/browser_admin_master_test.cjs
node tests/browser_track_cache_test.cjs
node tests/browser_updater_test.cjs
node tests/updater_test.cjs
node tests/browser_twitch_layers_test.cjs
node tests/browser_viewer_ratings_test.cjs
node tests/browser_scene_audio_actions_test.cjs
node tests/browser_scene_transparency_test.cjs
node tests/audio_startup_smoke.cjs
node tests/network_smoke.cjs
powershell -NoProfile -ExecutionPolicy Bypass -File tests/resource_test.ps1
```

For the HTTPS regression suite, in a dedicated PowerShell session:

```powershell
$env:DECKSTATUS_TEST_PROXY = '1'
node tests/browser_portal_test.cjs
Remove-Item Env:DECKSTATUS_TEST_PROXY
```

The proxy fixture uses an ephemeral certificate and a disposable browser profile, not a change to system trust. `DECKSTATUS_TEST_ROOT` can point the portal suite at an extracted package. `node tests/portable_smoke.cjs 'C:/path/to/extracted-package'` checks the portable application in isolated demo/idle-ProLink modes. Leave `DECKSTATUS_TEST_DISCOVERY` unset for ordinary checks.

Native tests include `twitch_automation`, `portal_access`, `network_access`, `prolink_backend`, `master_detection`, `master_history`, `artwork_database`, `http_server`, `track_waveform`, `scanner_boundaries`, `rekordbox_profiles`, `injection_lifecycle` and `audio_capture`. Injection targets an owned fixture; the default audio test does not open capture. The optional database test needs CMake's `REKORDBOX_TEST_EXE` set to an appropriate local executable; a new machine without it may skip that case. Report what actually ran. The opt-in `build/Release/rekordbox_profile_test.exe --verify '<exe>' '<backup>'` checks actual files without executing them.

For v2.0.2, all nine native cases, Java model/UTF-8 pipe checks and all six browser suites passed. The portal suite also passed through HTTPS. Network smoke verified simultaneous local/LAN access with a configured domain. EXE/DLL versions and nine icon sizes were checked. A freshly extracted ZIP, including a path with spaces, passed portable smoke; 426 files matched source/build output, 33 dependency archives matched pinned hashes and all 315 bundled runtime files were unmodified. These are dated results, not proof that a future checkout or a new PC has been tested.

## Release workflow

When the owner requests another release:

1. Inspect Git status and the intended diff. Preserve unrelated files. Confirm branch/remote and the next version; do not move an existing published tag.
2. Update the version in `CMakeLists.txt` only: `src/version.h.in` feeds the EXE/DLL resources and `/api/app`, and `tools/version.cjs` feeds the tests and screenshot tooling. Update current README/Wiki links separately, and preserve historical changelog/validation entries.
3. Write English release/upgrade notes and keep compatibility limitations explicit. Run the appropriate full release checks above. Record actual results in `docs/validation.md`.
4. Run CMake install into a fresh staging directory after the full build, for example `cmake --install build --config Release --prefix build/package-NEXT` from a shell where CMake is available. Include EXE, bridge DLL, `DeckStatus.Update.ps1`, `web`, `prolink` with its runtime/dependencies/licenses, documentation and notices. Never package the entire build directory, deployed private data or `DeckStatus.update` jobs/backups.
5. Create a ZIP and SHA-256 sidecar. Extract it to a fresh location, compare contents against source/build and dependency hashes, and run portable smoke. Check that no private/generated files entered the Git diff or package.
6. Commit the intended source changes, create the requested tag and push without forcing unrelated history. Create a GitHub draft release, upload the ZIP/checksum, verify uploaded sizes/digests and then publish. Keep repository-document links in GitHub release notes absolute and version-pinned.
7. Verify the public tag/release/assets. Report the release URL and completed checks. Keep credentials in the configured credential manager or CLI authentication; never print or commit them.

The local one-off publishing scripts used for v2.0.2 are not part of the clone. Recreate the workflow using available authenticated tools rather than expecting those ignored files to exist. Never move or overwrite previously published release tags/assets.

## Continuing with a new assistant

Suggested first prompt:

> Read AGENTS.md and HANDOFF.md, inspect the current Git status, and summarize the project state and compatibility boundaries in German. Version 2.4.0 is the current release; preserve prior archives and published tags. Do not start production injection, device discovery or audio capture during orientation. Then continue with my next request.

Keep this file current with future decisions and completed work. It transfers project context, not the previous chat session or its tool permissions.
