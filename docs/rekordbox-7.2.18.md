# Speicherprofil für Rekordbox 7.2.18.0 (Windows x64)

Dieses Profil beschreibt die Adressen und Prüfungen in `src/deckstatus_bridge.cpp` für die am 14.09.2026 untersuchte lokale `rekordbox.exe`. Die unten genannten Bytes, PE-Felder und Konstruktorzuweisungen wurden statisch an der installierten Datei geprüft. Die Ergebnisse des separaten Live-Tests stehen in [validation.md](validation.md).

## Identität der untersuchten Datei

| Merkmal | Wert |
|---|---|
| Lokale Datei | `D:\Programs\rekordbox 7.2.18\rekordbox.exe` |
| Dateiversion | `7.2.18.0` |
| Dateigröße | `100561840` Bytes |
| SHA-256 | `a99896cf26d5998e6ad4177796a467b83df14bf8ae7207df21ed01251e402493` |
| DOS-/PE-Signatur | `MZ` / `PE\0\0` |
| Machine / OptionalHeader.Magic | `0x8664` (AMD64) / `0x020B` (PE32+) |
| TimeDateStamp | `0x6A672BEA` |
| SizeOfImage | `0x06291000` |
| Bevorzugte ImageBase | `0x0000000140000000` |

Die DLL prüft Dateiname, vollständige Dateiversion, PE-Struktur, Architektur, Timestamp, Imagegröße und die 14 Codeabschnitte unten. Der SHA-256 ist ein dokumentierter Vergleichswert; die DLL berechnet ihn zur Laufzeit nicht. Ein abweichender Build wird als `unsupported` gemeldet. Eine andere Rekordbox-7-Version wird nicht anhand ähnlicher Offsets akzeptiert.

Alle Code- und Datenadressen in diesem Dokument sind RVAs, also relativ zur tatsächlichen Modulbasis. Objekt-Offsets wie `+0x490` beziehen sich dagegen auf den jeweiligen Heap-Zeiger. ASLR verändert weder die relativen Instruktionsdisplacements noch diese Objekt-Offsets.

## Ausgelesene Felder und statische Belege

Für `index = 0..3` liest die DLL folgende Kette:

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

| Feld | Beleg in der untersuchten EXE |
|---|---|
| Globaler Component-Zeiger, RVA `0x05D1F260` | RVA `0x01729D41`: `mov [rip+0x045F5518], r15`; Zieladresse ist `0x05D1F260`. |
| Manager bei Component `+0x490` | RVA `0x0175578D` ruft Konstruktor `0x024E9720`, RVA `0x01755798` initialisiert über `0x024E99F0`, RVA `0x0175579D` speichert den Zeiger mit `mov [rsi+0x490], rbp`. |
| Vier Player-Zeiger ab Manager `+0x50` | Funktion `0x024E99F0` ruft Player-Konstruktor `0x0244D6D0` mit Indizes `0, 1, 2, 3`. RVA `0x024E9A3A` adressiert `manager+0x50`, `0x024E9A3E` speichert den ersten Player. Weitere Zuweisungen: `0x024E9A71` (`+0x58`), `0x024E9AA8` (`+0x60`), `0x024E9ADF` (`+0x68`). |
| Player-vtable, RVA `0x03BB7E70` | RVA `0x0244D85E` lädt diese Tabelle; `0x0244D865` schreibt sie an den Objektanfang. |
| Player-Index bei `+0x478` | Konstruktor `0x0244D6D0` übernimmt bei `0x0244D6F1` den Index aus `r8d` nach `esi`; `0x0244D923` schreibt ihn als 32-Bit-Wert an `[r14+0x478]`. |
| Track-ID bei Player `+0x580` | RVA `0x0249EB4E` liest die ID aus `[rdx+8]`; `0x0249EB51` schreibt sie als 32-Bit-Wert nach `[rdi+0x580]`. Die Metadaten werden anhand dieser ID vom Host aufgelöst. |
| BPM-Device bei Player `+0xCE8` | RVA `0x02473EC8` lädt den Namen von RVA `0x03B8BF2C`, dessen Bytes `40 42 50 4D 00` (`@BPM\0`) sind. RVA `0x02473EE7` ruft Konstruktor `0x022AB510`; `0x02473EF8` speichert dessen Rückgabewert an `[rdi+0xCE8]`. |
| BPM-Device-vtable, RVA `0x03B85620` | RVA `0x022AB54D` lädt diese Tabelle; `0x022AB554` schreibt sie an den Objektanfang. |
| Device-Name bei `+0x10` | Basiskonstruktor `0x022C79C0` initialisiert den übergebenen String an `this+0x10` (RVA `0x022C79E9` und Aufruf bei `0x022C79F0`). Zur Laufzeit prüft die DLL den dortigen Zeiger auf exakt fünf Bytes `@BPM\0`. |
| Zwischengespeicherter BPM-Wert bei Device `+0x9C` | Konstruktor-RVA `0x022AB571` initialisiert den 32-Bit-Wert auf null. Update-Funktion `0x022ABF20` schreibt den Wert bei `0x022ABF46` mit `mov [rcx+0x9C], r9d`, bevor sie ihn an ein nachgelagertes UI-Objekt weiterreicht. |

