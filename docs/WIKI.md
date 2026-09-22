# DeckStatus technical guide

**GitHub Wiki import:** create a page named **Home** in your repository's Wiki, select Markdown and paste this entire file. The wiki Git repository was unavailable when this guide was prepared.

This guide describes **DeckStatus v2.2.0**, including the creative components documented in [Creative scene components](scene-components.md). Version 1.4.0 predates authentication, ratings, saved component presets, scenes and configurable HTTP access. After upgrading from v1.4.0, sign in and generate new keyed OBS URLs. If you used a preview build, preserve its data directory and network settings; existing accounts, presets, scenes and keys remain valid.

## Running DeckStatus

[Twitch automation and improved scene layers](twitch.md) are included. Link accounts under **Admin → Twitch** and create rules with multiple actions under **Stream → Automations**. It works with both DJ source modes and does not require a public inbound connection. Version 2.2.0 adds a refreshed application interface and a shared master hold time under **Admin → Master detection** (4 seconds by default).

Extract the full Windows x64 package into a writable directory. Keep the EXE, bridge DLL, `web` and `prolink` folders together.

| Command | Source |
|---|---|
| `DeckStatus.exe` | Default Rekordbox integration |
| `DeckStatus.exe --mode prolink` / `Start-ProLink.cmd` | Experimental PRO DJ LINK |
| `DeckStatus.exe --demo` | Synthetic metadata and timelines |
| `DeckStatus.exe --help` | Available options |

Open `http://127.0.0.1:18740/`. On the first start, sign in as `admin` using the random temporary password printed in the local console. Change it before accessing protected pages, then sign in again. Initial credentials remain available in the console on subsequent starts until changed. Stop with Ctrl+C.

## Compatibility and architecture

**Only the author's Rekordbox 7.2.18.0 Windows x64 executable has been live-tested.** The bridge uses a version-specific profile and refuses unsupported executables. New versions need independent validation.

ProLink targets CDJ-3000, CDJ-3000X and XDJ-AZ player endpoints, plus DJM-A9 and DJM-900NXS2 mixers, with **no live hardware validation**. XDJ-AZ requires PRO DJ LINK → Connect to CDJ/XDJ/DJM; standalone four-deck mode is not supported. Metadata is requested directly through DBServer; the DeviceSQL export.pdb fallback is disabled because OneLibrary/Device Library Plus IDs can refer to different tracks. Missing metadata stays unknown. Firmware combinations, streaming and cloud sources are unverified. See [ProLink](prolink.md) for device-specific limits.

The native C++ EXE serves HTTP, accounts, persistence and Windows audio. In Rekordbox mode its companion DLL reads supported deck state and collection metadata enriches tracks/artwork. In ProLink mode an owned Java helper uses Beat Link 8.0.0 with direct DBServer queries and communicates over bounded JSON pipes. The legacy Crate Digger metadata fallback is disabled.

The web interface uses local HTML/CSS/JavaScript without a CDN. `web/theme.css` holds the shared design tokens and base styles for application pages; renderer documents keep their overlay styling. The displayed language loads first; application pages then prefetch the other language, while renderers load only their selected language. Shared polling pauses while a tab is hidden, and public scripts, styles and translations revalidate with an ETag. A shared master-history model serves both modes. A separate persistent store holds users, ratings, presets and scenes. The audio waveform visualizes an explicitly selected Windows source, not a track's analysed Rekordbox waveform.

## Users and permissions

| Action | Anonymous | Operator | Administrator |
|---|---|---|---|
| Full History / observed-track covers | Yes | Yes | Yes |
| Rate tracks | Twitch viewer login | Twitch viewer login | Twitch viewer login |
| Dashboard / metadata / overlay settings | No | Yes | Yes |
| Create, edit and delete shared scenes | No | Yes | Yes |
| Save, update and delete shared component presets | No | Yes | Yes |
| Upload/delete shared images and GIFs | No | Yes | Yes |
| Persistent rating aggregates | No | No | Yes |
| Manage users / reset passwords | No | No | Yes |
| Regenerate standalone OBS keys | No | No | Yes |
| Read network configuration | No | No | Yes |
| Change network configuration | No | No | On the DeckStatus PC |
| ProLink controls | No | Subject to network policy | Subject to network policy |
| Audio settings / capture controls | No | No | Subject to network policy |

