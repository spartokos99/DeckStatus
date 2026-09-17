# Validation log

This is a historical record. Test counts, behavior and protocol versions describe the revision tested in each entry; later entries may supersede them. Live Rekordbox validation remains limited to the author's **7.2.18.0 Windows x64 installation**. ProLink hardware has not been live-tested.

## DeckStatus 2.1.0 release validation — 2026-09-17

- Full `build.ps1` passed: all ten native CTest cases, including the configured read-only artwork database case, plus Java device-model, Unicode/Windows-pipe and disabled DeviceSQL-auto-start tests. Pinned dependencies/runtime archives verified. EXE and bridge DLL report 2.1.0 / 2.1.0.0; the EXE contains all nine icon sizes.
- Passed browser suites: master/deck overlays, waveform, dashboard/history, ProLink, network settings, real-EXE portal, creative renderers, creative portal, Admin audio, Twitch/layers, viewer ratings, runtime audio actions and scene transparency. Network/startup smoke and the HTTPS portal variant also passed (16 browser/smoke runs in total).
- The master animation test now explicitly selects normal motion before separately testing reduced motion; the host Windows preference previously made its animation assertion fail while the app behaved correctly. Navigation and waveform tests now reflect the Automations entry and Admin-owned capture controls. The shared browser harness waits for a readable, complete DevTools port file to handle a transient Windows startup lock.
- Source/package candidates, JavaScript syntax, translation-key parity and local documentation links checked. README headings/structure are preserved. Release notes describe upgrade persistence and Twitch login changes. Unrelated IDE files are excluded from the release commit.
- These checks do not extend hardware compatibility. Only the author's Rekordbox 7.2.18.0 installation has been live-tested; no production DJ attachment/discovery, physical audio capture, real Twitch account or OBS Studio session was started for release validation.

The earlier entries below describe development snapshots now included in 2.1.0.

## Twitch viewer ratings — 2026-09-17 (development)

- Release build and targeted CTest cases `twitch_automation`, `portal_access`, `network_access` and `http_server` passed. A deterministic viewer transport checks device-code timing, zero requested scopes, client/account identity validation, token revocation, session isolation, logout, Client ID changes and secret redaction. No real Twitch account or OAuth token was used.
- The native HTTP portal fixture completes viewer sign-in with a fake Twitch transport, verifies HttpOnly/SameSite cookie handling, casts a vote, reads its own rating and rejects voting after logout. Viewer sessions cannot access portal metadata or admin details. Anonymous, operator and administrator tests cover the ratings-detail endpoint and history-link permissions; even administrators need a separate Twitch viewer session to vote.
- Persistence tests cover one vote per Twitch ID across browser/name changes, individual viewer details, legacy anonymous counts and aggregate preservation across restart. Public aggregate/history responses never contain other voters' names; usernames are not accepted in vote submissions.
- `browser_viewer_ratings_test.cjs` passed: sign-in code, completion, vote/update, logout, unconfigured state, permission-aware ratings link, modal/keyboard close, safe text, legacy labels, EN/DE and mobile. Actual-EXE portal tests passed locally and behind the isolated HTTPS proxy; anonymous voting now correctly returns 401. Dashboard/history and automation/layer browser regressions passed.
- README.md remains unchanged. No DJ attachment/discovery, physical audio capture, OBS Studio or real Twitch connection was started. Production Twitch device authorization with the empty scope list remains subject to live-account validation.

## Multiple automation actions and separate editor — 2026-09-17 (unreleased)

- Native Release build passed without compiler warnings. Targeted CTest cases `twitch_automation`, `portal_access`, `network_access` and `http_server` passed. Coverage includes legacy migration and failed-save preservation, action order and dry-run simulation, independent expiry, shared cooldown, continuing after a missing target, action-count limits, split saves/conflicts, administrator-only routes and remote-control policy.
- The deterministic Twitch transport delivered both chat actions from one rule while retaining the two-second send interval. It checks that automation saves preserve the Client ID and connection saves preserve rules. No live chat messages were sent.
- `browser_twitch_layers_test.cjs` passed for separate connection/automation pages, multiple actions, reordering, duplication/removal, individual durations, save/reload, draft retention, dry-run output, EN/DE, mobile layout and remote read-only controls. Admin audio and actual-EXE portal browser regressions also passed.
- Locale keys, JavaScript syntax and diff checks passed. README.md remains unchanged. No production DJ connection, physical capture or real Twitch account was used. Live Twitch/OBS validation remains outstanding.

