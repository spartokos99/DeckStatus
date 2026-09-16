# DeckStatus project handoff

Prepared on **2026-09-16** for continuing development on another Windows PC. Read [AGENTS.md](AGENTS.md) first. This file records the state at handoff; check Git and the code for later changes.

## Current state

- Repository: [spartokos99/DeckStatus](https://github.com/spartokos99/DeckStatus), branch `main`.
- Published application: [v2.0.2](https://github.com/spartokos99/DeckStatus/releases/tag/v2.0.2).
- Application release commit: `b82fd1a2f07c80aa5b87638eda9e7efcde17fce1`. This documentation handoff is a later, documentation-only commit; the release tag stays on the tested application commit.
- Asset: `DeckStatus-2.0.2-win-x64.zip`, 93,566,741 bytes. SHA-256: `a1dc000a7b46ca6a131b851d0ab2525c3fbc3db753552574c876b374d6944571`. GitHub also hosts its `.sha256` sidecar.
- The requested implementation and release work is complete. There is no active implementation task to resume automatically; wait for the owner's next feature or fix request after orientation.
- A native OBS Studio plugin was discussed as a possible future project, but has **not** been implemented or commissioned. Current OBS integration uses Browser Sources.
- The old workspace contained an unrelated, untracked `rb_inj.sln`. It was deliberately left untouched and excluded from commits/releases. The repository's original folder name may still be `rb_inj`; the product is DeckStatus.

## First steps on another PC

```powershell
git clone https://github.com/spartokos99/DeckStatus.git
cd DeckStatus
git status --short
git log -3 --oneline
```

Open the repository in the IDE. Install Windows x64 Visual Studio Desktop development with C++ and CMake, JDK 21+ (`javac` and `jar` on PATH), Node.js 22+ and Microsoft Edge. The previous machine used Visual Studio 2026 / MSVC 19.51 and Node.js 24.13.0. Build tool paths are machine-specific; `build.ps1` discovers Visual Studio's CMake when it is not on PATH.

The Java build verifies `prolink/dependencies.lock.json` and downloads the pinned dependencies/runtime when missing. First build needs network access. End users receive the bundled JRE and do not need Java installed separately.

Do not copy an old `build/` tree as the new machine's build configuration. It contains generated projects, cached absolute paths, test artifacts and downloaded tools. In particular, the old `build/publish-*.ps1` and package-audit scripts were local release helpers, **not tracked build tooling**; a fresh clone will not contain them.

If moving the running application's data too, stop DeckStatus and privately transfer `DeckStatus.data` and `DeckStatus.network.json` from the actual deployment directory. Finish the initial password change before moving accounts: the temporary bootstrap password uses Windows DPAPI tied to the creator. Normal password hashes and persisted records are portable afterwards. Saved sessions expire at restart. Adjust the saved bind address to the new PC's interface. These private files and browser-local preferences are not transferred by Git.

## Product behavior

DeckStatus serves live DJ metadata and stream overlays from either the default Rekordbox source or an optional PRO DJ LINK source. Both feed the same dashboard, API and renderers.

- Four deck cards: title, artist, album, key, artwork, current/original BPM and track timelines.
- Deck overlays, master overlay with configurable history/scale/alignment and smooth transitions, and Windows-input audio waveform overlays with six styles.
- Shared saved component presets and a monitor-sized scene editor. Inserting a preset creates an independent layer copy; changing a preset does not rewrite existing scenes. A saved scene uses one OBS Browser Source URL.
- Accounts with administrator/operator roles, initial password change, user management and server-side authorization.
- Public Full History and 1–5-star browser-based ratings. Ratings persist across sessions; the played-track sequence does not. This is lightweight audience feedback, not verified-person voting.
- Configurable local/LAN access, optional public HTTPS domain behind a reverse proxy, and separately gated remote audio/ProLink controls.
- English default UI plus German translations. Navigation: Start, Stream (Scene editor, Full History, Scene Components dropdown), Connections and standalone Admin. Inactive source-mode functions remain visible but disabled.

## Source map

| Area | Main files | Responsibility |
|---|---|---|
| Startup | `src/deckstatus.cpp` | CLI, selected source, lifecycle and host startup |
| Injection/profile | `src/injector.cpp`, `src/deckstatus_bridge.cpp`, `src/deckstatus_protocol.h`, `src/scanner.h` | DLL attachment, exact supported executable profile, bounded reads and IPC |
| Library/artwork | `src/artwork.cpp` | Read-only local library enrichment and bounded artwork access/cache |
| HTTP | `src/server.cpp`, `src/portal_http.h` | Fixed routes, source gates, authorization, request validation, cookies, local/domain listeners |
| Network | `src/network.cpp`, `src/network.h` | Validated persisted listener/domain options, interfaces and peer policy |
| Persistent portal | `src/portal.cpp`, `src/portal.h` | Users/passwords, sessions, ratings, presets, scenes, revision checks and OBS keys |
| History | `src/master_history.h` | Shared master observations, overlay history and paginated full session history |
| Windows audio | `src/audio_capture.cpp`, `src/audio_samples.h` | Explicit WASAPI input/loopback capture and sample processing |
| Native ProLink host | `src/prolink.cpp` | Owned JVM process, bounded JSON IPC, freshness, artwork and restart handling |
| Java ProLink | `prolink/src/com/deckstatus/prolink/Main.java`, `DeviceSupport.java` in the same directory | Discovery/connection, exact model profiles, status/metadata adaptation |
| Shared web UI | `web/navigation.js`, `web/auth.js`, `web/i18n.js`, `web/locales/` | Navigation, authentication and shared translations |
| Overlay links/designs | `web/broadcast.js`, `web/settings.js`, `web/master-options.js`, `web/overlay-shared.js` | Scoped URLs, deck/master options and common rendering |
| Presets/scenes | `web/component-presets.js`, `web/scene-editor.js`, `web/scene.js`, `web/scene-shared.js` | Shared preset library, editing and live scene rendering |
| Waveforms | `web/waveform-settings.js`, `web/waveform.js`, `web/waveform-renderer.js`, `web/waveform-options.js` | Device controls, visualization options and canvas output |
| Build/resources | `CMakeLists.txt`, `build.ps1`, `prolink/build.ps1`, `src/deckstatus.rc` | Native/Java build, packaging inputs, version information and icon |

## Decisions that are easy to accidentally undo

### Rekordbox compatibility

Only the author's Rekordbox **7.2.18.0 Windows x64** installation has been live-tested. The bridge validates the version/PE profile and 14 exact code regions; it does not support arbitrary Rekordbox 7 builds. IPC is version 3. The separate host enriches metadata through the local read-only database; Rekordbox DLLs are not distributed.

Read [the memory profile](docs/rekordbox-7.2.18.md), [metadata/artwork sources](docs/artwork-sources.md) and [the validation record](docs/validation.md). Original BPM/key come from metadata; current BPM comes from live deck state. The Rekordbox profile does not provide play/pause, fader position or a live transposed key.

### ProLink limitations and metadata policy

- Implemented profiles: CDJ-3000, CDJ-3000X and XDJ-AZ player endpoints; DJM-A9 and DJM-900NXS2 mixers. **None has been tested with actual hardware.** Synthetic packets are not firmware certification.
- XDJ-AZ requires **PRO DJ LINK → Connect to CDJ/XDJ/DJM**. Only announced endpoints are exposed; standalone four-deck/lighting mode is not implemented. Multiple player numbers may share one IP. Standalone USB 2 metadata is unsupported; unknown raw slot numbers remain distinct in track identities.
- One to four selected players map in ascending player-number order to dashboard Decks 1–4; player numbers 5 and 6 are selectable. Mixers are automatic, not track decks. Duplicate selected numbers and unsupported discovered models prevent connection.
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

### Persistent data and access

The private store contains users, password hashes, votes, presets, scenes and scoped keys. Store writes are atomic, use revisions where appropriate and preserve prior data on failure. Invalid stores are not silently replaced. OBS keys authorize only their renderer/read routes; they cannot administer users or source controls. Public history must not expose unobserved library data or private settings. Preserve migration coverage and the last-admin protections.

## Validation commands

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
node tests/browser_master_test.cjs
node tests/browser_waveform_test.cjs
node tests/browser_dashboard_history_test.cjs
node tests/browser_prolink_test.cjs
node tests/browser_network_test.cjs
node tests/browser_portal_test.cjs
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

Native tests include `portal_access`, `network_access`, `prolink_backend`, `master_history`, `artwork_database`, `http_server`, `scanner_boundaries`, `injection_lifecycle` and `audio_capture`. Injection targets an owned fixture; the default audio test does not open capture. The optional database test needs CMake's `REKORDBOX_TEST_EXE` set to an appropriate local executable; a new machine without it may skip that case. Report what actually ran.

For v2.0.2, all nine native cases, Java model/UTF-8 pipe checks and all six browser suites passed. The portal suite also passed through HTTPS. Network smoke verified simultaneous local/LAN access with a configured domain. EXE/DLL versions and nine icon sizes were checked. A freshly extracted ZIP, including a path with spaces, passed portable smoke; 426 files matched source/build output, 33 dependency archives matched pinned hashes and all 315 bundled runtime files were unmodified. These are dated results, not proof that a future checkout or a new PC has been tested.

## Release workflow

When the owner requests another release:

1. Inspect Git status and the intended diff. Preserve unrelated files. Confirm branch/remote and the next version; do not move an existing published tag.
2. Update version references in `CMakeLists.txt`, `src/deckstatus.rc` (numeric and string EXE/DLL versions), `src/server.cpp`, current README/Wiki links, fixtures, `tests/portable_smoke.cjs`, `tests/resource_test.ps1` and `tools/readme-screenshots.cjs`. Search for the prior version, but preserve historical changelog/validation entries.
3. Write English release/upgrade notes and keep compatibility limitations explicit. Run the appropriate full release checks above. Record actual results in `docs/validation.md`.
4. Run CMake install into a fresh staging directory after the full build, for example `cmake --install build --config Release --prefix build/package-NEXT` from a shell where CMake is available. Include EXE, bridge DLL, `web`, `prolink` with its runtime/dependencies/licenses, documentation and notices. Never package the entire build directory or deployed private data.
5. Create a ZIP and SHA-256 sidecar. Extract it to a fresh location, compare contents against source/build and dependency hashes, and run portable smoke. Check that no private/generated files entered the Git diff or package.
6. Commit the intended source changes, create the requested tag and push without forcing unrelated history. Create a GitHub draft release, upload the ZIP/checksum, verify uploaded sizes/digests and then publish. Keep repository-document links in GitHub release notes absolute and version-pinned.
7. Verify the public tag/release/assets. Report the release URL and completed checks. Keep credentials in the configured credential manager or CLI authentication; never print or commit them.

The local one-off publishing scripts used for v2.0.2 are not part of the clone. Recreate the workflow using available authenticated tools rather than expecting those ignored files to exist. The current documentation handoff does not change the v2.0.2 ZIP, tag or published version.

## Continuing with a new assistant

Suggested first prompt:

> Read AGENTS.md and HANDOFF.md, inspect the current Git status, and summarize the project state and compatibility boundaries in German. Treat the published v2.0.2 work as complete. Do not start production injection, device discovery or audio capture during orientation. Then continue with my next request.

Keep this file current with future decisions and completed work. It transfers project context, not the previous chat session or its tool permissions.