Pending-password accounts can inspect their identity, change their password, sign out and use public resources. Server-side permission checks protect direct API requests too.

Usernames contain 3–40 ASCII letters, digits, dots, underscores or hyphens and are case-insensitive. New accounts and resets require a new password at first login. Editing users revokes their sessions. Users cannot delete themselves, and the last administrator cannot be removed or demoted.

Passwords contain 12–256 UTF-8 bytes. Windows CNG PBKDF2-HMAC-SHA-256 uses 600,000 iterations and random salts; derived hashes and salts persist. The initial password also has a Windows DPAPI-protected bootstrap field until changed, so its creator's Windows account can retrieve it after a restart.

Session cookies are random, HttpOnly and SameSite=Strict. They expire after 12 hours or on logout, password change, user edit or EXE restart. Sessions are held in memory. Login/password requests have bounded per-minute limits.

The built-in listener is HTTP. Use a trusted network for direct access and the proxy-to-DeckStatus connection. For public HTTPS, configure a domain under Connections → Network and terminate TLS in a reverse proxy such as Caddy. Session and Twitch viewer cookies use `Secure` for that domain; direct IP/localhost access retains HTTP cookies. Only the configured domain and matching HTTPS Origin are accepted. DeckStatus does not manage DNS or certificates. See the [Caddy setup](network.md#https-and-caddy).

## Public Full History and ratings

Share `/history` on the address viewers can reach. No account is needed. History is newest-first, with up to 100 rows per page. It records MASTER observations, not verified audible playback. Restarting the EXE starts a fresh history session.

To vote, viewers sign in with Twitch using the code shown on Full History. DeckStatus uses the Public Twitch Client ID configured in Admin → Twitch, independently of whether automations are enabled or a streamer account is linked. The browser receives an opaque HttpOnly session cookie; OAuth tokens remain in server memory. A DeckStatus administrator login alone does not allow voting.

Votes survive sessions and restarts. The rating ID is a SHA-256 hash of the title/artist/album JSON tuple after ASCII case folding and whitespace normalization. Deck, session ID, pitch and BPM do not participate. Identical tuples share ratings; changed metadata or album editions can create separate records. A non-empty title is required.

New votes are keyed by a hash of the verified Twitch account ID and include the username visible to administrators. Signing into the same Twitch account in another browser updates its existing vote. Older anonymous votes remain in aggregates and are labelled as having no Twitch identity; they are never attributed retroactively. IPs are used only by temporary rate limits. This verifies Twitch accounts, not unique people.

Admin → Track ratings displays average, count and the five-star distribution, with search/sorting. Click a vote count to open a modal with Twitch usernames and individual stars. Full History includes a direct link to this page only for a DeckStatus administrator who has completed the required password change. Public APIs never list other viewers' usernames. See [Twitch viewer sign-in](twitch.md#viewer-sign-in-and-ratings) for session and setup details.

## OBS keys

Protected settings generate URLs with a random `key` query parameter. OBS can load them without interactive login; unkeyed overlays still work in authenticated browsers.

Separate standalone keys authorize deck, master, waveform, text, image or FX renderers and their required read API/cover routes. They grant no account, network or source-control access. Admin → Broadcast links regenerates them, invalidating existing standalone OBS URLs.

Keep these URLs private. Referrer headers are suppressed; keys are forwarded only to the same origin. Renderer URLs come from known DeckStatus paths. Regenerate old unkeyed OBS URLs after upgrading.

## Navigation and component presets

**Start** contains the deck monitor and JSON API. **Stream** contains Scene editor, Full History and the **Scene Components** dropdown for Deck overlays, Master overlay, Waveform, Static text, Images & GIFs and Audio FX. Connections holds the source/network setup pages; Admin is a standalone link. Inactive mode/permission links remain visible and disabled. Anonymous Full History uses its public header.

1. Open Stream → Scene Components and choose a component type.
2. Configure its fields, dimensions, appearance and timing/signal settings. Built-in styles remain available as starting points.
3. Enter a name under **Saved presets** and choose **Save as new**.
4. Select a saved preset and use **Load** to restore it, **Update preset** to save current settings/name over it, or **Delete** to remove it. **Save as new** creates another copy.

The library lives on the DeckStatus server, shared by operators and administrators across browsers, LAN addresses and source modes. It supports up to 200 presets, with names up to 80 UTF-8 bytes. Deck presets include the deck number. Waveform presets include visual/signal settings; audio-device selection is a separate global control and loading a preset never starts capture.

Updates/deletes require the current revision. A concurrent edit returns HTTP 409; reload the list before retrying. Reloading the list does not replace your current overlay settings. Existing browser-local designs remain available and can be saved as named presets.

## Shared audio input

**Admin → Audio input** owns the shared Windows input used by waveforms and scene reactions. Select an input/loopback device and Save, then Start / switch source. Waveform settings now configure only its design. Saving a selection does not start/switch capture.

The device ID/name and **Start audio capture automatically when DeckStatus starts** persist in `portal.json`. The flag defaults to off. Enabled autostart runs once at application startup, before login, using exactly the saved device. Missing devices are reported without fallback. Stop ends the current capture while retaining the selection and policy for the next launch. Remote administrators need Allow remote controls. See [the audio guide](audio-waveform.md) for API details and limitations.

## Master hold time

Brief tempo-master changes reported by Rekordbox or PRO DJ LINK can flip an overlay during track preparation. **Admin → Master detection** sets how long a new deck must hold the role before DeckStatus publishes the handover. The default is 4 seconds; the range is 0 to 30 seconds and 0 restores the immediate switch.

The filter sits between the source and every consumer, so the state API, deck and master overlays, the dashboard badge and Full History use the same confirmed master. While a new candidate is pending, the previously confirmed deck stays master and a handover shorter than the hold does not reach session history. The wait only applies when there is already a confirmed master with a loaded track. The first master after startup or reconnect, a different track on the confirmed deck, and a replacement for a deck that lost its track are published immediately. Disconnected, stale or unsupported states publish no master at all.

The value lives in `portal.json` as `masterSettings.holdMs`, is read at startup and applies to the running server as soon as it is saved. Changing it requires an administrator plus the usual remote-control policy; it is a single server-wide value, so it is not part of an overlay's OBS URL or of a saved preset. It filters observations of tempo-master state and remains no proof of audible playback.

## Scene editor

1. Open Stream → Scene editor.
2. Create a scene and choose a standard monitor resolution or custom dimensions.
3. Choose a saved preset and click **Add to scene**. Use **Reload saved** if a preset was created in another tab. Alternatively, expand the default-component controls.
4. Drag to position and drag the selected corner to resize. Arrow keys move one pixel; Shift moves ten. Numeric fields set exact geometry.
5. Adjust opacity, rotation, visibility, stacking, presets, fonts, colours and fields. Selected layers also support audio-driven scale, position, rotation and opacity; see [Creative scene components](scene-components.md).
6. Save and copy the scene URL into one OBS Browser Source. Match OBS width/height to the scene dimensions.

Limits: canvas 320–7680 px wide and 180–4320 px high, 32 layers per scene, 100 scenes. Layers extending outside the canvas are clipped. Layer size scales track/waveform renderers; source width controls their internal layout. Text, image and FX content renders directly at the layer dimensions. Backgrounds may be transparent or solid.

The saved scene reloads once per second. Geometry/opacity updates retain existing iframes and image elements; changing track/waveform renderer options reloads that iframe. Unsaved edits remain local until Save. Scenes do not start audio capture.

Inserting a preset copies its name and options into an independent layer and sizes the layer from the saved component dimensions. Changing/deleting the preset leaves existing layers intact; editing a scene layer does not update its original preset. To use a revised preset, insert it again and replace the old layer. Multiple instances of the same preset are allowed.

Operators and administrators share scenes. Every mutation requires the current revision; stale saves return 409. Reload the saved version after deciding whether to discard a draft. There is no automatic merge or revision history.

Each scene has a stable key authorizing its document and renderer types present in visible layers. Rotating its link or deleting it revokes the old key. Scene data cannot contain arbitrary HTML, JavaScript, external iframe URLs or filesystem paths.

## Network access

Initially the listener is `127.0.0.1:18740`. Sign in locally as admin and open Connections → Network. Choose localhost, all IPv4 interfaces (`0.0.0.0`) or a specific active IPv4 address, then a TCP port. Save and restart.

Allow DeckStatus.exe on the selected private-network TCP port in Windows Firewall if needed. No firewall rules or router forwards are created. Wildcard listening includes VPN adapters.

The remote-controls switch only concerns Windows audio / ProLink discovery and connections. It does not remove account permissions to edit presets/scenes or administer users. Without it, remote source controls return 403. Only a local administrator can change network settings.

Numeric IPv4, loopback names and one optional HTTPS domain on port 443 are supported. Store the domain alone as `publicDomain` in `DeckStatus.network.json`, or use the Network page, then restart. Older files without this property keep domain access disabled. Host, Origin and cross-site checks remain active; other domains, ports and subdomains are rejected. IPv6 listeners and path-prefix hosting are not supported.

A specific interface also starts a loopback listener on the same port. Both listeners share accounts, audio and scene state; startup fails if either required socket cannot bind. Domain requests use remote permissions even if the proxy connects locally. Network configuration requires a local administrator accessing the listener by IP/localhost. Forwarding headers are not trusted for local privileges or Host/Origin validation; proxies must preserve Host and Origin. IP-based rate limits use the proxy's TCP address and are shared by clients behind it.

Copied deck/master/waveform/scene URLs always use `http://127.0.0.1:<port>` for OBS on the DeckStatus PC, including when settings are opened through the domain. `/api/app` exposes this origin as `obsBaseUrl`. Embedded previews and renderer API requests stay on their current origin. For OBS on another PC, replace the copied URL's origin with the reachable LAN/HTTPS address, keeping its path and query intact. Browser preferences use separate origins for localhost, LAN and the domain. HTTP clipboard fallback is selecting the URL and pressing Ctrl+C.

## ProLink setup

Connect the PC and supported players/mixers on the same Ethernet network with unique player numbers. Multiple announced XDJ-AZ player endpoints may share an IP address. Start ProLink mode, sign in, open Connections → ProLink setup, find devices and connect selected players.

One to four selected players map to dashboard decks in ascending number order; players 5/6 are selectable. The mixer is detected automatically. Disconnect before changing selection. Merely opening setup performs no discovery.

Allow the bundled `prolink/runtime/bin/java.exe` on the private DJ network. PRO DJ LINK uses UDP 50000–50002 and TCP DBServer traffic (port discovery on 12523, then the player's reported port). Other Link clients may already occupy those ports. The legacy NFS/export.pdb metadata fallback is disabled to avoid mismatched OneLibrary IDs.

The helper uses a monitoring identity and normal metadata handshakes, not passive sniffing. No playback/load/sync/tempo or mixer fader/EQ/FX controls are exposed. Playing, Sync and On-Air flags can be reported; On-Air does not prove audible output. Stale reports stop appearing live. ProLink track IDs remain session-scoped and distinguish player/media sources.

## Storage and recovery

| Location | Purpose |
|---|---|
| `DeckStatus.data/portal.json` | Accounts, hashes, votes, presets, scenes, media metadata, audio settings and keys |
| `DeckStatus.data/media/` | Uploaded images and GIFs, one file per asset named by its SHA-256 |
| `DeckStatus.data/portal.lock` | Exclusive store ownership |
| `DeckStatus.network.json` | Listener, port and remote-control setting |
| Browser local storage | Last-used component settings and UI language |

Use `--data-dir PATH` to choose another data directory. Two instances cannot share one store. Saves use flushed temporary files and atomic replacement. Failed saves retain the old state; invalid data stops startup instead of resetting accounts. Maximum store size is 64 MiB.

Creative-component upgrades add an empty `media` collection and text/image/FX read keys while preserving existing keys. Uploaded files are stored beside the store in `media/`, named by their content hash, so saving a vote, preset or scene never rewrites image data. A store written by DeckStatus 2.1.0 or older keeps the images Base64-encoded inside `portal.json`; the first start moves them into `media/` and removes them from the JSON, after which an older build no longer finds them. Back up the entire data directory before upgrading and restore that complete backup to downgrade; copying only `portal.json` is no longer sufficient. Unreferenced files in `media/` are removed at startup. See the [media limits and API](scene-components.md#api-and-storage).

Existing version-1 stores without a `presets` property are upgraded with an empty library. Accounts, hashes, ratings, scenes and keys are preserved. Presets are stored by ID as `{id,revision,name,type,options}`; no external URLs, OBS keys or audio-device IDs are accepted in their options.

Stop DeckStatus before backing up the directory and network JSON. Preserve both when upgrading. Backups contain password hashes and active keys; keep them private. Standard data/configuration files are ignored by Git and excluded from fresh packages.

Another admin can reset a forgotten password. Otherwise restore a known backup or choose a new, separate data directory to create a fresh administrator; keep the original directory and its records. There is no unauthenticated HTTP recovery.

Finish the first password change before moving the store between Windows accounts/PCs, because bootstrap decryption belongs to the creator. Ordinary hashes and records remain portable afterwards. Restart always expires sessions.

## Command-line examples

~~~powershell
.\DeckStatus.exe
.\DeckStatus.exe --demo --data-dir 'D:\DeckStatus Demo'
.\DeckStatus.exe --mode prolink --bind 192.168.1.20 --port 18740
.\DeckStatus.exe --bind 0.0.0.0 --allow-remote-control
.\DeckStatus.exe --network-config 'D:\DeckStatus\network.json'
.\DeckStatus.exe --lang de
~~~

CLI network options override saved fields for that launch without rewriting them. Source mode is per launch. `--pid`/`--database` are Rekordbox-only; `--demo` cannot combine with ProLink.

## HTTP API

Mutations need `Content-Type: application/json`. Retain cookies and use same-origin requests. Errors: 400 input, 401 login, 403 permissions/password change, 409 conflict, 415 content type, 429 temporary request limit.

| Route | Purpose |
|---|---|
| `GET /api/auth/me` | Public identity or null |
| `POST /api/auth/login` | `{username,password}`; session cookie |
| `POST /api/auth/logout` | `{}`; revoke session |
| `POST /api/auth/password` | `{currentPassword,password}`; signs out |
| `GET /api/app` | Signed-in mode, capabilities, user |
| `GET /api/state`, `/api/decks`, `/api/decks/{1..4}` | Live metadata |
| `GET /api/master` | Current master and recent overlay history |
| `GET /api/history?limit=100&before=...` | Public running-session history and ratings |
| `GET /api/history/covers/{trackId}` | Public observed-track artwork |
| `GET /api/public/twitch` | Current viewer identity / pending authorization; no tokens |
| `POST /api/public/twitch` | `{action:"start"|"poll"|"logout"}`; separate viewer session cookie |
| `POST /api/public/rating` | `{track:ratingId,stars:1..5}` with validated Twitch viewer session |
| `GET /api/admin/ratings` | Admin persistent aggregates |
| `GET /api/admin/ratings/:trackId/viewers` | Admin-only usernames, individual stars and legacy anonymous count |
| `GET/POST /api/admin/master` | Admin server-wide master hold time; `{holdMs:0..30000}` |
| `GET/POST /api/admin/users` | Admin user list/save/delete |
| `GET/POST /api/scenes` | Signed-in list/save/delete/rotate |
| `GET/POST /api/presets` | Signed-in component library/list/save/delete |
| `GET/POST /api/media` | Signed-in media library/upload/delete |
| `GET /api/media/{id}` | Original image/GIF through session or scoped read key |
| `GET /api/scene?scene=ID&key=KEY` | Document via session or scene key |
| `GET /api/broadcast` | Signed-in standalone keys |
| `POST /api/admin/broadcast` | Admin `{}` regenerates keys |
| `GET/POST /api/network` | Admin; POST also requires local peer |
| `GET /api/prolink/devices`, `POST /api/prolink/control` | Mode and remote gates apply |
| `GET /api/audio/devices`, `/api/audio/state` | Signed-in device list / scoped sample reads |
| `GET/POST /api/admin/audio` | Admin saved device, autostart, live state and start/stop; remote gate on POST |
| `POST /api/audio/source` | Admin compatibility start/switch/stop; remote gate applies |
| `GET /api/health` | Signed-in; 200 connected/demo, otherwise 503 |

Other than public routes above, APIs require a session or a matching read capability. Keys never authorize mutations.

Each route declares its access level explicitly through `Access` in `src/portal_http.h`; registration has no default permission. Portal reads share a lock, while password derivation and media file I/O run outside it. Authentication rechecks the password record before completing a login or password change.

User save: `{action:"save",id:"optional ID",username:"name",role:"admin"|"operator",password:"new/reset or empty"}`. Delete: `{action:"delete",id:"ID"}`.

Scene save: `{action:"save",id:"optional ID",revision:0,scene:{name,width,height,background,items}}`. Existing documents require the current revision. Delete/rotate: `{action:"delete"|"rotate",id,revision}`.

Preset save: `{action:"save",id:"optional ID",revision:0,preset:{name,type,options}}`. New presets receive an ID and revision 1; updates require their current revision and cannot change type. Delete: `{action:"delete",id,revision}`. GET returns `{presets:[...]}`; a successful save returns the saved preset. Anonymous users and OBS keys cannot access this library.

Items: `{id,type,x,y,width,height,opacity,visible,options}`, with optional `name` and `rotation` (degrees), where type is `deck`, `master`, `waveform`, `text`, `image` or `fx`. Array order defines stacking back to front.

`bpm` is pitch-adjusted; `originalBpm` comes from metadata. Unknown fields are null. Timelines use milliseconds and can include negative preroll. Master entries add `entryId`, `startedAt`, `endedAt` and `isMaster`.

## Build and validation

Requires Windows x64, Visual Studio C++ desktop tools/CMake, JDK 21+ for the helper, Node.js 22+ and Microsoft Edge for browser tests.

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
node tests/browser_master_test.cjs
node tests/browser_admin_master_test.cjs
node tests/browser_waveform_test.cjs
node tests/browser_dashboard_history_test.cjs
node tests/browser_prolink_test.cjs
node tests/browser_network_test.cjs
node tests/browser_portal_test.cjs
node tests/browser_creative_test.cjs
node tests/browser_creative_portal_test.cjs
node tests/browser_admin_audio_test.cjs
node tests/audio_startup_smoke.cjs
node tests/network_smoke.cjs
~~~

`build.ps1` runs native CTest checks and builds/tests Java. `-SkipProLink` omits Java, not suitable for a complete ProLink distribution. Downloads are pinned in `prolink/dependencies.lock.json`.

`CMakeLists.txt` is the single product-version source. Configuration generates `deckstatus_version.h` for the native API and EXE/DLL resources; `tools/version.cjs` reads the same declaration for JavaScript tests and screenshot tooling. Reconfigure and rebuild after changing the version.

Native portal tests cover password lifecycle, sessions, roles, last-admin protection, failed-save preservation, persistent ratings/presets/scenes, legacy-store migration, preset validation/conflicts, key scope/rotation and real HTTP boundaries. Browser portal tests launch the actual EXE with isolated demo data and test login/users, public history and anonymous-vote rejection, all three preset types, independent scene copies, drag/save/mobile/EN-DE, anonymous OBS iframes, conflicts and restart persistence. Navigation tests include the grouped dropdown, keyboard/Escape, outside-click closing, mobile layout and mode gates.

Network smoke uses authenticated demo and idle ProLink instances. Production injection, DJ device connection and audio capture remain off. Optional portable helper discovery requires a separate explicit opt-in.

Local LAN-address checks do not verify another PC's firewall or OBS installation. Chromium rendering is not a live OBS Studio plugin test. Automated tests do not establish additional Rekordbox or real ProLink compatibility.

## Troubleshooting

- **Password redirect:** complete the required change and sign in again.
- **OBS asks for login:** generate a new URL including its key.
- **Old scene layout:** save, wait one second and check OBS dimensions.
- **Save conflict:** reload after preserving required draft changes.
- **Missing ratings:** wait for a title; changed metadata tuples create separate records.
- **429:** wait one minute before retrying.
- **Store in use:** stop the other process or select another directory.
- **Save failure:** check folder rights/disk space; prior data remains intact.
- **LAN failure:** restart after saving and check IP, port and firewall.
- **ProLink ports busy:** stop competing Link clients.
- **No waveform:** check Admin → Audio input and start the saved device. Review missing-device/autostart errors there; opening a scene does not start capture.

## References

- [Windows CNG derivation](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptderivekeypbkdf2)
- [OWASP password storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
- [Beat Link](https://github.com/Deep-Symmetry/beat-link)
- [PRO DJ LINK analysis](https://djl-analysis.deepsymmetry.org/djl-analysis/)
