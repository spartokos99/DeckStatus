# Memory profile for Rekordbox 7.2.18.0 (Windows x64)

This profile documents the addresses and checks in `src/deckstatus_bridge.cpp` for the local `rekordbox.exe` examined on September 14, 2026. The bytes, PE fields and constructor assignments below were checked statically against the installed file. Separate live-test results are recorded in [validation.md](validation.md).

## Examined file identity

| Property | Value |
|---|---|
| Local file | `D:\Programs\rekordbox 7.2.18\rekordbox.exe` |
| File version | `7.2.18.0` |
| File size | `100561840` bytes |
| SHA-256 | `a99896cf26d5998e6ad4177796a467b83df14bf8ae7207df21ed01251e402493` |
| DOS/PE signature | `MZ` / `PE\0\0` |
| Machine / OptionalHeader.Magic | `0x8664` (AMD64) / `0x020B` (PE32+) |
| TimeDateStamp | `0x6A672BEA` |
| SizeOfImage | `0x06291000` |
| Preferred ImageBase | `0x0000000140000000` |

The DLL checks the file name, complete file version, PE structure, architecture, timestamp, image size and the 14 code regions below. SHA-256 is a documented comparison value; the DLL does not calculate it at runtime. A different build is reported as `unsupported`. Other Rekordbox 7 versions are not accepted based on similar offsets.

All code and data addresses in this document are RVAs, relative to the actual module base. Object offsets such as `+0x490` refer to their respective heap pointers. ASLR changes neither relative instruction displacements nor these object offsets.

## Read fields and static evidence

For `index = 0..3`, the DLL reads this chain:

```text
component = *(module_base + 0x05D1F260)
manager   = *(component + 0x490)
player    = *(manager + 0x50 + index * 8)
track_id  = uint32(player + 0x580)
device    = *(player + 0xCE8)
bpm_x100  = uint32(device + 0x9C)
master_device = *(player + 0x958)
is_master = uint32(master_device + 0x94) == 0
```

| Field | Evidence in the examined EXE |
|---|---|
| Global component pointer, RVA `0x05D1F260` | RVA `0x01729D41`: `mov [rip+0x045F5518], r15`; target address `0x05D1F260`. |
| Manager at component `+0x490` | RVA `0x0175578D` calls constructor `0x024E9720`, RVA `0x01755798` initializes it through `0x024E99F0`, and RVA `0x0175579D` stores the pointer with `mov [rsi+0x490], rbp`. |
| Four player pointers from manager `+0x50` | Function `0x024E99F0` calls player constructor `0x0244D6D0` with indices `0, 1, 2, 3`. RVA `0x024E9A3A` addresses `manager+0x50`; `0x024E9A3E` stores the first player. Further assignments: `0x024E9A71` (`+0x58`), `0x024E9AA8` (`+0x60`), `0x024E9ADF` (`+0x68`). |
| Player vtable, RVA `0x03BB7E70` | RVA `0x0244D85E` loads this table; `0x0244D865` writes it to the beginning of the object. |
| Player index at `+0x478` | Constructor `0x0244D6D0` takes the index from `r8d` into `esi` at `0x0244D6F1`; `0x0244D923` writes it as a 32-bit value to `[r14+0x478]`. |
| Track ID at player `+0x580` | RVA `0x0249EB4E` reads the ID from `[rdx+8]`; `0x0249EB51` writes it as a 32-bit value to `[rdi+0x580]`. The host resolves metadata using this ID. |
| BPM device at player `+0xCE8` | RVA `0x02473EC8` loads the name from RVA `0x03B8BF2C`, whose bytes are `40 42 50 4D 00` (`@BPM\0`). RVA `0x02473EE7` calls constructor `0x022AB510`; `0x02473EF8` stores its return value at `[rdi+0xCE8]`. |
| BPM device vtable, RVA `0x03B85620` | RVA `0x022AB54D` loads this table; `0x022AB554` writes it to the beginning of the object. |
| Device name at `+0x10` | Base constructor `0x022C79C0` initializes the supplied string at `this+0x10` (RVA `0x022C79E9` and call at `0x022C79F0`). At runtime, the DLL checks that the pointer there refers to exactly five bytes, `@BPM\0`. |
| Cached BPM value at device `+0x9C` | Constructor RVA `0x022AB571` initializes the 32-bit value to zero. Update function `0x022ABF20` writes it at `0x022ABF46` using `mov [rcx+0x9C], r9d`, before passing it to a downstream UI object. |

The `bpm_x100` unit is also supported by the scaling at RVA `0x02440653`: `vmulss xmm0, xmm0, [rip+0x03142DE5]` multiplies the previously queried float by the constant at RVA `0x05583440`. Its bytes are `00 00 C8 42`, or IEEE-754 `float32(100.0)`. The host exposes the integer divided by 100 as BPM. Static scaling evidence does not replace a live comparison of a loaded track and a tempo change; actual test coverage is recorded in `validation.md`.

## Master status

At RVA `0x02470049`, the player constructor loads the name `Master\0` from `0x03B8BDEC`, calls device constructor `0x022AB510` at `0x02470068`, and stores the result at player `+0x958` at `0x02470079`. This is the same device type as the BPM field, with vtable RVA `0x03B85620` and a string pointer at `+0x10`.