## Timed audio-reactivity actions — 2026-09-17 (unreleased)

- Native Release build passed. Targeted CTest cases `twitch_automation`, `portal_access` and `http_server` passed. The action tests cover all six component types, saved true/false/absent flags, dry runs, opposite-action timer replacement, expiry, indefinite duration, reset, scoped audio read access and persisted rules. Saved designs are not rewritten.
- `browser_twitch_layers_test.cjs` passed with both new actions: duration controls, hidden irrelevant value fields, save/reload and English/German labels. `browser_scene_audio_actions_test.cjs` verifies actual transforms turning on, off and back on in the rendered scene for all six component types, using synthetic samples and zero capture mutations.
- Translation parity, JavaScript syntax, local documentation links and diff checks passed. README.md is unchanged. No real Twitch account, OBS Studio, DJ hardware or audio device was started for this change.

## Twitch automation and scene layer controls — 2026-09-16 (unreleased)

- The native Release build and all ten CTest cases passed, including the new `twitch_automation` case. The existing portal/network tests cover anonymous/operator rejection, CSRF/Origin checks and remote-control gates on the integration endpoint. Existing native DJ/audio checks remain synthetic/isolated as before.
- The Twitch fixture validates rule matching, roles, command boundaries, duplicate deliveries, cooldowns, timer extension/expiry, scene-revision invalidation, runtime reset, UTF-8 truncation and literal template substitution. It also exercises device authorization, token validation/refresh, reward listing, all five EventSub subscriptions, chat output/echo suppression, account unlink, DPAPI storage, API secret redaction, persistence and failed-save preservation through a deterministic injected transport. It never contacts Twitch or sends an actual chat message.
- `browser_twitch_layers_test.cjs` passed: layer selection/order/visibility and save, hidden iframe deferral, Twitch rule CRUD, 60-second defaults, device-code UI, dry-run results, preserved drafts during polling, German/English labels, mobile layout and remote read-only controls.
- The actual-EXE portal and creative-component browser suites, admin audio browser suite and composited scene transparency regression passed. Default audio remains stopped in these fixtures; no Rekordbox attachment, ProLink discovery or physical capture was started incidentally.
- The production WinHTTP TLS/WebSocket transport and live Twitch channel permissions have not been validated with a real account. OBS Studio was not launched. Java/ProLink code and pinned runtime are unchanged. README.md remains identical to the owner's GitHub commit `df71604`; no new release or push was requested.

## Scene iframe transparency — 2026-09-16 (unreleased)

- Reproduced opaque Chromium iframe backgrounds in the rendered `/scene` page: child overlays use `color-scheme: dark`, while their iframe elements inherited the scene's different scheme. Transparent CSS alone did not prevent the browser's opaque default canvas.
- Scene iframe elements now explicitly use the same dark colour scheme and a transparent CSS background. This also works when an editor's surrounding theme differs; configured card/canvas backgrounds remain unchanged.
- `browser_scene_transparency_test.cjs` failed on the old renderer with alpha 255 in an empty master-overlay region. It passes after the fix by checking actual composited PNG pixels: transparent master/deck/waveform regions under light/dark preferences, opaque intended card fill, a coloured scene showing through, and the shared editor renderer. Tests use synthetic metadata and stopped audio.
- The native executable is unchanged; updated web assets are copied into the local build. Actual OBS Studio was not launched for this check. README.md remains identical to the owner's GitHub commit.

## Admin audio settings and startup policy — 2026-09-16 (unreleased)

