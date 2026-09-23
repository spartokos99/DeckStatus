# Application updates

The updater is included in **DeckStatus 2.3.2 and later**. Earlier ZIPs do not contain it: install the complete 2.3.2 package manually once before using the updater for future releases. Version numbers come exclusively from `CMakeLists.txt`.

## Check and prepare

DeckStatus checks the latest stable release in [spartokos99/DeckStatus](https://github.com/spartokos99/DeckStatus/releases) at startup and every six hours. A newer version produces a console message and an indicator in the application navigation. Operators can follow the release link; administrators can open **Admin → Updater**. Anonymous Full History visitors do not see the indicator.

Checks run in a background worker and compare numeric `major.minor.patch` versions. Drafts, prereleases and other tag formats are rejected. A local build ahead of GitHub is not an available update. Manual checks have a one-minute cooldown. Demo mode and the `DECKSTATUS_NO_UPDATE_CHECK=1` environment variable disable automatic checks; explicit administrator checks remain available.

In **Admin → Updater**, choose either:

- **Download and prepare:** downloads the newer stable Windows ZIP from the fixed repository and verifies its SHA-256 against GitHub's release asset digest or the adjacent `.zip.sha256` file. Missing verifiable assets disable this action.
- **Upload and prepare:** select a complete official Windows x64 release ZIP. Uploads use bounded 4 MiB chunks. The calculated SHA-256 is shown for comparison; uploading does not establish the publisher's identity or verify a digital signature.

Preparation leaves the running application intact. It rejects downgrades, mismatched EXE/DLL versions, other architectures, incomplete core web/Java packages, invalid dependency hashes, unsafe paths, duplicate entries and private data files. Same-version packages are allowed for repairs. Limits are **512 MiB compressed**, **1 GiB extracted**, and **10,000 entries**. Discard an interrupted upload before trying again.

## Install

1. Review the prepared version, checksum and source.
2. Select **Install and restart** and confirm the interruption.
3. The helper rechecks every staged file, verifies the running process identity and waits for DeckStatus to shut down.
4. It backs up the complete configured data directory and network configuration, then replaces managed program files.
5. It restarts DeckStatus with the same arguments and working directory. Sign in again after the restart; OBS Browser Sources can reconnect using their existing URLs.

Accounts, ratings, presets, scene links, media and saved settings are preserved. Custom `--data-dir` and `--network-config` locations are supported when they do not overlap managed application files or `DeckStatus.update`. Played-track history and login sessions remain session-local and reset on restart. Saved audio autostart and ProLink connection preferences apply again through normal startup.

The restarted process runs in the background. To return to an interactive console, end that DeckStatus instance in Task Manager and launch it normally with the same arguments. The updater is designed for a writable portable installation running under the same Windows account, not Windows services or installations requiring elevation. Windows PowerShell 5.1 must be available. A second DeckStatus process using the same installation must be closed first.

Only administrators with control permission can check, upload, download or install through the API. Remote administrators also require **Allow remote control** in Network settings. Reading update status does not grant this permission. Existing Host/Origin validation and the mandatory initial password change still apply.

## Backups and recovery

Jobs live under `DeckStatus.update/job-<random-id>/`. The directory is private local state, excluded from Git and release packages and not served over HTTP. It may contain:

| Path | Contents |
|---|---|
| `package.zip`, `stage/`, `manifest.json` | Uploaded/downloaded ZIP, validated files and their hashes |
| `backup/` | Replaced program files |
| `data-backup/`, `network-backup.json` | Complete pre-update private data and network settings |
| `journal.json`, `result.json` | File replacement progress and outcome |
| `failed/`, `failed-data/` | Retained failed candidate and data, when applicable |

Normal replacement/startup failures trigger restoration of the previous program. If the new process started and then failed, the helper also restores the saved data before restarting the old program. Startup verification checks that the new process remains alive for six seconds; it does not certify every feature or hardware connection.

An abrupt Windows shutdown, disk failure or a failure during rollback can require manual recovery. Stop DeckStatus and any updater helper for **that installation**, restore its managed program files from `backup/` or the previous complete ZIP, and restore `data-backup/` and `network-backup.json` to their original configured locations. The job's `plan.json` records those locations; `journal.json` records file moves. Keep the failed job until recovery is complete. Do not mix a downgraded program with data migrated by a newer version.

Backups and abandoned uploads are intentionally retained. After verifying the updated application and preserving any needed backup elsewhere, remove completed job directories while DeckStatus and its updater are stopped. Keep backups private: they contain user data and authentication material. Allow enough free disk space for the ZIP, extracted program, previous program and a complete data backup.

## Implementation and tests

`src/updater.cpp` owns cached status, the background worker, ordered uploads and the shutdown handoff. The host's HTTP thread never performs downloads or extraction. `tools/DeckStatus.Update.ps1` is installed beside the EXE as `DeckStatus.Update.ps1`; a private job copy performs HTTPS requests, validation, replacement and rollback. No external package manager or additional runtime is needed beyond Windows PowerShell.

Managed roots are `DeckStatus.exe`, `DeckStatusBridge.dll`, `DeckStatus.Update.ps1`, `Start-ProLink.cmd`, `README.md`, `CHANGELOG.md`, `THIRD_PARTY_NOTICES.md`, `web`, `docs`, `prolink` and `vendor`. Present directories are replaced as units, removing obsolete files. Other root files remain untouched. Older same-version repair ZIPs may omit the updater helper; the installed helper is retained in that case. Symbolic links/junctions in installation, staged or backed-up data paths are rejected.

The transport uses the public [GitHub latest release API](https://docs.github.com/en/rest/releases/releases#get-the-latest-release), without a GitHub token. Release names must match `DeckStatus-<version>-win-x64.zip`. Downloads and redirects are restricted to HTTPS GitHub API/release asset hosts, with bounded size and timeouts. A failed check does not stop DJ metadata or overlays.

Routes use explicit administrator access: `GET/POST /api/admin/updater` and `POST /api/admin/updater/upload`. The signed-in `/api/app` response exposes only the available version and release URL. The administrator response additionally reports preparation, package checksums, control permission and the last backup location.

Run after the full native/Java build:

```powershell
node tests/updater_test.cjs
node tests/browser_updater_test.cjs
```

The first suite creates a complete disposable package and runs only its own demo installation. It tests release selection using a fixture transport, both digest formats, permissions, ordered uploads, archive attacks, downgrade rejection, staged-file tampering, real restart/data preservation and rollback with a locked DLL. The browser suite tests the indicator, confirmation, translations, mobile layout and failed/remote controls. Neither starts production injection, ProLink networking or audio capture. See the dated [validation log](validation.md) for actual runs and limits.
