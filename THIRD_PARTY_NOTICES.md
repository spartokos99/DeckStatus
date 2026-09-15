# Quellen und Abhängigkeiten

## ProLink runtime (1.4.0)

The optional ProLink process uses the following **unmodified** runtime libraries, pinned in `prolink/dependencies.lock.json`. Their binary JARs retain upstream notices; matching source JARs and POMs are distributed under `prolink/licenses` in the portable ZIP. This directory provides the exact sources for the distributed library versions.

| Component | Version | Upstream licence / source |
|---|---|---|
| Beat Link | 8.0.0 | EPL-1.0 as declared by the release POM; [source](https://github.com/Deep-Symmetry/beat-link) |
| Crate Digger | 0.2.1 | EPL-1.0; [source](https://github.com/Deep-Symmetry/crate-digger) |
| Electro | 0.1.4 | EPL-1.0; [source](https://github.com/Deep-Symmetry/electro) |
| sqlite-jdbc (Willena) | 3.49.0.0 | Apache-2.0 wrapper and bundled SQLite notices; [source](https://github.com/Willena/sqlite-jdbc) |
| Kaitai Struct Java runtime | 0.10 | MIT; [source](https://github.com/kaitai-io/kaitai_struct_java_runtime) |
| SLF4J API / Simple | 1.7.36 | MIT; [source](https://github.com/qos-ch/slf4j) |
| API Guardian | 1.1.2 | Apache-2.0; [source](https://github.com/apiguardian-team/apiguardian) |
| Remote Tea ONC/RPC | 1.1.4 | LGPL; licence text in the corresponding sources; [project](https://remotetea.sourceforge.net/) |
| Apache Commons Math | 3.6.1 | Apache-2.0; [source](https://commons.apache.org/proper/commons-math/) |
| JSON-java | 20250517 | Public domain as declared by the release POM; [source](https://github.com/stleary/JSON-java) |

The ZIP also includes unmodified **Eclipse Temurin OpenJDK JRE 21.0.12.1+1**, Windows x64. Its `legal` directory, `NOTICE` and other upstream notices are preserved in `prolink/runtime`. OpenJDK is distributed under GPLv2 with the Classpath Exception, plus the included third-party notices. Corresponding upstream source is available with the [matching Temurin release](https://github.com/adoptium/temurin21-binaries/releases/tag/jdk-21.0.12.1%2B1). The locally installed compiler is not redistributed.

The DeckStatus adapter source is in `prolink/src`; no AlphaTheta/Pioneer code, firmware or user media is included. Protocol references: [Deep Symmetry DJ Link analysis](https://djl-analysis.deepsymmetry.org/djl-analysis/).

## Mitgelieferte Bibliotheken

- [cpp-httplib 0.20.0](https://github.com/yhirose/cpp-httplib/tree/v0.20.0), Copyright Yuji Hirose, MIT. Lizenz: `vendor/httplib/LICENSE`.
- [JSON for Modern C++ 3.12.0](https://github.com/nlohmann/json/tree/v3.12.0), Copyright Niels Lohmann, MIT. Lizenz: `vendor/nlohmann/LICENSE.MIT`.

## Rekordbox-Interoperabilität

Die Recherche zu älteren UIPlayer-Layouts begann mit [Unreal-Dan/RekordBoxSongExporter](https://github.com/Unreal-Dan/RekordBoxSongExporter/tree/1da869afcf4546720a33992ec1ef75d64243ef93/Module). Die Implementierung in diesem Projekt ist eigenständig; das enthaltene 7.2.18-Profil wurde direkt anhand der installierten EXE geprüft. Interne RowDataTrack-Funktionen werden nicht aufgerufen. Der Recherche-Checkout unter `vendor/RekordBoxSongExporter` ist keine Build-Abhängigkeit und gehört nicht zum ausgelieferten Programm.

Die Datenbankanbindung verwendet die auf dem Rechner installierte `sqlite3.dll` von Rekordbox. Rekordbox-Binärdateien werden nicht mitgeliefert. Herkunft des Datenbankformats und der Schlüsselkonstante: [pyrekordbox](https://github.com/dylanljones/pyrekordbox), MIT; Details in `docs/artwork-sources.md`.

Rekordbox und seine Produktnamen gehören den jeweiligen Rechteinhabern. Dieses Projekt ist eine unabhängige lokale Erweiterung.
