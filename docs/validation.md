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

`/api/health` und `/overlay?deck=1` lieferten HTTP 200. Der Wechsel von leeren zu geladenen Decks wurde ohne Neustart der Bridge erkannt. Nach dem Beenden der Bridge war `DeckStatusBridge.dll` nicht mehr in Rekordbox geladen; Rekordbox lief weiter.

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

## Umbenennung in DeckStatus am 15. September 2026

Anwendung, CMake-Targets, Solution/Projektdatei, C++-Namensraum, IPC-Objekte und Windows-Dateiinformationen tragen jetzt den Namen DeckStatus. Die neuen Laufzeitdateien heißen `DeckStatus.exe` und `DeckStatusBridge.dll`; die Produktnamen wurden an beiden kompilierten Dateien geprüft. Das IPC-Layout bleibt Version 3, mit eigener DeckStatus-Kennung und Objektnamen.

Der Build und alle fünf nativen Tests bestanden mit den umbenannten Dateien, einschließlich Laden, doppelter Anbindung und Entladen der neuen DLL im isolierten Testprozess. Die Browserprüfung deckt zusätzlich die Übernahme alter Sprach-, Deck- und Master-Einstellungen in `deckstatus.*` ab und bestätigt, dass neue Werte dabei Vorrang behalten.

Dashboard, Einstellungsseite und drei Overlay-Designs wurden für das README mit synthetischen Trackdaten neu aufgenommen. Der Rekordbox-Speicherzugriff wurde durch die Umbenennung nicht auf andere Builds erweitert; die ausschließlich für 7.2.18.0 dokumentierten Live-Prüfungen bleiben maßgeblich.

## Ergänzung: DeckStatus 1.3.1 – 15.09.2026

- Neue WASAPI-Audioquelle im Host, separater Waveform-Renderer und eigene Einstellungsseite; die bestehende Rekordbox-Profilprüfung bleibt unverändert.
- Release-Build mit MSVC erfolgreich, sechs von sechs nativen CTest-Tests bestanden (einschließlich des konfigurierten Datenbanktests).
- Bisherige Deck-/Master-Browser-Suite bestanden.
- Neue Waveform-Browser-Suite bestanden: Audioeingang zuerst, kein automatischer Capture-Start, Start/Quellenwechsel/Stopp, sechs reale Canvas-Darstellungen, FFT-Frequenz/Amplitude, Stereo-/Kanalwahl, Rauschschwelle, Grenzen, Sprache/Speicherung, konstante Hintergrundtransparenz mit Nachleuchten, Stille und Fehler-/Stale-Zustände. Sämtliche Browser-Audiosignale waren synthetisch.
- Standard-Audiotest liest die Geräteliste und prüft PCM-/Float-Konvertierung, NaN/Clipping, Mono/Stereo, Fensterreihenfolge, Stille und ungültige Pakete. CTest öffnet keine Audioquelle.
- Separater opt-in Hardwaretest `audio_test.exe --loopback-smoke`: vorhandenen Windows-Ausgabe-Loopback zweimal geöffnet, Mixformat erhalten, gestoppt und erneut geöffnet. Kein Mikrofon geöffnet, keine Audiodateien gespeichert. Der Test prüft den Stream-Lebenszyklus, nicht die Wiedergabetreue eines Rekordbox-Signals.
- Ressourcenprüfung der kompilierten EXE bestanden: Produktversion 1.3.1 / Dateiversion 1.3.1.0 und neun eingebettete Icon-Bilder (16, 20, 24, 32, 40, 48, 64, 128, 256 px).
- Vorschau und Icon visuell kontrolliert; öffentliche Screenshots verwenden gekennzeichnete synthetische Daten.
- Mikrofon-/Interface-Erfassung, Rekordbox-zu-Loopback-Signalübertragung und eine eigene OBS-Waveform-Sitzung bleiben nicht separat validiert.
- Die produktive Rekordbox-Bridge wurde für diese Arbeiten nicht gestartet. Die einzige weiterhin live validierte Rekordbox-Version ist die eigene Installation **7.2.18.0, Windows x64**.


## Ergänzung: Dashboard und Full History – 15.09.2026 (unveröffentlichter Quellstand)

- Waveform und Full History sind im Dashboard erreichbar. Dashboard, beide Overlay-Einstellungsseiten, Waveform-Einstellungen und History verwenden dieselbe Icon-SVG-Vorlage wie die EXE.
- Vier Dashboard-Timelines mit Position/Gesamtdauer, begrenztem Fortschritt, negativem Vorlauf und nicht verfügbaren/veralteten Werten geprüft.
- Vollständiges Sitzungsgedächtnis für Master-Track-Wechsel zusätzlich zum unveränderten 50-Track-Fenster des Overlays. Tests prüfen Wiederholungen, alte Cover/Metadaten, stabile Cursor, Seitenbegrenzung und ungültige API-Parameter.
- Alle sechs nativen CTest-Tests und alle drei Browser-Suiten erfolgreich. Die neue Browser-Suite nutzt 137 synthetische History-Einträge und prüft Navigation, Logo, vier Timelines, Seitenwechsel, zusätzliche Trackwechsel, Cover, XSS-Abwehr, EN/DE, Ausfall und leere Sitzung.
- Alle sechs öffentlichen README-Screenshots mit englischer Oberfläche und englischen synthetischen Daten neu erzeugt und visuell kontrolliert. Der neue Generator prüft auch die Sprache der eingebetteten Vorschauen.
- Inkrementeller Build kopiert geänderte Webdateien auch ohne erneutes Linken der EXE; Hashvergleich zwischen Quell- und Build-Datei erfolgreich.
- History bleibt auf die aktuelle App-Sitzung begrenzt. MASTER-Wechsel sind weiterhin kein Nachweis hörbarer Wiedergabe. Die produktive Bridge wurde nicht gestartet; keine zusätzliche Rekordbox-Version wurde validiert.

