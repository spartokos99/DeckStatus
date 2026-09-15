# Metadaten- und Cover-Datenquelle

`src/artwork.cpp` liest `djmdContent.ImagePath` anhand der Content-ID aus der
lokalen `master.db`. Der Datenbankpfad kommt aus `--database`, alternativ aus
`%APPDATA%/Pioneer/rekordboxAgent/storage/options.json` (`options` / `db-path`),
sonst aus `%APPDATA%/Pioneer/rekordbox/master.db`.

`ArtworkResolver::enrich` ergaenzt anhand derselben gebundenen Content-ID
Titel und Original-BPM aus `djmdContent`, Artist aus `djmdArtist.Name`, Album
aus `djmdAlbum.Name`, Key aus `djmdKey.ScaleName`, Genre aus `djmdGenre.Name`
und Label aus `djmdLabel.Name`. LEFT JOINs erhalten Titel mit fehlenden
optionalen Beziehungen; SQL-NULL wird als leere Zeichenfolge ausgegeben.
Die Datenbank speichert BPM mit Faktor 100. Die Live-BPM und Deck-/Track-IDs
bleiben unveraendert. Bei fehlendem Eintrag werden vorhandene Collection-
Metadaten geloescht und `metadata_available` auf 0 gesetzt. Zeichenfolgen
werden an vollstaendigen UTF-8-Zeichen abgeschnitten; ungueltige UTF-8-Praefixe
werden leer ausgegeben. Abfragen und Cover-Cache teilen sich einen Mutex.

Die Datenbank wird mit `SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX` geoeffnet.
Es werden ausschliesslich SELECT-Abfragen ausgefuehrt. Die SQLCipher-kompatible
`sqlite3.dll` sowie `zlib.dll`/`zlib1.dll` werden aus dem kanonischen Verzeichnis
der ausgewaehlten lokalen Rekordbox-Installation geladen. Es werden keine
Rekordbox-DLLs mit diesem Projekt verteilt.

Die publizierten Kompatibilitaetskonstanten `BLOB` und `BLOB_KEY` sowie die
Dekodierreihenfolge (Base85, XOR, zlib) stammen aus **pyrekordbox** von Dylan Jones
(MIT-Lizenz, abgerufen am 14. September 2026):

- [masterdb/database.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/masterdb/database.py): `BLOB`, Datenbankinitialisierung und `share`-Verzeichnis.
- [utils.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/utils.py): `BLOB_KEY` und `deobfuscate`.
- [config.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/config.py): `options.json` und `db-path`.
- [masterdb/models.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/masterdb/models.py): `djmdContent` und die Metadatenbeziehungen sowie `ImagePath`.
- [SQLite: Opening a database](https://www.sqlite.org/c3ref/open.html): Read-only- und Full-mutex-Flags.
- [SQLite: Binding values](https://www.sqlite.org/c3ref/bind_blob.html): gebundene Content-ID.

Der dekodierte Schluessel wird ausschliesslich an `sqlite3_key` uebergeben,
anschliessend aus dem temporaeren Puffer geloescht und weder protokolliert noch
per HTTP ausgegeben. Fuer unverschluesselte Testdatenbanken wird kein Schluessel
gesetzt.

Cover-Dateien werden nur innerhalb des kanonischen Datenbankverzeichnisses
akzeptiert, einschliesslich `share/ARTWORK`. Der tatsaechlich geoeffnete
Dateihandle wird erneut auf dieses Verzeichnis geprueft. Netzwerk-/Geraetepfade,
Traversal und alternative Datenstroeme werden abgewiesen. Erlaubt sind anhand
ihrer Dateisignatur erkannte JPEG-, PNG-, GIF-, WebP- und BMP-Bilder bis 8 MiB.
Der Cache haelt maximal 16 Eintraege und 32 MiB; erfolgreiche Eintraege gelten
10 Sekunden, fehlende Cover 2 Sekunden. Streamingtitel ohne lokalen
`djmdContent`-Eintrag oder ohne gespeichertes Bild liefern kein Cover.

## Lizenzhinweis fuer die uebernommenen pyrekordbox-Konstanten und den Algorithmus

MIT License

Copyright (c) 2022-2025, Dylan Jones

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
