<p align="center">
  <img src="docs/images/deckstatus-icon.png" width="96" height="96" alt="DeckStatus logo">
</p>

<h1 align="center">DeckStatus</h1>

<p align="center">Live DJ metadata, audience ratings and configurable OBS overlays for Windows.</p>

<p align="center">
  <a href="https://github.com/spartokos99/DeckStatus/releases/latest"><img src="https://img.shields.io/github/v/release/spartokos99/DeckStatus?style=flat-square&amp;color=6ae9b3" alt="Latest GitHub release"></a>
  <img src="https://img.shields.io/badge/platform-Windows%20x64-0078d4?style=flat-square" alt="Platform: Windows x64">
  <img src="https://img.shields.io/badge/OBS-Browser%20Source-8b5cf6?style=flat-square" alt="OBS Browser Source">
  <img src="https://img.shields.io/badge/languages-EN%20%7C%20DE-f2b84b?style=flat-square" alt="Languages: English and German">
</p>

<p align="center">
  <a href="https://github.com/spartokos99/DeckStatus/releases/latest">📦 Download</a> ·
  <a href="docs/WIKI.md">📖 Technical guide</a> ·
  <a href="CHANGELOG.md">📝 Changelog</a>
</p>

> [!IMPORTANT]
> Rekordbox support has been live-tested on **Rekordbox 7.2.18.0 Windows x64** installation. ProLink support is experimental.
> 
> I should also mention that the entire tool was 100% AI-generated; I originally intended to use it just for my own stream, but it turned out so well and proved so practical that I decided to share it so other streamers could use it too.

## ✨ Features

- 🎛️ **Four-deck dashboard:** title, artist, album, label, key, cover, current/original BPM and timelines.
- 🎨 **OBS overlays:** individual decks, current master with adjustable history, and six audio waveform styles - fully customizable.
- ⏱️ **Stable master detection:** filter brief tempo-master handovers with a shared hold time for overlays, the dashboard and Full History.
- 🧩 **Scene editor:** insert saved presets on a monitor-sized canvas.
- 🔄 **Updater:** release indicators, ZIP upload or verified GitHub download, backups and restart under **Admin → Updater**. See the [update guide](docs/updater.md).
- 🎶 **Public Full History:** viewers browse played tracks at `/history` without an account. Rating tracks requires a separate Twitch sign-in.
- ⭐ **Persistent ratings:** the admin panel shows averages, vote counts and star distributions across sessions.
- 🖼️ **Creative components:** static text, uploaded images/GIFs, optional Wikimedia Commons import, and audio-driven fog/flash effects.
- ⚡ **Twitch automations:** trigger up to 16 actions per rule with rewards, chat commands/text, raids or stream status. Change layers, toggle audio reactivity or reply in chat, with individual durations and shared cooldowns.
- 🎙️ **Shared audio input:** choose and save a device under Admin → Audio input.
- 🔐 **Accounts:** administrator/operator roles, user management and required initial password changes.
- 🌐 **Optional LAN access and HTTPS domain support** behind a reverse proxy, JSON API, Rekordbox/ProLink modes and English/German application translations.

## 🧪 Compatibility
  
### 🍦 Rekordbox (Software)

Live-tested only with the author's **Rekordbox 7.2.18.0 Windows x64 installation**. Version 2.3.1 recognizes the original executable and one specifically audited patched variant using exact SHA-256/PE profiles and loaded-code checks. Other patched files and Rekordbox versions remain unsupported; see the [profile identities](docs/rekordbox-7.2.18.md) and [validation record](docs/validation.md).

|                    | Windows | macOS |
|:------------------:|:-------:|:-----:|
| Rekordbox 7.2.18.0 |    ✅    |   ❌   |

### 🧰 Pro DJ Link (Hardware - Experimental)

