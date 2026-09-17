# ProLink mode

ProLink is an **experimental, opt-in network source**. It supplies the existing dashboard, JSON API, deck/master overlays, scenes and session history. Supported device profiles have automated synthetic-packet coverage, **not live hardware validation**. No firmware combination is certified. The separately validated Rekordbox source remains limited to the author's 7.2.18.0 Windows x64 executable.

## Devices

| Model | Role | Requirements and limits |
|---|---|---|
| CDJ-3000 | Player | Existing status, metadata, artwork and timeline integration |
| CDJ-3000X | Player | Direct DBServer metadata requests for OneLibrary media; a free standard player number (1–4) is preferred. Metadata may be unavailable without one. Cloud/streaming sources are unverified. |
| XDJ-AZ | Player endpoints | Select **PRO DJ LINK → Connect to CDJ/XDJ/DJM** on the unit. Select its announced player numbers individually; multiple endpoints can share one IP. Standalone four-deck mode and its USB 2 metadata are not supported. |
| DJM-A9 | Mixer | Automatically detected for mixer status and On-Air availability |
| DJM-900NXS2 | Mixer | Automatically detected for mixer status and On-Air availability; no fader, EQ or FX values |

Only player numbers 1–6 can be selected; at most four feed the dashboard. A mixer is never a selectable track deck. Device recognition uses exact model names, not broad CDJ/XDJ/DJM prefixes. Unknown models still prevent connection. All rows above describe implemented profiles, not hardware certification.

## Start and connect

1. Extract the complete Windows release, including `prolink`. Run `Start-ProLink.cmd` or `DeckStatus.exe --mode prolink`.
2. Connect the PC and supported hardware to the same Ethernet network. Use unique player numbers and one matching PC network adapter. Configure the XDJ-AZ's PRO DJ LINK mode as described above.
3. Open `http://127.0.0.1:18740/prolink/settings`. Click **Find devices**. This listens for announcements; opening the page alone starts no network discovery.
4. Select one to four players and click **Connect selected players**. Player numbers in ascending order map to dashboard Decks 1–4. Players 5 and 6 can be selected. The mixer is detected automatically.
5. Open the familiar overlay settings and copy your OBS URLs. **Disconnect / stop discovery** stops Link networking. Disconnect before changing the player selection.

`DeckStatus.exe` with no mode argument retains the original Rekordbox startup. `--mode rekordbox` is equivalent. `--prolink` is an alias for `--mode prolink`; `--port` and `--lang en|de` work in both modes. ProLink rejects `--pid`, `--database` and `--demo` instead of silently ignoring them. Mode selection is per launch; it does not overwrite your default or saved overlay designs.

The navigation shows Start, Stream and Connections, plus standalone Admin. Stream contains the Scene Components dropdown, Scene editor and Full History. The current mode appears beside the logo. Inactive connection links have no URL and are excluded from keyboard navigation; their APIs return HTTP 409. Shared functions stay enabled in both modes.

## Data and boundaries

| Capability | ProLink behaviour |
|---|---|
| Title, artist, album, key, genre, label, artwork | Requested directly from the device through Beat Link's DBServer client; missing metadata remains unknown |
| Current BPM | Effective deck tempo, including pitch |
| Original BPM | Analysed tempo from track metadata; not inferred from current tempo |
| Timeline | Beat Link time tracking when position reports or a usable beat grid are available; waits for a new report after each track change. No precise-position guarantee is made for the new models. |
| MASTER and history | Same MASTER sequence semantics as the Rekordbox mode; an unselected player or mixer as master produces no current track card |
| Playing, Sync, On-Air | Additional fields in `/api/state` and badges on the device setup page; On-Air requires a fresh compatible mixer status |
| Windows audio waveform | Available in both modes through the explicitly selected Windows audio source |
| Mixer faders/EQ/FX and analysed track-waveform overlay | Not implemented; capability flags are false |
| Remote player or mixer control | No play, stop, load, sync, tempo-master or mixer-control commands are exposed |

**Rekordbox-exported USB media is the primary target.** Streaming, cloud sources and arbitrary firmware versions have not been validated. Connections are refused if discovery includes models outside the table, selected player numbers are duplicated, or the network interface is ambiguous/unreachable.

**OneLibrary / Device Library Plus:** these track IDs must not be looked up in the older DeviceSQL `export.pdb` database; the same numeric ID can identify another track. DeckStatus therefore uses direct DBServer requests and disables Crate Digger's legacy fallback, including its indirect lifecycle autostart. This applies to the whole ProLink session so mixed networks and devices discovered later cannot inject incorrect metadata. If DBServer cannot supply metadata, title, artist, album, key, original BPM, artwork and possibly duration/timeline remain unavailable while status/current BPM can still appear. This also removes the old offline-export fallback for CDJ-3000 networks. No encrypted OneLibrary database parser is included.

