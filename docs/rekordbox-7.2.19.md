# Memory profile for Rekordbox 7.2.19.0 (Windows x64)

Port of the 7.2.18.0 profile ([rekordbox-7.2.18.md](rekordbox-7.2.18.md)). Same classes and device component, with player and component fields shifted. The file was analyzed read-only, never executed or modified. A bridge built from this profile was live-tested by the contributor on one installation; see [validation.md](validation.md).

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

`tests/rekordbox_profile_test.exe --verify` accepts this file: PE profile, full-file SHA-256 and all 14 mapped code guards.

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
Master       device = *(player + 0x938)               7.2.18: +0x958
bpm_x100  = uint32(device + 0x9C)                  unchanged
is_master = uint32(master_device + 0x94) == 0      unchanged
player vtable RVA 0x03DDB150, device vtable RVA 0x03DA4C18, device name pointer at +0x10 (unchanged)
```

## Evidence

| Field | Evidence in the examined EXE |
|---|---|
| Global component, RVA `0x05FE0EA0` | Constructor `0x01815BD0` initializes `this+0x4C0` (`0x01815F12`) and publishes `this` at `0x01816554`: `mov [rip+0x047CA945], r15`. Its caller `0x0184B768` stores the same object at `[rsi+0x100]`, the path also used by `0x01A480E0` (`[rcx+0x238] → [+0x100]`) before calling `0x01841E70`. |
| Manager at component `+0x4C0` | `0x01841E70` calls manager init `0x025E70B0` at `0x01842098` and stores the manager at `0x0184209D`: `mov [rsi+0x4C0], rbp`. |
| Players at manager `+0x50…+0x68` | `0x025E70B0` calls player constructor `0x0254AF80` with `r8d` = 0, 1, 2, 3 and stores the results at `[rbp+0x50]` (`r14`), `+0x58` (`0x025E7131`), `+0x60` (`0x025E7168`), `+0x68` (`0x025E719F`); then calls `0x0254DDB0` on each. |
| Player vtable, index | Constructor `0x0254AF80`: `0x0254B10E` loads RVA `0x03DDB150`, `0x0254B115` writes it to `[r14]`; `0x0254B1D3` writes the index from `r8d` to `[r14+0x458]`. `0x0254DDE3` reads it back. |
| Track ID at `+0x560` | `0x0259C43E`: `mov eax,[rdx+8]; mov [rdi+0x560], eax` (unique match of the 7.2.18 pattern with a new displacement). |
| Devices | `0x0256BE30` builds the named devices through factory `0x023A8B90` (`r9b=1`, `r8`=name). `@CurrentTime` (string `0x03DAC3D8`, ref `0x025716DE`) → store `0x0257170E` `[rdi+0xCB8]`; `@TotalTime` (`0x03DAC3E8`, `0x02571753`) → `0x02571783` `[rdi+0xCC0]`; `@BPM` (`0x03DAC404`, `0x025717C8`) → `0x025717F8` `[rdi+0xCC8]`; `Master` (`0x03DAC2C4`, `0x0256D949`) → `0x0256D979` `[rdi+0x938]`. |
| Device class | Factory `0x023A8B90` calls base constructor `0x023C5220` (name copied to `this+0x10`), then writes vtable `0x03DA4C18` at `0x023A8BD4` and zeroes `+0x9C`. Vtable entry `+0x48` = `0x023A93A0` (writes `!value` to `+0x94` at `0x023A93C5`, after `xor eax,1` at `0x023A93BF`); entry `+0x60` = `0x023A95A0` (writes the 32-bit value to `+0x9C` at `0x023A95C6`). Same roles as 7.2.18 (`+0x48` Boolean setter, `+0x60` value setter). |
| BPM × 100 | Tempo getters `0x0253DC93` / `0x0253DD03` multiply the queried float by `float32(100.0)`, as at 7.2.18 RVA `0x02440653`. |
| Sign-magnitude time | `0x02575DD1`: `85 C0 79 06 F7 D8 0F BA E8 1F`, unchanged sequence. |

## Code guards

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
| `0x023A93BF` | `83 F0 01 48 8B D9 89 81 94 00 00 00` | Boolean inversion and cache at `+0x94` |
| `0x025716DE` | `48 8D 15 F3 AC 83 01` | CurrentTime name |
| `0x0257170E` | `48 89 87 B8 0C 00 00` | CurrentTime device |
| `0x02571753` | `48 8D 15 8E AC 83 01` | TotalTime name |
| `0x02571783` | `48 89 87 C0 0C 00 00` | TotalTime device |
| `0x02575DD1` | `85 C0 79 06 F7 D8 0F BA E8 1F` | Sign-magnitude encoding of negative positions |

## Live validation checklist

Load a track on each deck, compare BPM with Rekordbox while moving the tempo fader, switch Master, play and compare remaining time, eject a track. Results are recorded in [validation.md](validation.md).