|             	| **Implemented** 	| **Tested** 	|
|:-----------:	|:-----------:	|:--------:	|
|   **CDJs**  	|             	|          	|
|   CDJ-3000  	|      ✅      	|      ✅    	|
|  CDJ-3000X  	|      ✅      	|      ❌    	|
|             	|             	|          	|
|  **MIXERS** 	|             	|          	|
|    DJM-A9   	|      ✅      	|      ❌    	|
| DJM-900NXS2 	|      ✅      	|      ✅    	|
|             	|             	|          	|
|   **AiO**   	|             	|          	|
|    XDJ-AZ   	|      ✅      	|      ❌    	|

I successfully discovered 3× CDJ-3000 and a DJM-900NXS2; See the [device and metadata limitations](docs/prolink.md).

## ▶️ Run

1. Extract the full Windows ZIP into a writable folder.
2. Start Rekordbox, then run **DeckStatus.exe**. For ProLink, use **Start-ProLink.cmd**.
3. Open **http://127.0.0.1:18740**.
4. On first start, sign in as **admin** with the temporary password printed in the DeckStatus console. Set a new password, then sign in again.

Stop with **Ctrl+C**. Use `DeckStatus.exe --demo` to explore synthetic tracks without Rekordbox or DJ hardware.

For upgrades, stop DeckStatus and back up **DeckStatus.data** and **DeckStatus.network.json** before replacing application files. Existing users, presets, scenes, ratings and OBS links remain valid. Versions 2.2.0 and later automatically move uploaded images/GIFs into **DeckStatus.data/media**; keep the entire data directory together. Restore the pre-upgrade backup if you need to downgrade.

Master handovers now wait **4 seconds** by default; the first master after startup or reconnect appears immediately. Set **Admin → Master detection** to **0** for immediate handovers.

Configure Twitch under **Admin → Twitch** and edit rules under **Stream → Automations**. Viewer sign-in uses the same Public Twitch Client ID. Live Twitch account/OBS validation is still pending; see the [Twitch setup guide](docs/twitch.md).

This package is **DeckStatus v2.3.2** (`DeckStatus-2.3.2-win-x64.zip`). Published versions are available from [GitHub Releases](https://github.com/spartokos99/DeckStatus/releases).

Version 2.3.2 includes per-element font size/style/weight, editable scene deck assignments and the updater. See the [changelog](CHANGELOG.md) for all changes since the previous public release.

## 🛠️ Build and test

Requires Windows x64, Visual Studio C++ tools, CMake and JDK 21+ for ProLink.

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
node tests/browser_master_test.cjs
node tests/browser_track_design_test.cjs
node tests/browser_waveform_test.cjs
node tests/browser_dashboard_history_test.cjs
node tests/browser_prolink_test.cjs
node tests/browser_network_test.cjs
node tests/browser_portal_test.cjs
node tests/browser_creative_test.cjs
node tests/browser_admin_audio_test.cjs
node tests/browser_admin_master_test.cjs
node tests/browser_updater_test.cjs
node tests/updater_test.cjs
node tests/browser_twitch_layers_test.cjs
node tests/browser_viewer_ratings_test.cjs
node tests/network_smoke.cjs
~~~

Browser tests require Node.js 22+ and Microsoft Edge. Native tests cover injection fixtures, metadata, HTTP, audio, ProLink, network settings, authentication, ratings, presets, scenes and storage migration. Twitch protocol tests use an isolated transport and do not contact a real account. The portal browser test runs the real EXE with isolated data, including preset reuse, anonymous OBS rendering and restart persistence. Automated tests do not establish additional Rekordbox or real ProLink compatibility.

Set `$env:DECKSTATUS_TEST_PROXY='1'` before the portal browser test to run it through an isolated HTTPS proxy, including Secure cookies, local OBS links and same-origin previews. Unset it afterwards with `Remove-Item Env:DECKSTATUS_TEST_PROXY`.

See the [technical wiki](docs/WIKI.md) for setup, permissions, APIs, storage and troubleshooting, and [third-party notices](THIRD_PARTY_NOTICES.md) for dependencies.
