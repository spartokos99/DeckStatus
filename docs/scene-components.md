# Creative scene components

These additions are included in DeckStatus 2.1.0. They work in both Rekordbox and ProLink modes and use the same accounts, preset library and OBS scene links as existing overlays.

## Build a scene

1. Open **Stream → Scene Components** and choose **Static text**, **Images & GIFs** or **Audio FX**.
2. Configure the component, enter a preset name and select **Save as new**. Each component also has its own local OBS Browser Source URL.
3. In **Stream → Scene editor**, reload the preset list and add your preset. Position, resize, rotate and reorder the layer. **Fill scene** fits it to the canvas; new FX layers already cover the entire scene.
4. Save the scene. Its existing OBS Browser Source updates without replacing its URL. Set OBS dimensions to the scene's dimensions.

The editor can also add blank components. **Save as new** in the selected layer's properties creates a reusable preset from that layer's settings. Layers remain independent copies of presets. Choose layers from the layer list when a full-size FX layer covers the canvas.

## Text

Text supports multiple lines, three font families, font size, bold/italic, horizontal and vertical alignment, foreground/background colours, background opacity, padding and rounded corners. Text is rendered literally, never interpreted as HTML. A text field accepts up to 2,048 UTF-8 bytes.

## Images and animated GIFs

The **Images & GIFs** page contains the shared media library. Upload PNG, JPEG, WebP or GIF, then choose it for a component. Original bytes are retained, including GIF animation. Choose contain, crop/cover or stretch, and optionally round the corners.

- Each file: at most **8 MiB**, dimensions at most **4096 × 4096**.
- Shared library: at most **100 files / 32 MiB of Base64-encoded data** (approximately 24 MiB of original files).
- Identical file bytes reuse one asset. Names are labels, not filesystem paths.
- A saved scene or preset that references an asset prevents its deletion. Remove those references first. Standalone image URLs are not tracked as references.
- Operators and administrators share this library. The metadata persists in the private `DeckStatus.data/portal.json` store and each file in `DeckStatus.data/media/`, named by its content hash; back up the whole data directory with accounts, presets and scenes while DeckStatus is stopped.

Expand **Search Wikimedia Commons** to search public images or filter for GIFs. Searching is optional and needs internet access, but no API key. Only clicking Search contacts Commons; imported files are copied into the local library, so playback does not depend on the external service. Author/licence credits and a source-page link are retained in the library. Check the source's licence and add any required attribution to your stream, for example with a text component. Search results and external service availability can vary.

The browser calls the [MediaWiki search API](https://www.mediawiki.org/wiki/API:Search) and [Imageinfo API](https://www.mediawiki.org/wiki/API:Imageinfo) anonymously. No DeckStatus cookies or OBS keys are sent to Commons. Only the image-settings page permits the required Wikimedia connections; scene playback loads local assets only. GIPHY/Tenor integrations are not included.

## Audio reactions

Components use the **shared Windows audio input** configured under **Admin → Audio input**. An administrator saves an input or loopback device there and starts capture. The optional saved autostart setting starts that device when the application launches; it is off by default. Loading a component, preset or scene never starts capture. These are reactions to live captured audio, not Rekordbox beat-grid events or mixer FX controls.

Enable **Audio reaction** on a component's settings page or in a selected scene layer's properties. The scene editor supports reactions on deck, master and waveform layers too.

| Control | Meaning |
|---|---|
| Audio signal | RMS loudness, peak level, bass (20–250 Hz), mids (250–4,000 Hz) or highs (4,000–20,000 Hz) |
| Sensitivity | Multiplier applied before the trigger threshold |
| Trigger threshold | The level below which a reaction rests |
| Attack / release | Time constants for rising/falling motion; zero responds immediately |
| Scale amount | Added scale at full reaction; 0.25 means up to 1.25×, -0.5 means down to 0.5× |
| Horizontal / vertical movement | Offset in scene pixels; negative values move left/up |
| Rotation | Additional degrees around the layer centre |
| Opacity change | Added to the layer's base opacity, clamped to 0–1 |

All properties may react together. Geometry and static rotation provide the resting position. Scene boundaries clip anything that moves outside them. Stale or disconnected audio clears FX and lets motion return to rest according to release. Repeated sample buffers cannot hold an effect indefinitely. The analysis uses 1,024-sample windows, so frequency bands are approximate, especially at low frequencies.

Each scene has one shared reaction-analysis loop. Existing waveform visualizations still run their own rendering/sample loop. Scene updates retain existing image elements and legacy iframes when their source is unchanged, so moving a layer does not restart GIF animation or the track renderer.

## Audio FX

Choose **Smoke / fog**, **Flash**, or **Flash + fog**. Built-in styles provide soft fog, bass flashes and a combined neon look. Adjust both colours, intensity, cloud count, motion speed, flash decay and trigger cooldown.

Fog uses a transparent procedural canvas; signal level changes its density/opacity and cloud size. Flash triggers when the selected signal crosses the threshold from below, then fades. Sustained loud audio does not continuously retrigger it; a new crossing must also satisfy the cooldown. Start with low flash intensity when designing the effect.

New FX layers fill the scene. If you later change the canvas resolution, select the FX layer and use **Fill scene** again. Use layer order to place fog behind or over other components. Disable its audio reaction or hide the layer to turn it off.

## API and storage

Creative component types are `text`, `image` and `fx`, in addition to `deck`, `master` and `waveform`. The existing preset/scene APIs accept their validated options. Scene items also accept optional `rotation` in degrees. Image options refer to a 64-character content-hash `assetId`; arbitrary remote URLs and local paths are not accepted.

| Route | Access and response |
|---|---|
| `GET /components/{text,image,fx}` | Signed-in settings page |
| `GET /component/{text,image,fx}` | Session or matching standalone/scene read key |
| `GET /api/media` | Signed-in `{media:[...]}` metadata list; excludes encoded file data |
| `POST /api/media` | Signed-in upload/delete |
| `GET /api/media/{id}` | Session, standalone image key, or scene key referencing that visible image |

Upload: `{action:"upload",name,data:"BASE64",credit:"optional",source:"optional Commons page URL"}`. Delete: `{action:"delete",id}`. Upload returns metadata including `id`, `name`, `mime`, `width`, `height`, `bytes`, `credit` and `source`. The media endpoint accepts a JSON body up to 12 MiB; other API routes retain their 64 KiB limit. File signatures, dimensions, encoded size and library quota are checked server-side. SVG and arbitrary active content are not supported. Invalid data returns 400, in-use deletion 409, quota exhaustion 507.

Scene read keys expose only referenced visible images, not the library listing or unrelated media. A standalone image key permits fetching any known media ID, so keep standalone URLs private. Creative keys may read audio samples but never start/stop capture. Rotating standalone keys in Admin revokes their URLs; scene keys retain their separate rotation mechanism.

Existing stores gain an empty media collection and three additional standalone read keys without changing existing credentials, presets, scenes or OBS keys. No account setup is repeated.

## Validation

Run `node tests/browser_creative_test.cjs` for synthetic audio/rendering checks and `node tests/browser_creative_portal_test.cjs` for real-EXE upload, preset, scene, permission and restart checks. Both use isolated fixtures; neither opens an audio device. Native `portal_access` also checks media validation, references, migration and failed-write preservation. See [the validation log](validation.md) for recorded results and untested live scenarios.
