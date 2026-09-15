# 🔌 ProLink mode · DeckStatus 1.4.0

ProLink is an **experimental, opt-in network source** for CDJ-3000 players and DJM-A9 mixers. It supplies the existing dashboard, JSON API, deck/master overlays and session history. This release has automated fixture coverage, **not live CDJ-3000/DJM-A9 validation**. No firmware combination is certified yet. The separately validated Rekordbox source remains limited to the author's 7.2.18.0 executable.

## Start and connect

1. Extract the complete Windows release, including `prolink`. Run `Start-ProLink.cmd` or `DeckStatus.exe --mode prolink`.
2. Connect the PC, CDJ-3000 players and DJM-A9 to the same Ethernet network. Use unique player numbers and one matching PC network adapter.
3. Open `http://127.0.0.1:18740/prolink/settings`. Click **Find devices**. This listens for announcements; opening the page alone starts no network discovery.
4. Select one to four players and click **Connect selected players**. Player numbers in ascending order map to dashboard Decks 1–4. Players 5 and 6 can be selected. The mixer is detected automatically.
5. Open the familiar overlay settings and copy your OBS URLs. **Disconnect / stop discovery** stops Link networking. Disconnect before changing the player selection.

`DeckStatus.exe` with no mode argument retains the original Rekordbox startup. `--mode rekordbox` is equivalent. `--prolink` is an alias for `--mode prolink`; `--port` and `--lang en|de` work in both modes. ProLink rejects `--pid`, `--database` and `--demo` instead of silently ignoring them. Mode selection is per launch; it does not overwrite your default or saved overlay designs.

The navigation always shows Monitor, Stream overlays and Connections. The current mode appears beside the logo. Inactive connection links have no URL and are excluded from keyboard navigation; their APIs return HTTP 409. Shared functions stay enabled in both modes.

## Data and boundaries

| Capability | ProLink behaviour |
|---|---|
| Title, artist, album, key, genre, label, artwork | Requested through Beat Link / Crate Digger; missing metadata remains unknown |
| Current BPM | Effective deck tempo, including pitch |
| Original BPM | Analysed tempo from track metadata; not inferred from current tempo |
| Timeline | Beat Link time tracking, including precise CDJ-3000 positions; waits for a report after each track change |
| MASTER and history | Same MASTER sequence semantics as the Rekordbox mode; an unselected player or mixer as master produces no current track card |
| Playing, Sync, On-Air | Additional fields in `/api/state` and badges on the device setup page; On-Air requires a fresh compatible mixer status |
| Windows audio waveform | Available in both modes through the explicitly selected Windows audio source |
| Mixer faders/EQ/FX and analysed track-waveform overlay | Not implemented; capability flags are false |
| Remote player or mixer control | No play, stop, load, sync, tempo-master or mixer-control commands are exposed |

**Rekordbox-exported USB media is the primary target.** Streaming, cloud sources, Device Library Plus-only media, other device models and arbitrary firmware versions have not been validated. This release refuses connection when discovery includes devices other than CDJ-3000 and DJM-A9. It also refuses duplicate selected player numbers and ambiguous/unreachable network interfaces.

On-Air describes the mixer's channel indication; it does not prove audible output. Match player numbers to mixer channels. Overlays and history continue to follow MASTER, including when the master is paused. Disconnecting retains the collected history without presenting it as live. Restarting DeckStatus clears the history.

Track IDs exposed by ProLink are **session IDs**, not globally unique Rekordbox library IDs. They include source player/address, media slot, media generation and source track ID. The same source track moved between decks keeps its identity; a different USB source cannot overwrite it. `sourceTrackId`, `sourcePlayer`, `sourceSlot` and `playerNumber` preserve the source details when available. Helper restarts use a new ID range. Artwork is bounded to 2 MiB per image and 64 MiB in the host cache; older artwork may be evicted while its history row remains.

## Networking and runtime

The helper uses **Beat Link 8.0.0** and its dependencies. It joins as a virtual monitoring device using an available number above the real CDJ channels; it never enables status/beat transmission for tempo control. Metadata retrieval requires normal Link handshakes and read requests, so connection is not passive packet sniffing.

Windows Firewall must allow the bundled `prolink/runtime/bin/java.exe` on the private DJ network. PRO DJ LINK uses UDP 50000–50002 and additional player metadata/NFS traffic. Rekordbox, Beat Link Trigger or another Link client on the same PC may already occupy the ports. Stop that client before connecting. No automatic firewall rules, adapter settings or device configuration are changed.

The web server still binds to `127.0.0.1`. The Java helper has no HTTP service: bounded newline-delimited JSON travels over inherited anonymous pipes. The host launches it without a visible console and owns it through a Windows job object. Closing/crashing the host terminates its helper. Stale helper or player reports stop producing live deck data. No ProLink networking or Java process starts in Rekordbox mode.

The portable ZIP includes an unmodified **Eclipse Temurin 21.0.12.1+1 Windows x64 JRE**. Users do not need to install Java. Beat Link may temporarily cache metadata export files downloaded from players. No audio recording or playback is added. The existing WASAPI waveform remains a separate feature and starts with capture off.

## Build and validation

The complete build additionally needs a JDK providing `javac` and `jar` (21 or later), plus internet access for the first dependency download:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
```

`prolink/dependencies.lock.json` pins every downloaded JAR, source JAR, POM and runtime ZIP by URL and SHA-256. Downloads stay in ignored `build/prolink-deps`. The helper is compiled with `--release 21` and tested with the bundled runtime. Runtime libraries, dependency sources/POMs and runtime legal notices are included in the portable package. `build.ps1 -SkipProLink` builds/tests only the native application; it does not create a complete portable ProLink distribution.

Validation includes seven native CTest cases (including helper lifecycle/IPC/freshness/restart and HTTP mode gating), Java model checks using parsed synthetic CDJ status packets, and four headless browser suites. A real helper subprocess additionally verifies UTF-8 JSON over Windows pipes, even with a Windows-1252 stdout default, and clean shutdown on closed input. These do not certify real hardware interoperability.

```powershell
node tests/browser_prolink_test.cjs
powershell -NoProfile -ExecutionPolicy Bypass -File prolink/build.ps1
node tests/portable_smoke.cjs C:/path/to/extracted-release
```

Sources: [Beat Link](https://github.com/Deep-Symmetry/beat-link), [DJ Link protocol analysis](https://djl-analysis.deepsymmetry.org/djl-analysis/), [Temurin runtime](https://github.com/adoptium/temurin21-binaries/releases/tag/jdk-21.0.12.1%2B1). See [dependency notices](../THIRD_PARTY_NOTICES.md).