- Fetched and fast-forwarded the owner's GitHub README commit `df71604`; README.md remains identical to that commit. The previous local README edits were backed up under ignored build output.
- The native Release build and all nine CTest cases passed. New callback-based lifecycle tests verify defaults off, persistence, opt-in startup, manual stop retaining the policy/device, missing endpoints without fallback, and failed-save preservation. Operator and anonymous requests to audio administration are rejected; the compatibility source mutation is also admin-only.
- The synthetic admin browser suite passed: separate Save/Start/Stop, draft preservation during polling, retained settings on reload, missing-device selection, remote-control restrictions, EN/DE, mobile layout and outage recovery. The updated waveform suite passed with no source controls or capture commands on that page.
- The real-EXE portal browser suite passed locally, including saving an enumerated endpoint without opening it and retaining the selection with capture stopped after restart. Creative-component preset/upload/scene/browser checks also passed against the updated EXE.
- `audio_startup_smoke.cjs` passed against an isolated real EXE: a deliberately nonexistent saved endpoint with autostart enabled produced the expected error without fallback; disabling autostart persisted and the next restart stayed stopped without an error. Successful capture activation is covered using a fake backend, not a newly opened physical audio device.
- The HTTPS portal regression encountered a test initialization race at the first password form. The harness now awaits the authentication module before submission. After temporary approval-review capacity errors cleared, the HTTPS suite passed, including the admin audio remote-control denial, original Host/Origin handling, Secure cookies and local OBS links.
- The audio-admin preview ZIP contains 437 verified files. Its freshly extracted copy passed portable smoke in demo/idle-ProLink modes and the missing-device audio-startup regression. No private runtime store or network configuration is packaged.
- No production injection, discovery or physical audio capture was started. The Java helper was unchanged and not rebuilt.

## Creative components — 2026-09-16 (unreleased)

- Native Release build with `build.ps1 -SkipProLink` passed all nine CTest cases. The Java helper was unchanged and was not rebuilt for this feature.
- Extended `portal_access` coverage checks media deduplication/original bytes, invalid formats/Base64, image dimensions and decoded-size limits, missing/in-use references, reaction-option validation, visible-layer read scopes, failed-write preservation, restart persistence and additive migration without replacing existing data or keys.
- All six existing browser suites passed: master/deck, waveform, dashboard/history, ProLink/navigation, network and real-EXE portal.
- `browser_creative_test.cjs` passed with synthetic samples: RMS/frequency bands, attack/release, stale and stalled buffers, scale/position/rotation/opacity, fog output, flash rising edges/cooldown/decay, safe literal text, UTF-8 truncation and retained legacy iframe instances. Disabled reactions send no sample polls; renderers send no capture commands.
- `browser_creative_portal_test.cjs` passed against an isolated real demo EXE: first-login restrictions, text/image/FX preset save/load, an original two-frame GIF larger than the ordinary API body limit, unchanged GIF bytes, scoped anonymous image/scene rendering, media-list protection, in-use deletion rejection, normal request-size enforcement, full-canvas FX placement, EN/DE, mobile layout and persistence through an EXE restart. Capture remained stopped.
- Commons result filtering, plain-text attribution and credential omission were checked using a deterministic mocked API response. Live Commons search/download availability, licence suitability of individual results, OBS Studio/CEF rendering and sustained live-audio performance were not independently validated.
- Settings, scene and synthetic fog screenshots were visually inspected. English documentation describes setup, limits, key scopes and explicit audio capture. No production injection, DJ-device discovery or audio capture was started. No new release/version was published.

## Build and automated tests — September 14, 2026

Windows x64, MSVC 19.51 / Visual Studio 2026, C++20, Release build with the static C++ runtime. All five CTest cases passed:

- `artwork_database`: metadata joins, missing rows and relationships, SQL NULL, UTF-8 boundaries, artwork paths, path confinement and an unchanged test database. Uses the installed sqlite3.dll and a dedicated temporary database.
- `http_server`: JSON, Unicode, pages, methods, Host/Origin checks, artwork association during track changes, malformed/duplicate track IDs, missing artwork, health states, occupied ports and clean shutdown.
- `scanner_boundaries`: invalid memory ranges, page boundaries, UTF-8, PE boundaries and ambiguous signatures.
- `injection_lifecycle`: DLL loading into an owned test process, IPC, rejection of an incompatible EXE fingerprint, duplicate attachment and reattachment.
- `master_history`: master changes, the same track on another deck, unknown/interrupted master states, returning tracks, late metadata, frozen BPM, the 50-entry history limit, artwork access and eviction during a request, and stale sampling.

