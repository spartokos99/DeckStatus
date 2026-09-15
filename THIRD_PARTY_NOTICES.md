# Quellen und Abhängigkeiten

## Mitgelieferte Bibliotheken

- [cpp-httplib 0.20.0](https://github.com/yhirose/cpp-httplib/tree/v0.20.0), Copyright Yuji Hirose, MIT. Lizenz: `vendor/httplib/LICENSE`.
- [JSON for Modern C++ 3.12.0](https://github.com/nlohmann/json/tree/v3.12.0), Copyright Niels Lohmann, MIT. Lizenz: `vendor/nlohmann/LICENSE.MIT`.

## Rekordbox-Interoperabilität

Die Recherche zu älteren UIPlayer-Layouts begann mit [Unreal-Dan/RekordBoxSongExporter](https://github.com/Unreal-Dan/RekordBoxSongExporter/tree/1da869afcf4546720a33992ec1ef75d64243ef93/Module). Die Implementierung in diesem Projekt ist eigenständig; das enthaltene 7.2.18-Profil wurde direkt anhand der installierten EXE geprüft. Interne RowDataTrack-Funktionen werden nicht aufgerufen. Der Recherche-Checkout unter `vendor/RekordBoxSongExporter` ist keine Build-Abhängigkeit und gehört nicht zum ausgelieferten Programm.

Die Datenbankanbindung verwendet die auf dem Rechner installierte `sqlite3.dll` von Rekordbox. Rekordbox-Binärdateien werden nicht mitgeliefert. Herkunft des Datenbankformats und der Schlüsselkonstante: [pyrekordbox](https://github.com/dylanljones/pyrekordbox), MIT; Details in `docs/artwork-sources.md`.

Rekordbox und seine Produktnamen gehören den jeweiligen Rechteinhabern. Dieses Projekt ist eine unabhängige lokale Erweiterung.
