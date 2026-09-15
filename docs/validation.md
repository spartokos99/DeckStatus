# Validierung am 14. September 2026

## Build und automatisierte Tests

Windows x64, MSVC 19.51 / Visual Studio 2026, C++20, Release-Build mit statischer C++-Laufzeit. Alle fünf CTest-Tests bestanden:

- `artwork_database`: Metadaten-Joins, fehlende Einträge und Verknüpfungen, SQL-NULL, UTF-8-Grenzen, Coverpfade, Pfadgrenzen und unveränderte Testdatenbank. Verwendet die installierte sqlite3.dll und eine eigens erzeugte temporäre Datenbank.
- `http_server`: JSON, Unicode, Seiten, Methoden, Host/Origin, Coverzuordnung bei Trackwechseln, fehlerhafte/mehrfache Track-IDs, fehlende Cover, Health-Zustände, belegter Port und sauberes Beenden.
- `scanner_boundaries`: ungültige Speicherbereiche, Seitengrenzen, UTF-8, PE-Grenzen und Signaturmehrdeutigkeit.
- `injection_lifecycle`: Laden der DLL in einen eigenen Testprozess, IPC, Ablehnung eines unpassenden EXE-Fingerabdrucks, doppelte Anbindung und erneute Anbindung.
- `master_history`: Master-Wechsel, derselbe Track auf einem anderen Deck, unbekannte/unterbrochene Master-Zustände, erneut auftauchende Tracks, verspätete Metadaten, eingefrorene BPM, History-Grenze 50, Coverzugriff und Verdrängung während eines Coverabrufs, veralteter Sampler.

Dashboard und bisheriges Deck-Overlay zusätzlich mit JavaScript-Syntaxprüfung und einem DOM-Test geprüft: sichere Textausgabe, fehlende Werte, Deckwahl, Demo-Kennzeichnung, Verbindungsverlust und Wiederholung fehlgeschlagener Coverabrufe.

Das neue Master-Overlay wurde mit `tests/browser_master_test.cjs` in einem echten Edge-Browser im Headless-Modus gerendert, mit eigenem Profil und synthetischem HTTP-Testserver. Geprüft: Vorschau, gespeicherte Einstellungen, erzeugte OBS-URL, History-Anzahl, Album-Schalter und ein Overlay nur mit Titel (alle fünf übrigen Felder verborgen), tatsächliche Web-Animations-Übergänge, Wiederverwendung der Karten beim Wechsel, schnelle Wechsel vor Animationsende, BPM-Updates ohne Animationsneustart, sichere Textausgabe, fehlende Cover, Verbindungsunterbrechung und reduzierte Bewegung. Screenshots `build/test-artifacts/master-settings.png` und `master-overlay.png` wurden visuell geprüft. Kein eigener OBS-Test wird behauptet.

## Live mit Rekordbox 7.2.18.0

Erweiterte Browserprüfung für History-Skalierung, Ausrichtung und BPM-Anzeige: tatsächliche Karten- und Coverabmessungen bei 0,65× sowie Grenzwerte 0,20×/1,00× geprüft, ebenso die Abstände in der Liste. Links-, Mittel- und Rechtsausrichtung wurden sowohl gegenüber dem Master als auch gegenüber dem Browserfenster gemessen. Ein laufender Übergang wurde zwischen voller Master-Größe und 0,40× History-Größe kontrolliert; schnelle Trackwechsel zusätzlich bei 0,20× und rechter Ausrichtung. Neue Einstellungen und URL-Parameter überstehen das Neuladen. Aktuelle und Original-BPM wurden in beiden Overlays mit unterschiedlichen Werten, laufender Tempoänderung und fehlendem Originaltempo geprüft. Screenshots `master-settings-scaled.png`, `master-overlay-scaled-right.png` und `deck-overlay-bpm-mobile.png` wurden visuell geprüft. Für diese Anpassungen war keine Änderung am nativen Datenzugriff erforderlich.