For XDJ-AZ, DeckStatus exposes only player endpoints actually announced in PRO DJ LINK mode. It does not synthesize four standalone decks or translate lighting-mode packets. USB 2's newer slot code is retained as `UNKNOWN_7` if encountered, keeping track identities separate; this does not enable metadata retrieval for that slot. Concurrent metadata requests from endpoints sharing an IP still depend on Beat Link 8.0.0 and the device's DBServer behavior and require hardware validation.

On-Air describes the mixer's channel indication; it does not prove audible output. Match player numbers to mixer channels. Overlays and history continue to follow MASTER, including when the master is paused. Disconnecting retains the collected history without presenting it as live. Restarting DeckStatus clears the history.

Track IDs exposed by ProLink are **session IDs**, not globally unique Rekordbox library IDs. They include source player/address, media slot, media generation and source track ID. The same source track moved between decks keeps its identity; a different USB source cannot overwrite it. `sourceTrackId`, `sourcePlayer`, `sourceSlot` and `playerNumber` preserve the source details when available. Helper restarts use a new ID range. Artwork is bounded to 2 MiB per image and 64 MiB in the host cache; older artwork may be evicted while its history row remains.

## Networking and runtime

The helper uses **Beat Link 8.0.0** and its pinned dependencies. It normally joins using an available number above the real CDJ channels. When CDJ-3000X is present, it first tries a free standard number (1–4) for DBServer compatibility; occupied numbers are never deliberately reused. Number selection is reset on reconnect. It never enables status/beat transmission for tempo control. Metadata retrieval requires normal Link handshakes and read requests, so connection is not passive packet sniffing.

Windows Firewall must allow the bundled `prolink/runtime/bin/java.exe` on the private DJ network. PRO DJ LINK uses UDP 50000–50002 and additional TCP DBServer traffic (port discovery on 12523, followed by the port returned by the player). Rekordbox, Beat Link Trigger or another Link client on the same PC may already occupy the ports. Stop that client before connecting. No automatic firewall rules, adapter settings or device configuration are changed.

The web server initially binds to `127.0.0.1`; the current source supports optional [network access](network.md). The Java helper has no HTTP service: bounded newline-delimited JSON travels over inherited anonymous pipes. The host launches it without a visible console and owns it through a Windows job object. Closing/crashing the host terminates its helper. Stale helper or player reports stop producing live deck data. No ProLink networking or Java process starts in Rekordbox mode.

The portable ZIP includes an unmodified **Eclipse Temurin 21.0.12.1+1 Windows x64 JRE**. Users do not need to install Java. The legacy export download is disabled; metadata is cached in memory by Beat Link. No audio recording or playback is added. The existing WASAPI waveform remains a separate feature. Capture defaults to off; administrators can opt into starting a saved Windows audio device at application launch under Admin → Audio input.

## Build and validation

The complete build additionally needs a JDK providing `javac` and `jar` (21 or later), plus internet access for the first dependency download:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
```

`prolink/dependencies.lock.json` pins every downloaded JAR, source JAR, POM and runtime ZIP by URL and SHA-256. Downloads stay in ignored `build/prolink-deps`. The helper is compiled with `--release 21` and tested with the bundled runtime. Runtime libraries, dependency sources/POMs and runtime legal notices are included in the portable package. `build.ps1 -SkipProLink` builds/tests only the native application; it does not create a complete portable ProLink distribution.

The project has nine native CTest cases (including helper lifecycle/IPC/freshness/restart and HTTP mode gating) and six headless browser suites. Java checks parse synthetic CDJ-3000, CDJ-3000X and XDJ-AZ announcement/status packets, validate both mixer profiles, shared-IP deck identities, unknown slot isolation and removal of the unsafe DeviceSQL auto-start hook. Browser fixtures exercise model guidance and selection. A real helper subprocess verifies UTF-8 JSON over Windows pipes, even with a Windows-1252 stdout default, and clean shutdown on closed input. These checks do not certify real hardware interoperability. See the dated [validation log](validation.md) for completed runs.

```powershell
node tests/browser_prolink_test.cjs
powershell -NoProfile -ExecutionPolicy Bypass -File prolink/build.ps1
node tests/portable_smoke.cjs C:/path/to/extracted-release
```

Sources: [Beat Link](https://github.com/Deep-Symmetry/beat-link), [DJ Link protocol analysis](https://djl-analysis.deepsymmetry.org/djl-analysis/), [Temurin runtime](https://github.com/adoptium/temurin21-binaries/releases/tag/jdk-21.0.12.1%2B1). See [dependency notices](../THIRD_PARTY_NOTICES.md).

Device-specific references: [XDJ-AZ connection modes](https://downloads.support.alphatheta.com/manuals/all-in-one-dj-systems/XDJ-AZ/html/en/000COV_en/Product_overview/Product_overview.htm?rhtocid=_2), [upstream Device Library Plus correction](https://github.com/Deep-Symmetry/beat-link-trigger/blob/main/CHANGELOG.md), [status packet and source-slot analysis](https://djl-analysis.deepsymmetry.org/djl-analysis/vcdj.html).
