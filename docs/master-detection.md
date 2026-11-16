# Master detection

Open **Admin → Master detection**. Rekordbox and ProLink use one shared now-playing detector. The former tempo-master hold and optional ProLink detector have been removed. Device/software tempo-master flags no longer select the displayed MASTER.

The four controls follow **ProlinkTools 0.1.0-beta.5**, whose `prolink-connect ^0.5.0` dependency implements the [SmartTiming/end/interruption rules](https://github.com/evanpurkhiser/prolink-connect/blob/v0.5.0/src/mixstatus/index.ts). DeckStatus implements these rules independently, with the source-availability protections below. It does not run or bundle ProlinkTools.

| Setting | Default | Meaning |
|---|---|---|
| Smart timing beat count | 128 | Playback time required before an incoming track can take over during overlap; 1–1024 beats |
| Allowed beats during interruptions | 16 | Grace period for pausing or an already reported track leaving On Air; 0–1024 beats |
| Only report after the last track ends | Off | Disable overlap smart timing; wait for the current track to end, unload, return to cue or exceed the interruption allowance |
| Use On-Air status | On | Require a known, true mixer On-Air signal in ProLink; unavailable in Rekordbox |

Save applies the settings server-wide and persists them across restarts. Administrator access and the usual remote-control permission are required. Presets and OBS URLs do not contain detection settings.

## Timing and history

- The first eligible playing track is reported immediately. When the current track ends, the longest-playing eligible **unreported** deck takes over without waiting for the smart threshold.
- During overlap, playback elapsed time is converted to beats using the current effective BPM, not the track's original BPM or beat/position counters. **128 beats at 128 BPM take 60 seconds.** Changing tempo changes the required elapsed time.
- Playback starts the timer even before a track comes On Air. On-Air status gates reporting, not pre-listening time. Without a supported mixer, disable On Air explicitly; otherwise an unknown signal cannot qualify.
- Short pauses/cuts preserve the playback start, including the elapsed short interruption, as in ProlinkTools. Reaching the interruption allowance resets that deck's episode. Zero allowance ends it immediately. A different loaded track also starts a new episode.
- Already reported outgoing tracks **never become a fallback** merely because a newer track stops. They can qualify again after an actual stop, sufficiently long interruption, or new load. This removes the old A → B → A fallback that produced duplicate history during an overlap.
- Unknown current BPM freezes timing; no guessed tempo is substituted. Missing playback/required On-Air observations freeze both timers. Scheduling gaps longer than one second earn no time. Seeks and loops do not add their position delta to the timer.

Only the source sampler advances detection. HTTP reads use a published snapshot; opening more browsers cannot change detection or add history entries. All state/overlay/dashboard/history consumers share this result. History remains session-local, and legitimate later replays are not globally deduplicated by title.

## Source differences

**ProLink** supplies playing/looping, cue/end and mixer On-Air states directly. Existing exact device profiles, three-second packet freshness checks and source/player/media track identities remain in force. This does not add support for other players or XDJ-AZ standalone four-deck operation.

**Rekordbox** exposes verified timeline positions, but the currently audited bridge profiles do not expose transport or mixer On-Air signals. Playback is therefore inferred from two consecutive plausible forward position samples. A stationary position for 500 ms indicates pause; a stationary cue/start or end position indicates stop. Repeated bridge timestamps are unavailable observations, not evidence of a pause. Loop/seek jumps do not themselves trigger a handover.

The same timing engine operates on those inferred states. The UI explicitly identifies this limitation and disables On Air in Rekordbox while preserving its saved ProLink preference. Missing timelines prevent detection; unusual scratching, seeks or very short loops can be ambiguous. There is no fallback to tempo master and no new unchecked memory offset. Exact Rekordbox executable/profile checks remain unchanged; bridge IPC is now version 4, so keep host and DLL together.

Neither mode proves audible playback. Live validation of this rewritten detector on Rekordbox, CDJs/mixer and OBS remains pending.

## Interruptions and metadata cache

An unavailable player is not an unloaded or stopped player. The detector preserves episode identity and freezes timers across unavailable observations. It protects a missing current deck for **10 seconds**, after which an eligible unreported deck may take over. A returning already reported deck cannot steal the master back merely by reconnecting.

Deck/master overlays retain their last card for up to **10 seconds after the last available response**. This is display-only: a visible **cached / connection interrupted** label replaces the live label, timelines stop/hide, and the same card/artwork stays in place. The cache also works in embedded previews and scene layers. It clears on expiry, confirmed unload/end or access rejection (401/403); it never feeds history or detector timing.

Missing static metadata on the same track retains previously known title, artist, album, key, label, genre, original BPM and duration. The server retains that metadata for the loaded track episode; the browser additionally preserves its approved cover URL. A different track identity cannot inherit it. A `sourceSession` marker also prevents reusing metadata/cards/artwork after a server restart that reuses numeric track IDs. Current BPM is not replaced with original BPM or cached as live during a metadata miss. Unknown values remain unknown until genuinely supplied.

## Stored settings and API

`portal.json.masterSettings` contains only:

```json
{"detectionBeats":128,"interruptBeats":16,"onlyAfterEnd":false,"useOnAir":true}
```

Old stores migrate atomically: valid beat/interruption/On-Air choices survive, `onlyAfterEnd` defaults to false, and `holdMs` / `prolinkMethod` are discarded. Other users, keys, votes, scenes and presets are preserved. This intentionally changes detection for installations previously using tempo master; old hold time no longer has any effect.

`GET/POST /api/admin/master` supports validated partial updates. GET also reports `available`, `canControl`, `onAirAvailable` and `playbackSource` (`position` or `device`). POST rejects old method/hold keys. State and master feeds expose `masterAvailable`; deck feeds expose `statusAvailable` to distinguish missing observations from explicit empty decks. Rekordbox state includes monotonic `sampleTick` for detecting repeated bridge samples.

See [validation](validation.md) for synthetic timing, migration, IPC and browser-cache coverage and its hardware limits.