Die DLL wurde in die bereits laufende Installation unter `D:\Programs\rekordbox 7.2.18` geladen. Die HTTP-API meldete `connected`, `demo: false`, Version `7.2.18.0` und frische Messungen (unter 200 ms im beobachteten Abruf). Die Bibliothek ließ sich nur lesend öffnen.

Zunächst wurden leere Decks ohne veraltete Metadaten ausgegeben. Nach dem Laden von vier Tracks lieferten alle vier Decks gültige Track-IDs, Titel, Artist, Album, Key und BPM; `metadataAvailable` war jeweils `true`.

| Deck | BPM beim Abruf | Key | Cover |
|---|---:|---|---|
| 1 | 174 | 9A | JPEG, HTTP 200 |
| 2 | 174 | 7B | JPEG, HTTP 200 |
| 3 | 175 | 9A | JPEG, HTTP 200 |
| 4 | 174 | 7A | JPEG, HTTP 200 |

`/api/health` und `/overlay?deck=1` lieferten HTTP 200. Der Wechsel von leeren zu geladenen Decks wurde ohne Neustart der Bridge erkannt. Nach dem Beenden der Bridge war `rb_bridge.dll` nicht mehr in Rekordbox geladen; Rekordbox lief weiter.

Die Bibliothekswerte und Coverantworten wurden direkt über die API geprüft. Nicht gesondert live geprüft: manuelle Pitchbewegungen, Tracks aus Streamingdiensten, Export-Modus, andere Rekordbox-Versionen sowie ein längerer DJ-Betrieb. Der gespeicherte Key und Original-BPM stammen aus der Bibliothek; aktuelle Deck-BPM stammen aus dem geprüften `@BPM`-Objekt.

## Master-Overlay live

Die erweiterte Bridge mit IPC-Version 2 erkannte in derselben laufenden Rekordbox-Instanz die Master-Abfolge **Deck 1 → Deck 4 → Deck 1 → Deck 4**. `/api/master` enthielt danach den aktuellen Track „PRVLG (Original Mix)“ von Blend (174 BPM, 7A, Album „Chrome“) sowie drei korrekt geordnete History-Einträge einschließlich „1873“ von Data 3. Die später erneut auftauchenden Tracks hatten neue `entryId`-Werte. Beide verwendeten Cover lieferten auch über die History-URLs HTTP 200 mit MIME `image/jpeg` (152249 beziehungsweise 145012 Bytes).

Ein zusätzlicher lesender Speicherabruf bestätigte für die vier Master-Devices vtable-RVA `0x03B85620`, Namen `Master` und Cachewerte **1, 1, 1, 0**, passend zum API-Master Deck 4. Die neuen drei Codeprüfungen wurden außerdem direkt an der EXE verifiziert. Ein vorheriger Rider-Debugger-Versuch konnte den Quell-Haltepunkt mangels Release-Debug-Symbolen nicht zuordnen; er lieferte keine belastbaren Feldwerte. Der Debugger wurde getrennt, der eigene Haltepunkt entfernt, Rekordbox lief weiter. Die Master-Funktion wurde anhand der direkten Lese- und HTTP-Ergebnisse bestätigt.
## Deck-Einstellungen, Timeline und Sprachen

Die Browserprüfung umfasst zusätzlich getrennt gespeicherte Einstellungen für vier Decks, Übernahme auf alle Decks, generierte OBS-URLs, English als Standard, Deutsch/English-Wechsel im Dashboard und auf Einstellungsseiten sowie persistierte Sprachwahl. Die Darstellung wurde mit einem hellen Preset, 32-px-Schrift, transparentem Hintergrund, gestapeltem Cover und deaktivierten Beschriftungen geprüft. Screenshots `deck-settings.png` und `master-timeline.png` wurden visuell geprüft.

