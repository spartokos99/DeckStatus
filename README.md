# DeckStatus

Live DJ metadata, audience ratings and configurable OBS overlays for Windows.

**Compatibility:** live-tested only with the author's **Rekordbox 7.2.18.0 Windows x64 installation**. ProLink support for CDJ-3000 / DJM-A9 is experimental and has not been tested on real hardware.

## Run

1. Extract the full Windows ZIP into a writable folder. Keep the EXE, DLL, `web` and `prolink` together.
2. Start Rekordbox, then run **DeckStatus.exe**. For ProLink, use **Start-ProLink.cmd**.
3. Open **http://127.0.0.1:18740**.
4. On first start, sign in as **admin** with the temporary password printed in the DeckStatus console. Set a new password, then sign in again.
5. Open **Stream → Scene Components**, configure an overlay and save a named preset. Insert it in **Stream → Scene editor**, save the scene and copy its URL into an **OBS Browser Source**.

Stop with **Ctrl+C**. Use `DeckStatus.exe --demo` to explore synthetic tracks without Rekordbox or DJ hardware.

Download the complete Windows package from [DeckStatus v2.0.1](https://github.com/spartokos99/DeckStatus/releases/tag/v2.0.1). When upgrading from v1.4.0, generate new OBS URLs after signing in: overlays now require a read-only key. See the [upgrade notes](CHANGELOG.md#upgrade).

## Features

- **Four-deck dashboard:** title, artist, album, key, cover, current/original BPM and timelines.
- **OBS overlays:** individual decks, current master with adjustable history, and six audio waveform styles. Customise fields, colours, fonts, dimensions, alignment and smooth transitions.
- **Saved presets:** save, load, update and delete named deck, master and waveform designs. The library persists on the server and is shared with signed-in users.
- **Scene editor:** insert saved presets on a monitor-sized canvas. Drag, resize, reorder and style independent layers; save to update the same OBS source.
- **Public Full History:** viewers browse played MASTER tracks at `/history` and rate them from 1–5 stars without an account.
- **Persistent ratings:** the admin panel shows averages, vote counts and star distributions across sessions.
- **Accounts:** administrator/operator roles, user management and required initial password changes.
- **Optional LAN access**, JSON API, Rekordbox/ProLink modes and English/German application translations.

![Scene editor with synthetic demo tracks](docs/images/scene-editor-en.png)

## Sharing and storage

Dashboard and configuration pages require sign-in. Full History is public. Generated OBS URLs contain revocable read-only keys; keep these links private.

Users, ratings, presets and scenes persist in **`DeckStatus.data`** beside the EXE. Network settings use **`DeckStatus.network.json`**. Preserve both when upgrading and back them up while the app is stopped. Played-track history itself belongs to the running session.

Ratings allow one changeable vote per browser and track. This is lightweight audience feedback, not verified-person voting.

The server initially accepts localhost connections only. Enable LAN access under **Connections → Network** and restart. HTTP is not encrypted: use a trusted network and do not expose the server directly to the internet.

## Build and test

Requires Windows x64, Visual Studio C++ tools, CMake and JDK 21+ for ProLink.

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
node tests/browser_master_test.cjs
node tests/browser_waveform_test.cjs
node tests/browser_dashboard_history_test.cjs
node tests/browser_prolink_test.cjs
node tests/browser_network_test.cjs
node tests/browser_portal_test.cjs
node tests/network_smoke.cjs
~~~

Browser tests require Node.js 22+ and Microsoft Edge. Native tests cover injection fixtures, metadata, HTTP, audio, ProLink, network settings, authentication, ratings, presets, scenes and storage migration. The portal browser test runs the real EXE with isolated data, including preset reuse, anonymous OBS rendering and restart persistence. Automated tests do not establish additional Rekordbox or real ProLink compatibility.

See the [technical wiki](docs/WIKI.md) for setup, permissions, APIs, storage and troubleshooting, and [third-party notices](THIRD_PARTY_NOTICES.md) for dependencies.