The dashboard and existing deck overlay also passed JavaScript syntax and DOM checks covering safe text output, missing values, deck selection, demo labeling, connection loss and retries for failed artwork requests.

The new master overlay was rendered by `tests/browser_master_test.cjs` in a real headless Edge browser with an isolated profile and synthetic HTTP fixture. Checks covered previews, saved settings, generated OBS URLs, history count, album visibility, title-only rendering with the other five fields hidden, actual Web Animations transitions, card reuse, rapid changes before animations finish, BPM updates without restarting animations, safe text, missing artwork, disconnection and reduced motion. `build/test-artifacts/master-settings.png` and `master-overlay.png` were visually inspected. No separate OBS test is claimed.

## Live Rekordbox 7.2.18.0

Additional browser checks covered history scaling, alignment and BPM display: actual card/artwork dimensions at 0.65×, limits of 0.20× and 1.00×, and list spacing. Left, center and right alignment were measured against both the master card and the browser window. An active transition from full master size to 0.40× history size was checked; rapid changes were also tested at 0.20× with right alignment. Settings and URL parameters survived reload. Both overlays displayed distinct current/original BPM, live tempo changes and missing original BPM correctly. `master-settings-scaled.png`, `master-overlay-scaled-right.png` and `deck-overlay-bpm-mobile.png` were visually inspected. These changes required no native data-access modifications.

The DLL was loaded into the running installation at `D:\Programs\rekordbox 7.2.18`. The HTTP API reported `connected`, `demo: false`, version `7.2.18.0` and fresh measurements (under 200 ms in the observed request). The library was opened read-only.

Initially, empty decks had no stale metadata. After loading four tracks, every deck returned valid track IDs, title, artist, album, key and BPM, with `metadataAvailable: true`.

| Deck | BPM at request time | Key | Artwork |
|---|---:|---|---|
| 1 | 174 | 9A | JPEG, HTTP 200 |
| 2 | 174 | 7B | JPEG, HTTP 200 |
| 3 | 175 | 9A | JPEG, HTTP 200 |
| 4 | 174 | 7A | JPEG, HTTP 200 |

`/api/health` and `/overlay?deck=1` returned HTTP 200. Empty-to-loaded deck changes were detected without restarting the bridge. After the bridge stopped, `DeckStatusBridge.dll` was no longer loaded in Rekordbox; Rekordbox continued running.

Library values and artwork responses were checked directly through the API. Manual pitch movement, streaming-service tracks, export mode, other Rekordbox versions and extended DJ operation were not separately live-tested. Stored key and original BPM come from the library; current deck BPM comes from the verified `@BPM` object.

## Live master overlay

The extended bridge using IPC version 2 detected **Deck 1 → Deck 4 → Deck 1 → Deck 4** in the same running Rekordbox instance. `/api/master` then contained the current track “PRVLG (Original Mix)” by Blend (174 BPM, 7A, album “Chrome”) and three correctly ordered history entries, including “1873” by Data 3. Returning tracks received new `entryId` values. Both artwork images returned HTTP 200 through history URLs with MIME type `image/jpeg` (152249 and 145012 bytes).

An additional read-only memory query confirmed master-device vtable RVA `0x03B85620`, the name `Master`, and cache values **1, 1, 1, 0**, matching API master Deck 4. The three new code checks were also verified directly against the EXE. An earlier Rider debugger attempt could not bind the source breakpoint without Release debug symbols and produced no reliable field values. The debugger was detached, the owned breakpoint removed, and Rekordbox continued running. Direct reads and HTTP responses confirmed the master behavior.

## Deck settings, timeline and languages

Browser coverage additionally included independent settings for four decks, applying settings to every deck, generated OBS URLs, English by default, German/English switching on the dashboard and settings pages, and persisted language choice. Rendering was checked with a light preset, 32 px type, transparent background, stacked artwork and hidden labels. `deck-settings.png` and `master-timeline.png` were visually inspected.

Synthetic timeline checks covered current position and total duration, 50% progress, negative lead-in, clamping to 0–100%, missing data without an invented zero, recovery, stationary positions and track changes. Only the current master card has a visible timeline; history cards do not. Scaling, alignment, both BPM values and animation checks continued to pass. HTTP tests covered all new modules, CSS and translation files, including MIME types and the fixed route list.