Die Einheit `bpm_x100` wird außerdem durch die Skalierung bei RVA `0x02440653` gestützt: `vmulss xmm0, xmm0, [rip+0x03142DE5]` multipliziert den vorher abgefragten Float mit der Konstanten bei RVA `0x05583440`. Deren Bytes sind `00 00 C8 42`, also IEEE-754 `float32(100.0)`. Der Host stellt den Integer als BPM geteilt durch 100 bereit. Der statische Skalierungsbeleg ersetzt keinen Live-Vergleich eines geladenen Tracks und einer Tempoänderung; dessen tatsächlicher Testumfang ist in `validation.md` festgehalten.

## Master-Status

Der Player-Konstruktor lädt bei RVA `0x02470049` den Namen `Master\0` von `0x03B8BDEC`, ruft bei `0x02470068` den Device-Konstruktor `0x022AB510` und speichert das Ergebnis bei `0x02470079` an Player `+0x958`. Es ist derselbe Device-Typ wie beim BPM-Feld, mit vtable-RVA `0x03B85620` und String-Zeiger bei `+0x10`.

Die UI-Aktualisierung ruft bei `0x0247B509` die Funktion `0x02440A20`, vergleicht deren Master-Index bei `0x0247B54E` mit dem jeweiligen Deckindex und ruft bei Bedarf den booleschen Setter über vtable `+0x48` auf (`0x0247B576`). Dieser Setter liegt bei `0x022ABD20`: `r9b` wird zum Integer erweitert, bei `0x022ABD3F` mit 1 XOR-verknüpft und bei `0x022ABD45` nach Device `+0x94` geschrieben. **0 bedeutet Master, 1 bedeutet kein Master.** Das boolesche Feld ist nicht der numerische BPM-Cache bei `+0x9C`.

Die DLL validiert Namen, vtable, Zeiger und Wertebereich für alle vier Master-Devices und liest ihre Werte zum Gegenprüfen erneut. Genau ein gesetzter Master ergibt eine Decknummer; kein Master, mehrere Master, uninitialisierte oder instabile Werte ergeben `master_deck = 0` beziehungsweise API-`null`. Das aktuelle IPC-Protokoll ist Version 3 und überträgt zusätzlich Trackposition und Dauer. Die Erkennung folgt der UI-Master-Markierung und trifft keine Aussage über hörbare Wiedergabe.

## Trackposition und Gesamtlänge

Player `+0xCD8` enthält das Device `@CurrentTime\0`, Player `+0xCE0` das Device `@TotalTime\0`. Beide verwenden denselben Device-Konstruktor `0x022AB510`, dieselbe vtable `0x03B85620` und den numerischen Cache `+0x9C` wie BPM. Namen und Zeiger werden bei jedem Sample geprüft.

Die Namensreferenzen stehen bei RVA `0x02473DDE` (String-RVA `0x03B8BF00`) und `0x02473E53` (String-RVA `0x03B8BF10`), die Konstruktoraufrufe bei `0x02473DFD` und `0x02473E72`. Die Zeiger werden bei `0x02473E0E` beziehungsweise `0x02473E83` gespeichert.

Die Update-Funktion fragt bei `0x02478476` über `0x0243E000` die Gesamtlänge ab und setzt das TotalTime-Device bei `0x02478496` über vtable `+0x60`. Die aktuelle Position wird bei `0x024784BB` über `0x0243E4D0` ermittelt und bei `0x024784DE` gesetzt. Diese Werte liegen in Millisekunden vor. Für negative Positionen negiert der Code ab `0x024784D1` den Betrag und setzt Bit 31. Die DLL dekodiert dieses **Vorzeichen-/Betragsformat**, nicht Zweierkomplement.

Die maximale akzeptierte Dauer und der maximale Positionsbetrag betragen 86.400.000 ms (24 Stunden); Dauer null und ungültige Lesezugriffe ergeben unbekannte Zeitdaten. Die Track-ID wird nach dem Lesen erneut verglichen. Original- und Live-BPM sowie diese Zeitdaten bleiben beim Ergänzen der Bibliotheksmetadaten unabhängig erhalten. Die Anzeige interpoliert nicht über fehlende Samples hinweg und leitet keinen Play/Pause-Zustand daraus ab.

## Vierzehn exakte Codeprüfungen