The UI update calls function `0x02440A20` at `0x0247B509`, compares its master index with the respective deck index at `0x0247B54E`, and calls the Boolean setter through vtable `+0x48` when needed (`0x0247B576`). This setter is at `0x022ABD20`: `r9b` is extended to an integer, XORed with 1 at `0x022ABD3F`, and written to device `+0x94` at `0x022ABD45`. **0 means master; 1 means not master.** This Boolean field is separate from the numeric BPM cache at `+0x9C`.

The DLL validates names, vtables, pointers and value ranges for all four master devices, then rereads their values for consistency. Exactly one active master yields a deck number; no master, multiple masters, uninitialized or unstable values yield `master_deck = 0`, or API `null`. The current IPC protocol is version 3 and also carries track position and duration. Detection follows the UI master marker and makes no claim about audible playback.

## Track position and total duration

Player `+0xCD8` contains the `@CurrentTime\0` device; player `+0xCE0` contains `@TotalTime\0`. Both use the same device constructor `0x022AB510`, vtable `0x03B85620` and numeric cache `+0x9C` as BPM. Names and pointers are checked on every sample.

Name references are at RVA `0x02473DDE` (string RVA `0x03B8BF00`) and `0x02473E53` (string RVA `0x03B8BF10`); constructor calls are at `0x02473DFD` and `0x02473E72`. Pointers are stored at `0x02473E0E` and `0x02473E83`, respectively.

The update function queries total duration through `0x0243E000` at `0x02478476` and sets the TotalTime device through vtable `+0x60` at `0x02478496`. Current position is obtained through `0x0243E4D0` at `0x024784BB` and set at `0x024784DE`. These values are in milliseconds. For negative positions, code starting at `0x024784D1` negates the magnitude and sets bit 31. The DLL decodes this **sign-magnitude format**, not two's complement.

The maximum accepted duration and position magnitude are 86,400,000 ms (24 hours); zero duration and invalid reads yield unknown time data. The track ID is compared again after reading. Original BPM, live BPM and time data are preserved independently when library metadata is added. The display does not interpolate across missing samples or infer play/pause state from them.

## Fourteen exact code checks

These bytes were taken directly from `src/deckstatus_bridge.cpp`, mapped to file offsets through the PE section table, and compared with the installed EXE. The first nine comparisons and the five additional timeline checks passed.

| RVA | Expected and observed bytes | Purpose |
|---|---|---|
| `0x01729D41` | `4C 89 3D 18 55 5F 04` | Global component pointer |
| `0x0175579D` | `48 89 AE 90 04 00 00` | Manager offset |
| `0x0249EB4E` | `8B 42 08 89 87 80 05 00 00` | Track ID assignment |
| `0x02473EC8` | `48 8D 15 5D 80 71 01` | BPM device name reference |
| `0x02473EF8` | `48 89 87 E8 0C 00 00` | BPM device offset |
| `0x022ABF46` | `44 89 89 9C 00 00 00` | Cached 32-bit value |
| `0x02470049` | `48 8D 15 9C BD 71 01` | Master device name reference |
| `0x02470079` | `48 89 87 58 09 00 00` | Master device offset |
| `0x022ABD3F` | `83 F0 01 48 8B D9 89 81 94 00 00 00` | Boolean inversion and cache at `+0x94` |
| `0x02473DDE` | `48 8D 15 1B 81 71 01` | CurrentTime name |
| `0x02473E0E` | `48 89 87 D8 0C 00 00` | CurrentTime device |
| `0x02473E53` | `48 8D 15 B6 80 71 01` | TotalTime name |
| `0x02473E83` | `48 89 87 E0 0C 00 00` | TotalTime device |
| `0x024784D1` | `85 C0 79 06 F7 D8 0F BA E8 1F` | Sign-magnitude encoding of negative positions |

The remaining fields in the evidence table are additional static evidence, not further code checks. Player vtable, deck index, device vtable and device name are instead checked on the actual object on every sample.

## Read behavior

The DLL does not call any of the examined native functions. Memory reads use `ReadProcessMemory` within its own process. Unreadable pointers or mismatching type, index or name checks invalidate the affected deck data. The BPM plausibility limit is `100000` in scaled integer format, equivalent to 1000 BPM.

Player pointers, track IDs and device pointers are checked again around the reads. If the component or manager changes, the entire sample is discarded. These checks reduce inconsistent measurements during track changes; they are not an atomic snapshot and do not pause Rekordbox. Track ID zero is exposed as an empty deck with null BPM at the API level. Status remains `starting` until a valid player is found; at least one valid player yields `connected`. Therefore, `connected` does not mean a track is loaded or audibly playing.

The separate host adds title, artist, album, stored key and artwork from the local database. This memory profile does not determine play/pause state, fader position or a live transposed key.

## Reproducing the static checks

The checks used Node.js `24.13.0` for read-only PE/byte comparisons and SHA-256, and Microsoft `dumpbin` `14.51.36256.0` for headers and targeted disassembly. The file was neither loaded nor modified. For each section, byte comparisons use `file_offset = PointerToRawData + RVA - VirtualAddress`; memory addresses are not treated as file offsets.

Example header and scaling checks in a Visual Studio Developer PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'D:\Programs\rekordbox 7.2.18\rekordbox.exe'
dumpbin /headers 'D:\Programs\rekordbox 7.2.18\rekordbox.exe'
dumpbin /disasm /range:0x142440630,0x142440660 'D:\Programs\rekordbox 7.2.18\rekordbox.exe'
```

The dumpbin addresses in this example include the preferred ImageBase `0x140000000`. This static documentation and the runtime results in `validation.md` must be updated separately for a new Rekordbox build; matching version numbers alone are insufficient.