Console checks covered English default help, German help (`--lang de --help`) and rejection of an invalid language. The web interface and console use the same translation files.

The bridge with **IPC version 3** was loaded into the running Rekordbox instance after completely unloading the previous DLL. Rekordbox continued running. The API reported `connected`, `demo: false`, version `7.2.18.0`, master Deck 2 and fresh measurements (172 ms in the recorded request).

| Deck | Position (ms) | Duration (ms) |
|---|---:|---:|
| 1 | 221722 | 295550 |
| 2 | 68335 | 292414 |
| 3 | 52840 | 271526 |
| 4 | 181631 | 181631 |

Independent read-only memory queries confirmed the `@CurrentTime`/`@TotalTime` names, vtable `0x03B85620` and exactly these values for all eight devices. `/api/master.current` returned Deck 2's position and duration. Values survived library metadata enrichment. The five additional code checks had already been verified against the installed EXE build.

An additional Edge run against the real server confirmed the master timeline (1:08 / 4:52), loaded master artwork, deck settings preview and German connection status after switching language. Screenshots: `master-timeline-live.png` and `deck-settings-live.png`. In a separately started demo server, native API position increased from 0 to 657 ms with a total duration of 240000 ms; German diagnostics were also checked.

Live checks used existing stationary positions. Negative lead-in, seeking and pause behavior were checked with synthetic API data in the browser. No separate manual play/seek operation in Rekordbox or OBS test is claimed.

## Publication preparation — September 15, 2026

The Release build and all five native tests ran again. An intermittent Windows disconnect was found for rejected POST requests: early rejection could close the socket before the request body was fully read. At this revision, write methods were handled by normal handlers that only rejected requests after httplib had read the body, bounded to 1024 bytes.

Additional checks covered POST, PUT, PATCH, DELETE, OPTIONS and repeated POST/GET sequences over one connection. After the fix, all five tests and 20 consecutive HTTP test runs passed. At that time, this added no write API and did not change native Rekordbox access.

The public README preview used the existing browser fixture server and synthetic tracks. Library data, Rekordbox binaries, local IDE settings and build output were excluded from Git. Publication preparation did not extend version compatibility.

## Renaming to DeckStatus — September 15, 2026

The application, CMake targets, solution/project file, C++ namespace, IPC objects and Windows file information were renamed DeckStatus. Runtime files became `DeckStatus.exe` and `DeckStatusBridge.dll`; product names were checked in both compiled files. IPC remained version 3 with a dedicated DeckStatus signature and object names.

The build and all five native tests passed with the new names, including loading, duplicate attachment and unloading the DLL in an isolated test process. Browser checks also covered migration of old language/deck/master settings into `deckstatus.*`, with existing new values taking precedence.

Dashboard, settings and three overlay designs were captured again using synthetic tracks for the README. Renaming did not extend the Rekordbox memory profile to other builds; live validation remained limited to 7.2.18.0.

## DeckStatus 1.3.1 — September 15, 2026

- Added the host's WASAPI audio source, separate waveform renderer and settings page. Existing Rekordbox profile checks were unchanged.
- The MSVC Release build and all six native CTest cases passed, including the configured database test. The existing deck/master browser suite also passed.
- The new waveform browser suite passed: audio input first, no automatic capture, start/source change/stop, six actual canvas renderings, FFT frequency/amplitude, stereo/channel selection, noise floor, limits, language/persistence, constant background transparency with trails, silence, errors and stale data. All browser audio signals were synthetic.
- The default audio test enumerated devices and checked PCM/float conversion, NaN/clipping, mono/stereo, window ordering, silence and invalid packets. CTest opened no audio source.
- A separate opt-in hardware test, `audio_test.exe --loopback-smoke`, opened an existing Windows output loopback twice, obtained its mix format, stopped it and reopened it. No microphone was opened and no audio file was saved. This tested stream lifecycle, not fidelity of a Rekordbox signal.
- Compiled EXE resources passed checks for product version 1.3.1, file version 1.3.1.0 and nine embedded icon sizes (16, 20, 24, 32, 40, 48, 64, 128, 256 px).
- Preview and icon were visually inspected. Public screenshots used labeled synthetic data.
- Microphone/interface capture, Rekordbox-to-loopback signal transfer and a dedicated OBS waveform session remained unvalidated.
- Production Rekordbox injection was not started for this work. The only live-validated Rekordbox version remained the author's **7.2.18.0, Windows x64** installation.