Timeline-Fälle im synthetischen Browser-Test: aktuelle Position und Gesamtlänge, 50-%-Fortschritt, negativer Vorlauf, Begrenzung auf 0–100 %, fehlende Daten ohne erfundenen Nullwert, Wiederherstellung, unveränderte Position ohne neue Bewegung und Trackwechsel. Nur die aktuelle Master-Karte hat eine sichtbare Timeline; History-Karten haben keine. Die bisherigen Prüfungen für Skalierung, Ausrichtung, beide BPM-Werte und Animationen bestehen weiterhin. Die HTTP-Tests prüfen auch sämtliche neuen Module, CSS- und Sprachdateien samt MIME-Typ und fester Routenliste.

Die Konsole wurde mit englischer Standardhilfe, deutscher Hilfe (`--lang de --help`) und Ablehnung einer ungültigen Sprache geprüft. Beide Oberflächen und die Konsole verwenden dieselben Übersetzungsdateien.

Die Bridge mit **IPC-Version 3** wurde erneut in die laufende Rekordbox-Instanz geladen. Sie meldete `connected`, `demo: false`, Version `7.2.18.0`, Master Deck 2 und frische Messungen (172 ms beim dokumentierten Abruf). Die vorherige DLL wurde zuerst vollständig entladen; Rekordbox lief weiter.

| Deck | Position (ms) | Dauer (ms) |
|---|---:|---:|
| 1 | 221722 | 295550 |
| 2 | 68335 | 292414 |
| 3 | 52840 | 271526 |
| 4 | 181631 | 181631 |

Ein unabhängiger lesender Speicherabruf bestätigte für alle acht Devices die Namen `@CurrentTime`/`@TotalTime`, vtable `0x03B85620` und exakt dieselben Werte. `/api/master.current` lieferte die Position und Dauer von Deck 2. Die Werte bleiben beim Ergänzen von Bibliotheksmetadaten erhalten. Die fünf zusätzlichen Codeprüfungen wurden vorab am installierten EXE-Build verifiziert.

Ein zusätzlicher Edge-Durchlauf gegen den echten Server bestätigte die Master-Timeline (1:08 / 4:52), das geladene Master-Cover, die Deck-Settings-Vorschau und den deutschen Verbindungsstatus nach Sprachwechsel. Screenshots: `master-timeline-live.png` und `deck-settings-live.png`. Im separat gestarteten Demo-Server stieg die native API-Position von 0 auf 657 ms bei 240000 ms Gesamtlänge; die deutsche Diagnose wurde ebenfalls geprüft.

Live geprüft wurden die vorhandenen stationären Positionen. Negative Vorlaufpositionen, Sprünge und Pausenverhalten wurden mit synthetischen API-Daten im Browser geprüft; eine gesonderte manuelle Play-/Seek-Bedienung in Rekordbox und ein Test in OBS werden hier nicht behauptet.

## Veröffentlichungsvorbereitung am 15. September 2026

Der Release-Build und alle fünf nativen Tests wurden erneut ausgeführt. Dabei wurde ein sporadischer Windows-Verbindungsabbruch bei abgelehnten POST-Anfragen gefunden: Die frühe Ablehnung konnte den Socket schließen, bevor der Anfrageinhalt vollständig gelesen war. Schreibmethoden werden jetzt von normalen, ausschließlich ablehnenden Handlern beantwortet, nachdem httplib den auf 1024 Bytes begrenzten Inhalt gelesen hat.

Zusätzliche Prüfungen decken POST, PUT, PATCH, DELETE und OPTIONS sowie wiederholte POST/GET-Folgen auf derselben Verbindung ab. Nach der Korrektur bestanden alle fünf Tests und 20 aufeinanderfolgende HTTP-Testläufe. Die Änderung ergänzt keine schreibende API und verändert den nativen Rekordbox-Zugriff nicht.

Die öffentliche README-Vorschau wurde mit dem bestehenden Browser-Testserver und synthetischen Tracks erstellt. Bibliotheksdaten, Rekordbox-Binärdateien, lokale IDE-Einstellungen und Build-Ausgaben sind nicht Bestandteil des Git-Repositories. Diese Veröffentlichungsvorbereitung erweitert die Versionskompatibilität nicht.
