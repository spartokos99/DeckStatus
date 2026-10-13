# Memory profile for Rekordbox 7.2.19.0 (Windows x64)

Separate profile for the original Rekordbox 7.2.19.0 executable. Its exact file identity and code guards were examined read-only. The owner subsequently reported that this original Windows x64 installation worked successfully in a live test; the static profile evidence below documents the exact supported build.

## Live compatibility status

The project owner reports successful live testing with the original Rekordbox 7.2.19.0 Windows x64 installation. This is an owner-reported test, not a test performed by the documentation author. The profile remains limited to the exact executable identity below; other versions and patched variants are unsupported.

## Examined file identity

| Property | Value |
|---|---|
| File / product version | `7.2.19.0` / `7.2.19.0342` |
| File size | `103540656` bytes |
| SHA-256 | `c9ed23e974c51c75e2ec069fbd2679a3dbe606be9e79e9cbbd13d6418ab11c37` |
| Machine / Magic | `0x8664` / `0x020B` |
| TimeDateStamp | `0x6A97B2E4` |
| SizeOfImage | `0x06573000` |
| Entry point RVA | `0x0280E96C` |
| Number of sections | `8` (original, unpatched) |

The verifier checks this exact identity, full-file hash and all 14 loaded-image code guards. Version alone is never sufficient. The separate 7.2.18.0 original and patched profiles retain their own exact identities and share only the 7.2.18 layout.

## Read chain

```text
component = *(module_base + 0x05FE0EA0)            7.2.18: 0x05D1F260
manager   = *(component + 0x4C0)                   7.2.18: +0x490
player    = *(manager + 0x50 + index * 8)          unchanged
index     = uint32(player + 0x458)                 7.2.18: +0x478
track_id  = uint32(player + 0x560)                 7.2.18: +0x580
@CurrentTime device = *(player + 0xCB8)            7.2.18: +0xCD8
@TotalTime   device = *(player + 0xCC0)            7.2.18: +0xCE0
@BPM         device = *(player + 0xCC8)            7.2.18: +0xCE8
Master       device = *(player + 0x938)            7.2.18: +0x958
bpm_x100  = uint32(device + 0x9C)                  unchanged
is_master = uint32(master_device + 0x94) == 0      unchanged
player vtable RVA 0x03DDB150, device vtable RVA 0x03DA4C18
device name pointer at +0x10                      unchanged
```

## Static layout evidence

| Field | Evidence in the examined executable |
|---|---|
| Component global, RVA `0x05FE0EA0` | Constructor `0x01815BD0` initializes `this+0x4C0` (`0x01815F12`) and publishes `this` at `0x01816554`: `mov [rip+0x047CA945], r15`. The RIP-relative target is `0x05FE0EA0`. |
| Manager at component `+0x4C0` | `0x01841E70` calls manager initialization `0x025E70B0` at `0x01842098` and stores the manager at `0x0184209D`: `mov [rsi+0x4C0], rbp`. |
| Players at manager `+0x50…+0x68` | `0x025E70B0` constructs players with indices 0–3 and stores them at `[rbp+0x50]`, `+0x58`, `+0x60` and `+0x68`, then initializes each. |
| Player vtable and index | Player constructor `0x0254AF80` writes vtable RVA `0x03DDB150` and stores the constructor index at `[player+0x458]`. |
| Track ID at `+0x560` | `0x0259C43E` reads the ID and writes it to `[rdi+0x560]`. |
| Timeline, BPM and Master devices | Device creation references the named `@CurrentTime`, `@TotalTime`, `@BPM` and `Master` components and stores them at offsets shown above. |
| Device component | Device factory writes vtable RVA `0x03DA4C18`; its value setter caches a 32-bit value at `+0x9C`, while the Master setter stores inverted state at `+0x94`. |
| BPM scale and negative time | The tempo getter scales by `100.0`. RVA `0x02575DD1` contains the sign-magnitude position conversion also present in 7.2.18. |

The bridge additionally checks each player's vtable, index and track ID, and each device's vtable/name while sampling. It discards the sample if the root component or manager changes.

## Exact code guards

| RVA | Bytes | Purpose |
|---|---|---|
| `0x01816554` | `4C 89 3D 45 A9 7C 04` | Global component pointer |
| `0x0184209D` | `48 89 AE C0 04 00 00` | Manager offset |
| `0x0259C43E` | `8B 42 08 89 87 60 05 00 00` | Track ID assignment |
| `0x025717C8` | `48 8D 15 35 AC 83 01` | BPM device name reference |
| `0x025717F8` | `48 89 87 C8 0C 00 00` | BPM device offset |
| `0x023A95C6` | `44 89 89 9C 00 00 00` | Cached 32-bit value |
| `0x0256D949` | `48 8D 15 74 E9 83 01` | Master device name reference |
| `0x0256D979` | `48 89 87 38 09 00 00` | Master device offset |
| `0x023A93BF` | `83 F0 01 48 8B D9 89 81 94 00 00 00` | Inverted Master state at `+0x94` |
| `0x025716DE` | `48 8D 15 F3 AC 83 01` | CurrentTime name |
| `0x0257170E` | `48 89 87 B8 0C 00 00` | CurrentTime device |
| `0x02571753` | `48 8D 15 8E AC 83 01` | TotalTime name |
| `0x02571783` | `48 89 87 C0 0C 00 00` | TotalTime device |
| `0x02575DD1` | `85 C0 79 06 F7 D8 0F BA E8 1F` | Sign-magnitude position |

The code and data addresses above are RVAs relative to the loaded module base. Object offsets refer to their respective heap objects. See the [7.2.18 profile](rekordbox-7.2.18.md) for its separate layout and identities, and the [validation log](validation.md) for the checks run on this checkout.