## Dashboard and Full History — September 15, 2026 (then-unreleased source)

- Waveform and Full History became accessible from the dashboard. Dashboard, overlay settings, waveform settings and history used the same SVG icon template as the EXE.
- Four dashboard timelines were checked for position/duration, clamped progress, negative lead-in, unavailable and stale values.
- Full session history for master-track changes was added alongside the unchanged 50-track overlay window. Tests covered repeats, old artwork/metadata, stable cursors, page limits and invalid API parameters.
- All six native CTest cases and all three browser suites passed. The new suite used 137 synthetic history entries to test navigation, logo, four timelines, pagination, further track changes, artwork, XSS prevention, EN/DE, failure and empty sessions.
- All six public README screenshots were regenerated with English UI and English synthetic data and visually inspected. The generator also checked embedded preview language.
- Incremental builds copied changed web files without relinking the EXE; source/build hashes matched.
- History remained session-local. MASTER changes were not proof of audible playback. Production injection was not started and no additional Rekordbox version was validated.

## DeckStatus 1.3.2 — September 15, 2026

- The Release build and all six native CTest cases passed, including the configured database test. All three browser suites passed: deck/master, waveform, and dashboard/Full History.
- Native demo tracks used English titles/artists: `Night Drive "Live"` by `Orbit & Friends` and `First Light` by `Studio North`. Language settings continued to translate UI and diagnostics, not track metadata.
- All six public README images were regenerated as `*-en.png`. The generator checked English labels and track titles/artists, including embedded previews. Waveform and dashboard images received additional visual inspection. German waveform browser screenshots were written only to `build/test-artifacts` under a separate name.
- EXE resources passed checks for product version `1.3.2`, file version `1.3.2.0` and nine icon sizes from 16 to 256 px.
- The earlier dashboard/Full History additions were included in 1.3.2. Production injection and audio capture were not started. Live validation remained limited to the author's **Rekordbox 7.2.18.0 on Windows x64**.

## DeckStatus 1.4.0 — September 15, 2026

- Added opt-in ProLink mode, its device setup page, shared grouped navigation and server-side mode gates. Default startup remained Rekordbox mode; the injection profile and database/process options were unchanged.
- Seven native CTest cases passed. The new ProLink test used only an owned helper with no network access: command validation, missing runtime, JSON/artwork transfer, stale data, process termination, restart with separate track IDs and owned-process shutdown. HTTP checks covered both modes, inactive endpoints, JSON validation and Origin restrictions.
- Java model tests passed with the bundled Temurin runtime: synthetic CDJ status packets, flags/BPM, freshness limits, distinct player/media/slot identities, remounts, unknown values and Unicode JSON. An actual helper additionally tested UTF-8 output despite a Windows-1252 stdout default and shutdown on closed input. These model tests opened no network sockets.
- The complete application test checked EN/DE demo, unchanged default mode, ProLink runtime, mode/API gates, web routes and disabled audio capture. A real discovery attempt correctly returned `prolinkPortsBusy` on this PC because the required ports were occupied; no device was connected. This exposed a Windows Java stdout encoding issue, fixed with an explicit UTF-8 interface.
- Four browser suites passed. ProLink added coverage for discovery without automatic connection, player selection, connect/disconnect, safe device names, mode display/gates, shared audio functions, mobile layout and EN/DE. Existing overlay/waveform/history suites continued to pass.
- Seven public README screenshots were regenerated with English UI and synthetic data. ProLink setup and dashboard navigation were visually inspected.
- EXE resources passed checks for product version 1.4.0, file version 1.4.0.0 and nine embedded icon sizes.
- **No live CDJ-3000 or DJM-A9 test was performed.** ProLink remained experimental; firmware combinations and real USB/streaming workflows were unvalidated. Production Rekordbox injection and audio capture remained off. Rekordbox live validation was still limited to the author's 7.2.18.0 installation.

## Configurable network access — September 15, 2026 (then-unreleased)

