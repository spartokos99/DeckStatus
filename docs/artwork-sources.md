# Metadata and artwork sources

`src/artwork.cpp` reads `djmdContent.ImagePath` from the local `master.db`, using the content ID. The database path comes from `--database`, otherwise from `%APPDATA%/Pioneer/rekordboxAgent/storage/options.json` (`options` / `db-path`), with `%APPDATA%/Pioneer/rekordbox/master.db` as the fallback.

`ArtworkResolver::enrich` uses the same bound content ID to obtain the title and original BPM from `djmdContent`, artist from `djmdArtist.Name`, album from `djmdAlbum.Name`, key from `djmdKey.ScaleName`, genre from `djmdGenre.Name`, and label from `djmdLabel.Name`. LEFT JOINs retain tracks with missing optional relationships; SQL NULL becomes an empty string. The database stores BPM multiplied by 100. Live BPM and deck/track IDs remain unchanged. A missing row clears existing collection metadata and sets `metadata_available` to 0. Strings are truncated at complete UTF-8 character boundaries; invalid UTF-8 prefixes produce empty strings. Queries and the artwork cache share a mutex.

The database is opened with `SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX`. Only SELECT queries are executed. The SQLCipher-compatible `sqlite3.dll` and `zlib.dll`/`zlib1.dll` are loaded from the canonical directory of the selected local Rekordbox installation. No Rekordbox DLLs are distributed with this project.

The published compatibility constants `BLOB` and `BLOB_KEY` and their decoding sequence (Base85, XOR, zlib) come from **pyrekordbox** by Dylan Jones (MIT license, accessed September 14, 2026):

- [masterdb/database.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/masterdb/database.py): `BLOB`, database initialization and the `share` directory.
- [utils.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/utils.py): `BLOB_KEY` and `deobfuscate`.
- [config.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/config.py): `options.json` and `db-path`.
- [masterdb/models.py](https://github.com/dylanljones/pyrekordbox/blob/master/pyrekordbox/masterdb/models.py): `djmdContent`, metadata relationships and `ImagePath`.
- [SQLite: Opening a database](https://www.sqlite.org/c3ref/open.html): read-only and full-mutex flags.
- [SQLite: Binding values](https://www.sqlite.org/c3ref/bind_blob.html): bound content IDs.

The decoded key is passed only to `sqlite3_key`, then erased from the temporary buffer. It is never logged or returned over HTTP. No key is set for unencrypted test databases.

Artwork files are accepted only within the canonical database directory, including `share/ARTWORK`. The actual opened file handle is checked against that directory again. Network/device paths, traversal and alternate data streams are rejected. JPEG, PNG, GIF, WebP and BMP images are identified by their signatures and limited to 8 MiB. The cache holds at most 16 entries and 32 MiB; successful entries last 10 seconds and missing artwork entries 2 seconds. Streaming tracks without a local `djmdContent` row or stored image have no artwork.

## License notice for the reused pyrekordbox constants and algorithm

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