Die folgenden Bytes wurden direkt aus `src/deckstatus_bridge.cpp` übernommen, per PE-Section-Tabelle auf Dateioffsets abgebildet und mit der installierten EXE verglichen. Die ersten neun Vergleiche und die fünf zusätzlichen Timeline-Prüfungen waren erfolgreich.

| RVA | Erwartete und vorgefundene Bytes | Zweck |
|---|---|---|
| `0x01729D41` | `4C 89 3D 18 55 5F 04` | Globaler Component-Zeiger |
| `0x0175579D` | `48 89 AE 90 04 00 00` | Manager-Offset |
| `0x0249EB4E` | `8B 42 08 89 87 80 05 00 00` | Track-ID-Zuweisung |
| `0x02473EC8` | `48 8D 15 5D 80 71 01` | Referenz auf BPM-Device-Namen |
| `0x02473EF8` | `48 89 87 E8 0C 00 00` | BPM-Device-Offset |
| `0x022ABF46` | `44 89 89 9C 00 00 00` | Zwischengespeicherter 32-Bit-Wert |
| `0x02470049` | `48 8D 15 9C BD 71 01` | Referenz auf Master-Device-Namen |
| `0x02470079` | `48 89 87 58 09 00 00` | Master-Device-Offset |
| `0x022ABD3F` | `83 F0 01 48 8B D9 89 81 94 00 00 00` | Invertierung des booleschen Werts und Cache bei `+0x94` |
| `0x02473DDE` | `48 8D 15 1B 81 71 01` | CurrentTime-Name |
| `0x02473E0E` | `48 89 87 D8 0C 00 00` | CurrentTime-Device |
| `0x02473E53` | `48 8D 15 B6 80 71 01` | TotalTime-Name |
| `0x02473E83` | `48 89 87 E0 0C 00 00` | TotalTime-Device |
| `0x024784D1` | `85 C0 79 06 F7 D8 0F BA E8 1F` | Vorzeichen-/Betragskodierung negativer Positionen |

Die übrigen Felder der Tabelle sind zusätzliche statische Belege; sie sind keine weiteren Codeprüfungen. Player-vtable, Deckindex, Device-vtable und Device-Name werden stattdessen bei jedem Sample am konkreten Objekt geprüft.

## Verhalten beim Lesen

Die DLL ruft keine der untersuchten nativen Funktionen auf. Speicherzugriffe erfolgen mit `ReadProcessMemory` im eigenen Prozess. Unlesbare Zeiger und nicht passende Typ-, Index- oder Namensprüfungen verwerfen die betroffenen Deckdaten. Die BPM-Plausibilitätsgrenze beträgt `100000` im skalierten Integerformat, entsprechend 1000 BPM.

Player-Zeiger, Track-ID und Device-Zeiger werden um das Lesen herum erneut geprüft. Ändern sich Component oder Manager, verwirft die DLL das gesamte Sample. Diese Gegenprüfungen reduzieren inkonsistente Messungen bei Trackwechseln; sie sind kein atomarer Snapshot und halten Rekordbox nicht an. Track-ID null wird als leeres Deck ausgegeben, mit BPM null auf API-Ebene. Solange noch kein gültiger Player vorliegt, bleibt der Status `starting`; mindestens ein gültiger Player ergibt `connected`. `connected` bedeutet daher nicht, dass ein Track geladen ist oder hörbar spielt.

Titel, Artist, Album, gespeicherter Key und Cover werden vom separaten Host über die lokale Datenbank ergänzt. Dieses Speicherprofil ermittelt weder Play/Pause noch Faderstellung oder live transponierte Tonart.

## Nachvollziehbarkeit der statischen Prüfung

Verwendet wurden Node.js `24.13.0` zum lesenden PE-/Bytevergleich und SHA-256 sowie Microsoft `dumpbin` `14.51.36256.0` für Header und gezielte Disassemblierung. Die Datei wurde dabei weder geladen noch verändert. Der Bytevergleich verwendet pro Section `file_offset = PointerToRawData + RVA - VirtualAddress`; Speicheradressen werden nicht mit Dateioffsets verwechselt.

Beispiel für die Header- und Skalierungsprüfung in einer Visual-Studio-Developer-PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'D:\Programs\rekordbox 7.2.18\rekordbox.exe'
dumpbin /headers 'D:\Programs\rekordbox 7.2.18\rekordbox.exe'
dumpbin /disasm /range:0x142440630,0x142440660 'D:\Programs\rekordbox 7.2.18\rekordbox.exe'
```

Die Dumpbin-Adressen im Beispiel enthalten die bevorzugte ImageBase `0x140000000`. Diese statische Dokumentation und die Laufzeitergebnisse in `validation.md` müssen bei einem neuen Rekordbox-Build getrennt aktualisiert werden; passende Versionsziffern allein genügen nicht.
