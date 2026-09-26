# Track timeline designs

In **Stream → Scene Components → Master overlay** or **Deck overlays**, enable **Timeline** and open **Timeline design**. Save a preset to apply the design to linked scene layers. Existing designs retain the classic progress bar.

| Design | Display |
|---|---|
| Progress bar | Existing elapsed/total time and filled bar |
| Segmented bar | Progress divided into small visual segments |
| Progress ring | Circular progress with a percentage |
| Scrolling track waveform | Analyzed waveform moving past a fixed central playhead |
| Full-track waveform | Overview of the complete track and its moving playhead |

Canvas designs have an adjustable height of 32–200 px. The scrolling view shows 4–60 seconds of track time. Waveforms can use their analysis colors or the overlay accent color. The existing per-element timeline font and foreground/background overrides continue to style the labels. A master overlay only shows the current track's timeline; history cards do not fetch or draw waveforms.

## Data sources

- **Rekordbox:** the host reads `djmdContent.AnalysisDataPath` through the existing read-only database connection, then opens the corresponding `.EXT` analysis file within the collection directory. It reads RGB `PWV5` detail, or blue `PWV3` detail as a fallback. Missing files, unknown formats and paths outside the collection are rejected. The bridge and executable compatibility profiles are unchanged.
- **ProLink:** the bundled Beat Link `WaveformFinder` retrieves detail through the existing metadata connection. Returned track references must match the selected player's current metadata. Crate Digger/DeviceSQL auto-start remains disabled. Support depends on the device, source media and available analysis; no firmware certification is implied.
- The separate **Waveform** scene component still visualizes the Windows audio input. Track timelines do not start audio capture and do not manufacture a waveform from BPM or volume.

An unavailable waveform falls back to the ordinary progress bar with an explanation. Missing position/duration remains unknown; the existing “hide missing data” option can hide that timeline entirely. Pauses freeze the waveform, seeks update its position, and stale feeds remove the current card. Animation respects reduced-motion preferences.

## Transport and limits

`GET /api/master/waveform?trackId=…` and `GET /api/decks/1/waveform?trackId=…` (Decks 1–4) require a signed-in user or the matching scoped master/deck/scene OBS key. The server verifies the current loaded track before and after lookup. Arbitrary library IDs and historical tracks cannot be requested through these endpoints. Full History does not expose this API publicly.

Available responses contain `available`, `trackId`, `format: "rgb5"`, `durationMs` and `samples`. Samples are unsigned packed RGB/height values, reduced by peak selection to at most 30,000 columns. Original duration is preserved. The parser accepts at most 20 MiB and four hours of detailed analysis. Native caches hold at most eight tracks; successful local reads expire after five minutes, missing local files after five seconds. Samples are not included in ordinary state polls or session history.

The renderer loads once per track/URL, retries unavailable data at most once every five seconds and aborts obsolete requests. Drawing is capped at 30 fps, 1,600 logical pixels and 2× pixel density. It stops animation while hidden, paused or stale, and extrapolates position for at most 500 ms between reports.

Format references: [Deep Symmetry analysis-file documentation](https://djl-analysis.deepsymmetry.org/rekordbox-export-analysis/anlz.html), [pyrekordbox analysis paths](https://pyrekordbox.readthedocs.io/en/stable/tutorial/anlz.html), and [Beat Link](https://github.com/Deep-Symmetry/beat-link). Synthetic parser/transport/browser tests do not replace live Rekordbox/CDJ waveform validation.