- Eight native tests passed. `network_access` covered local defaults, valid/invalid addresses and ports, saved settings, unchanged running configuration, CLI precedence, preservation of the previous file after failed saves, and TCP peer rules. The HTTP test checked a wildcard listener through its actual addressed loopback IP, Host/Origin defenses and configuration endpoints.
- Connecting a local test client from a LAN source address to loopback did not work on this Windows system. Additional remote HTTP cases were therefore not run; their peer rules were tested separately. No successful access from a second PC is claimed.
- All five browser suites passed. The new network suite covered save/restart notices, active/saved values, preservation of unsaved input during polling, adapter/port selection, safe device names, URL display, EN/DE, mobile layout and failure. It also checked disabled network/audio/ProLink controls for read-only clients while overlay designs remained editable.
- The real EXE was started with isolated configuration files, configured, stopped and restarted in both modes. Saved bind/port changes, CLI overrides, invalid addresses and access through the PC's actual LAN IP were checked. ProLink stayed idle without discovery/connection; audio capture and production injection stayed off.
- Eight public README screenshots were regenerated with English UI and synthetic data. The network page was visually inspected. Default configuration files were ignored and excluded from portable packages.

## Accounts, ratings and scenes — 2026-09-15 (unreleased)

- All nine native CTest cases passed across the base-suite and portal runs. The new portal test covers random bootstrap credentials, mandatory password changes, revocation, administrator/operator HTTP gates, last-admin protection, login limits, failed-save preservation, cross-session rating identity and aggregation, scene persistence, revisions and scoped/rotatable keys.
- All six browser suites passed. The portal suite uses the actual demo EXE with isolated storage, including first login/password change, user creation, anonymous history voting, scene dragging and saving, mobile layout, EN/DE, unauthenticated OBS child renderers, live geometry updates without restarting the iframe, conflict rejection and data persistence after restarting the EXE.
- Authenticated network and portable smoke checks passed in demo and idle ProLink modes. The portable test's real device-discovery step now requires the explicit `DECKSTATUS_TEST_DISCOVERY=1` opt-in; it was not enabled for these changes. Production injection, DJ device connections and audio capture remained off.
- Ten public screenshots were regenerated in English with synthetic data and example keys. Scene editor and admin/history rendering were inspected. The German README was removed; public screenshot assets were already English. Application translations remain available in both languages.
- The repository wiki Git endpoint returned repository-not-found. `docs/WIKI.md` is the complete Markdown import fallback. No new GitHub release is claimed by this validation entry.
- Limitations remain: no second physical PC/OBS Studio test, no HTTPS listener, browser-cookie voting is not verified-person voting, and live compatibility remains limited to the author's Rekordbox 7.2.18.0. Real ProLink hardware remains unvalidated.

## Component presets and navigation — 2026-09-15 (unreleased)

- Release configuration rebuilt successfully. The three affected native tests (`portal_access`, `network_access`, `http_server`) passed. Preset coverage includes create/update/delete, invalid options, immutable types, stale revisions, failed-save preservation, restart persistence, anonymous/forced-password/operator access and denial of OBS-key access.
- Legacy-store migration was tested against an existing version-1 document: every other data field is retained, credentials still work, and an invalid preset store is rejected without resetting it. A Windows file-handle issue found by that test was corrected before verification passed.
- All six browser suites passed. The real-EXE test saves/loads all three component types, restores deck selection and visual/signal settings, updates/deletes presets, inserts them into scenes, and verifies independent copies after preset/scene changes. The library survives restart and cleared browser preferences. EN/DE, mobile layout and anonymous OBS rendering remain covered; loading a waveform preset does not start capture.
- Navigation tests cover the three groups, standalone Admin, all component links inside the dropdown, keyboard opening/Escape, outside-click closing, mobile layout and source-mode restrictions. Source buttons now reflect read-only access immediately while device data loads. The current-mode pill remains visible on deck/master settings pages.
- Ten English screenshots were regenerated with synthetic data and example keys. README and Wiki instructions describe the shared preset library, snapshot behavior, storage, API and the new navigation. Live hardware compatibility is unchanged; production injection and audio capture remained off.

## Release 2.0.1 — 2026-09-15

