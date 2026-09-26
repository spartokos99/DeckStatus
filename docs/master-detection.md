# Master detection

Open **Admin → Master detection**. Changes require an administrator and, for remote browsers, enabled remote control. Save applies the settings to the running server and persists them across restarts. All overlays, the dashboard, JSON state and session history share the same result.

## Tempo master (default)

This remains the default in both source modes. The device/software-reported tempo master must hold that role for the configured time before replacing a confirmed, loaded master. The default is 4 seconds, adjustable from 0 to 30. Startup, reconnection and replacement of an unloaded master are immediate. Rekordbox behavior is unchanged.

## Playing track (ProLink only)

This alternative is inspired by [prolink-connect's SmartTiming processor](https://github.com/evanpurkhiser/prolink-connect/blob/main/src/mixstatus/index.ts), used by [prolink-tools](https://github.com/evanpurkhiser/prolink-tools). It observes playback time and optional mixer On-Air status instead of following the tempo-master flag.

| Setting | Default | Meaning |
|---|---|---|
| Beats before detecting a new track | 128 | Eligible playing beats before a new track replaces the current one during a mix; range 1–1024 |
| Allowed interruption | 16 | Grace period for pauses or leaving On Air; range 0–1024 beats |
| Require On Air from the mixer | On | Only playing decks with a fresh, true On-Air status qualify |

- The first eligible track is shown immediately. When the current track unloads, reaches its end or returns to cue, the longest-playing eligible deck takes over without waiting for the detection threshold.
- During overlap, a new track takes over after its detection count. A track already reported will not repeatedly steal the master back. It can become the fallback when the newer track stops.
- Short interruptions pause the count and preserve the current master. Longer interruptions reset that track's count and permit a fallback. Loading another track always resets its count, even on the same deck.
- Beats are integrated from elapsed playing time and effective BPM. Seeking, beat-number jumps and repeated HTTP polls do not add beats. A loop continues to accumulate playing time.
- Unknown On Air does not qualify. Disable the checkbox when operating without a supported mixer or when On Air is not provided reliably. Playback-only detection cannot distinguish headphone cueing from audience playback.
- **128 beats are 60 seconds at 128 BPM.** Lower the threshold for earlier overlap detection. This method is not universally faster; the tempo-master hold setting does not apply to it.

Missing current BPM prevents detection-beat accumulation. Interruption timing uses the last known valid tempo, or 120 BPM solely as a timeout reference when none exists; this never becomes track metadata. Long scheduling gaps earn at most one second per observation. Disconnection/stale source data clears detection state. The method does not measure actual audio or guarantee which track the audience hears.

Settings live in `portal.json` under `masterSettings`: `holdMs`, `prolinkMethod` (`tempo`/`smart`), `detectionBeats`, `interruptBeats`, `useOnAir`. Older hold-only stores remain valid. `GET/POST /api/admin/master` expose/update this object; partial updates preserve other settings. Method-specific controls appear only in ProLink mode.

Validation uses synthetic playback transitions, pauses, off-air periods, unknown statuses, seeks and source changes. Live CDJ/mixer validation of this new method is still pending.
