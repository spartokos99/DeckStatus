# DeckStatus: agent working guide

## Start here

- Read [HANDOFF.md](HANDOFF.md) for the dated project state, architecture, setup and release workflow. Then inspect `git status`, the relevant code and tests before making changes.
- Use [README.md](README.md) for product behavior and [docs/WIKI.md](docs/WIKI.md) for technical details. [docs/validation.md](docs/validation.md) distinguishes automated checks from actual hardware validation.
- Treat handoff notes as a starting point, not a substitute for the current code or the user's latest instructions. Update them when a change makes them inaccurate.

## Communication and documentation

- Communicate with the owner in German unless asked otherwise.
- Keep public documentation, code comments, release notes and public screenshots in English. Keep the application bilingual: English is the default, German remains supported.
- Use synthetic English track data and example access keys for public screenshots. Do not expose real library data, credentials or keyed OBS links.
- Complete the requested work, test the affected behavior and report the result and meaningful limitations. Make routine implementation decisions without repeated confirmation requests.

## Architecture and behavior to preserve

- Windows x64, native C++20/MSVC host and injected bridge; optional Java 21 ProLink helper; local HTML/CSS/JavaScript UI without a frontend package manager or CDN.
- Default launch remains **Rekordbox mode**. ProLink is opt-in and must not initialize its networking in Rekordbox mode.
- Only the author's **Rekordbox 7.2.18.0 Windows x64** installation has been live-tested. Keep the exact executable/profile checks. Do not accept other versions based on similar offsets or describe synthetic tests as hardware certification.
- ProLink device support is experimental. Preserve exact device profiles, stale-data handling and source/media track identities. XDJ-AZ standalone four-deck mode is not supported.
- ProLink uses direct DBServer metadata requests. Do not restore the legacy DeviceSQL `export.pdb` fallback without resolving the OneLibrary ID mismatch and indirect Crate Digger auto-start; see `Main.configureMetadata()` and its regression test.
- Unknown metadata stays unknown. Current and original BPM are distinct. MASTER history is an observation of tempo-master state, not proof of audible playback.
- Domain configuration must preserve access through localhost/127.0.0.1, including with a specific LAN interface. Generated deck/master/waveform/scene URLs use `http://127.0.0.1:<port>`; embedded previews/API calls stay on their current origin.
- Every HTTP route is registered with an explicit access level (`Access` in `src/portal_http.h`); there is no permissive default. A new route must state whether it is public, key-scoped, signed-in, administrator-only or reachable during a pending password change.
- Preserve server-side authentication, roles, mandatory first password change, Host/Origin checks, remote-control policy and scoped OBS read keys. Full History and rating aggregates remain public to read; new votes require a separately validated Twitch viewer session. Viewer names and account/admin APIs remain admin-only. Twitch viewer login never grants DeckStatus operator/admin rights.
- The project version lives only in `CMakeLists.txt`; resources, the API and the tests read it from there. Do not reintroduce hard-coded version strings.
- Uploaded media lives in `DeckStatus.data/media/`, addressed by content hash, with only metadata in `portal.json`. Key derivation and media file I/O stay outside the portal lock; readers share it.
- Preserve persistent users, ratings, presets, scenes and keys. Scene layers are independent copies of inserted presets. Playing history is session-local. Loading a preset or scene must not start audio capture.

## Working safely with this application

- Prefer synthetic fixtures and isolated demo instances. Do not launch the default EXE merely to inspect the UI: it may attach to a running Rekordbox process.
- Do not start production injection, ProLink discovery/connections or audio capture as an incidental build/test step. Use such hardware operations only when they are part of the user's requested work.
- Isolate tests with their own data directory, network configuration and available port. Existing test helpers already do this. Stop only processes started for the task; leave unrelated sessions and user files alone.
- Do not commit runtime stores, network settings, databases, binaries, downloaded dependencies, IDE state, credentials or private session logs. Respect `.gitignore`; stage explicit intended files when unrelated untracked files exist.
- Windows paths and installed tools differ between PCs. Discover tool locations; do not copy machine-specific paths into build logic. Use native PowerShell file operations and verify any recursive move/delete target first.

## Build and tests

Run from the repository root on Windows with Visual Studio C++/CMake and JDK 21+ installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
```

This builds the Release binaries, runs native CTest cases, verifies pinned Java/runtime downloads and builds/tests the ProLink helper. `-SkipProLink` is for native-only development, not a complete release. Browser tests require Node.js 22+ and Microsoft Edge.

- Run tests appropriate to the change; use the commands in HANDOFF.md. Add regression coverage for meaningful behavioral changes, not tests that merely mirror implementation.
- Do not enable `DECKSTATUS_TEST_DISCOVERY=1` or the audio loopback smoke as part of ordinary validation.
- A fresh PC may skip the optional database integration case without a configured local Rekordbox installation. Report skips; do not count them as passes.
- Documentation-only changes need link/content/diff checks, not another application build or hardware run.

## Git and releases

- Preserve unrelated work. Commit/push or create releases when requested by the user; follow the requested scope and version.
- A documentation handoff does not require moving an existing tag or rebuilding a published release.
- For a release, synchronize code/resource/test versions, test the final build, package the full runtime, verify a freshly extracted ZIP, publish a SHA-256 sidecar and meaningful compatibility/upgrade notes, then verify the public release and asset digests.
- Never include `DeckStatus.data` or `DeckStatus.network.json` in a release. Do not print authentication tokens or store them in scripts or documentation.