- Full Release build via `build.ps1` passed: all nine native CTest cases and the Java ProLink model/UTF-8 pipe tests. Dependencies remain pinned and checksum-verified.
- All six browser suites passed, including the real EXE's account lifecycle, shared presets, independent scene copies, public ratings, anonymous OBS rendering, restart persistence and EN/DE/mobile navigation.
- The authenticated network smoke passed with an isolated store: local defaults, saved bind/port, idle ProLink mode, the host's LAN address, CLI overrides and invalid addresses. Production injection, DJ-device discovery/connections and audio capture remained off.
- EXE and DLL report 2.0.1 / 2.0.1.0; the EXE retains all nine icon sizes. Ten English public screenshots were regenerated from synthetic fixtures. The repository audit found no private/generated files or broken local documentation links.
- Release notes document first-login credentials, regenerated OBS links when upgrading from v1.4.0, preservation of preview data and the unchanged compatibility limit: only the author's Rekordbox 7.2.18.0 Windows x64 installation is live-tested; real ProLink hardware remains unvalidated.

## Additional ProLink profiles and English documentation — 2026-09-16 (unreleased)

- The full Release build passed, including all nine native CTest cases and the Java model/UTF-8 pipe checks with the bundled runtime.
- Synthetic announcement/status packets cover CDJ-3000, CDJ-3000X and XDJ-AZ, player numbers 1–6, both supported mixer profiles, rejection of unknown models, status flags/BPM, stale reports, shared-IP endpoints, shared-source track identity and distinct unknown media slots. No network sockets are opened by these Java tests.
- The metadata-policy regression check initializes Beat Link's indirect Opus/Crate Digger dependencies and verifies that the DeviceSQL fallback cannot auto-start, including repeated configuration. Unrelated lifecycle listeners are preserved. This prevents using legacy export.pdb IDs for OneLibrary/Device Library Plus tracks.
- The ProLink browser suite passed: selection and reconnect for CDJ-3000X and two XDJ-AZ endpoints sharing one IP, automatic/nonselectable DJM-900NXS2 and DJM-A9 cards, model-specific EN/DE guidance, safe text, default/source-mode gates, navigation and mobile layout. An asynchronous test wait was corrected to wait for completed disconnection before rediscovery.
- All Markdown documentation under `docs/` is now English. The memory-profile translation preserves every hexadecimal address, exact byte signature and code block; the artwork-source MIT notice is retained. Historical validation statements remain identified by revision/date. English/German application translations remain available.
- The portable package smoke test passed with isolated stores: authenticated EN/DE demo, unchanged default mode, idle ProLink/runtime availability, web routes, mode/API gates and audio off. Device discovery was not enabled. All 426 package files matched source/build output; 33 dependency archives matched pinned hashes and all 315 bundled runtime files remained unmodified. Local documentation links, translation-key parity and the repository publication audit passed.
- **No CDJ-3000X, DJM-900NXS2, XDJ-AZ or other ProLink hardware was connected or live-tested.** XDJ-AZ support is limited to announced endpoints in PRO DJ LINK mode, not standalone four-deck mode. Real firmware, DBServer metadata, shared-IP concurrency, artwork and timelines still need device validation. Production injection and audio capture remained off; Rekordbox live compatibility remains limited to the author's 7.2.18.0 Windows x64 installation.

## Release 2.0.2 — 2026-09-16

- Full Release build passed: all nine native CTest cases and the Java ProLink model/UTF-8 pipe tests with pinned dependencies and the bundled runtime.
- All six browser suites passed. The real-EXE portal suite ran through the isolated HTTPS proxy and verified original Host/Origin handling, Secure cookies, remote permissions, localhost OBS links, same-origin previews, accounts, presets, scenes, public ratings and persistence after restart.
- The network EXE smoke passed with isolated storage: saved interface/domain configuration, simultaneous LAN/localhost/127.0.0.1 access, both source modes, CLI overrides and invalid addresses. No device discovery or audio capture was started.
- Both EXE and DLL report product version 2.0.2 and file version 2.0.2.0. The EXE resource test confirmed all nine icon sizes. Documentation links and the publication audit passed. Production injection remained off and no new live hardware compatibility is claimed.