## Ergänzung: DeckStatus 1.3.2 – 15.09.2026

- Release-Build erfolgreich; alle sechs nativen CTest-Tests einschließlich des konfigurierten Datenbanktests bestanden.
- Alle drei Browser-Suiten bestanden: Deck/Master, Waveform und Dashboard/Full History.
- Die native Demo verwendet englische Tracktitel und Künstler: `Night Drive "Live"` von `Orbit & Friends` sowie `First Light` von `Studio North`. Die Spracheinstellung übersetzt weiterhin die Oberfläche und Diagnosen, nicht die Trackmetadaten.
- Alle sechs öffentlichen README-Bilder neu als `*-en.png` erzeugt. Der Generator prüft englische Labels und die englischen Tracktitel/Künstler auch in eingebetteten Vorschauen. Waveform- und Dashboard-Bild zusätzlich visuell kontrolliert. Der deutsche Waveform-Browsertest schreibt seine Bilder ausschließlich unter `build/test-artifacts` mit separatem Namen.
- Ressourcenprüfung bestanden: EXE-Produktversion `1.3.2`, Dateiversion `1.3.2.0`, neun eingebettete Icon-Größen von 16 bis 256 px.
- Die vorherigen Dashboard-/Full-History-Ergänzungen sind Bestandteil von 1.3.2. Die produktive Bridge und Audioerfassung wurden für dieses Update nicht gestartet. Es bleibt ausschließlich die eigene Rekordbox-Installation **7.2.18.0 unter Windows x64** live validiert.

## Ergänzung: DeckStatus 1.4.0 – 15.09.2026

- Opt-in-ProLink-Modus mit eigener Geräte-Setup-Seite, gemeinsamer gruppierter Navigation und serverseitigen Modussperren. Der normale Start bleibt im Rekordbox-Modus; das Injection-Profil und die bestehenden Datenbank-/Prozessoptionen wurden nicht verändert.
- Sieben native CTest-Tests bestanden. Der zusätzliche ProLink-Test nutzt ausschließlich einen eigenen Hilfsprozess ohne Netzwerkzugriff: Befehlsvalidierung, fehlende Laufzeit, JSON-/Cover-Übertragung, veraltete Daten, Prozessabbruch, Neustart mit getrennten Track-IDs und Beenden des eigenen Prozesses. HTTP-Tests prüfen beide Moduskonfigurationen, direkte inaktive Endpunkte, JSON-Validierung und Origin-Sperren.
- Java-Modelltests mit der mitgelieferten Temurin-Laufzeit bestanden: synthetische CDJ-Statuspakete, Statusflags/BPM, Zeitgrenzen, unterschiedliche Player-/Medien-/Slot-Identitäten, Medienwechsel, unbekannte Werte und Unicode-JSON. Ein echter Hilfsprozess prüft zusätzlich UTF-8-Ausgabe trotz Windows-1252-Standardausgabe und das Beenden bei geschlossenem Eingabekanal. Diese Tests öffnen keine Netzwerksockets.
- Der vollständige Programmtest prüft EN/DE-Demo, unveränderten Standardmodus, ProLink-Laufzeit, Modus-/API-Sperren, Webrouten und ausgeschaltete Audioerfassung. Der tatsächliche Suchversuch liefert auf diesem Rechner korrekt `prolinkPortsBusy`, weil die benötigten Ports belegt sind; es wird kein Gerät verbunden. Dabei wurde ein Windows-Zeichenkodierungsfehler der Java-Standardausgabe gefunden und durch eine explizite UTF-8-Schnittstelle behoben.
- Vier Browser-Suiten bestanden. ProLink prüft zusätzlich Suche ohne automatisches Verbinden, Playerwahl, Verbinden/Trennen, sichere Gerätenamen, Modusanzeige und Sperren, gemeinsame Audiofunktionen, Mobilansicht sowie EN/DE. Die bestehenden Overlay-/Waveform-/History-Suiten bestehen weiterhin.
- Alle sieben öffentlichen README-Screenshots mit englischer Oberfläche und synthetischen Daten neu erzeugt. ProLink-Setup und Dashboard mit der neuen Navigation visuell kontrolliert.
- EXE-Ressourcenprüfung bestanden: Produktversion 1.4.0, Dateiversion 1.4.0.0 und neun eingebettete Icon-Größen.
- **Kein Live-Test an CDJ-3000 oder DJM-A9 erfolgt.** ProLink ist experimentell; weder Firmwarekombinationen noch reale USB-/Streaming-Workflows sind damit validiert. Die produktive Rekordbox-Bridge und Audioerfassung wurden für diese Arbeiten nicht gestartet. Die bisherige Rekordbox-Livevalidierung bleibt auf die eigene Installation 7.2.18.0 begrenzt.
