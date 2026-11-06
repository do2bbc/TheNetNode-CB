# TheNetNode-CB 1.79cb55

Packet-Radio-Node fuer Linux, CB-Version von DAA531, weitergefuehrt von DO2BBC.
Quellcode: https://github.com/do2bbc/TheNetNode-CB

## 1. Kompilieren

Unter Debian, Ubuntu, Linux Mint oder Raspberry Pi OS:

```bash
sudo apt update
sudo apt install git build-essential linux-libc-dev
git clone https://github.com/do2bbc/TheNetNode-CB.git
cd TheNetNode-CB
make tnn
```

Das fertige Programm liegt unter `bin/tnn`. Auf dem Rechner kompilieren, auf
dem TNN laufen soll: Eine Raspberry-Pi-Binaerdatei funktioniert nicht auf
einem x86_64-PC. Der Build wurde mit GCC 12 unter Debian 12 getestet.
Keine eigenen Optimierungsflags wie `-O2` hinzufuegen.

## 2. Neuinstallation

**Nur fuer eine neue Installation!** `make baseinstall` kopiert
Beispielkonfigurationen und ueberschreibt dabei vorhandene Einstellungen.
Wenn `/usr/local/tnn` bereits existiert, stattdessen Abschnitt 3 verwenden.

Im Quellverzeichnis:

```bash
sudo make baseinstall
sudo nano /usr/local/tnn/tnn.ini
sudo nano /usr/local/tnn/tnn179.pas
sudo nano /usr/local/tnn/tnn179.tnb
sudo nano /usr/local/tnn/ax25ip.cfg
```

Vor dem ersten Start anpassen:

| Datei | Einstellungen |
|---|---|
| `tnn.ini` | Geraete, Schnittstellentypen und Zuordnung der TNN-Ports |
| `tnn179.pas` | Eigenes Node-Rufzeichen, Alias, Sysop- und Konsolenpasswort |
| `tnn179.tnb` | Portparameter und Node-Kommandos beim Start |
| `ax25ip.cfg` | AXIP-Verbindungen, sofern verwendet |

Die Vorlage erwartet serielle Hardware an `/dev/ttyS0`. Ohne diese Hardware
muss der Geraeteblock ersetzt werden. Beispiele fuer weitere Schnittstellen
stehen in [tnnini.all](os/linux/ini/tnnini.all).
Die Reihenfolge der Zeilen in `tnn179.pas` beibehalten und die
Beispielpasswoerter unbedingt ersetzen. Netzwerkports und Firewall passend
zum eigenen Betrieb konfigurieren; keine unveraenderte Testkonfiguration
ins Internet stellen.

Zum ersten Start in der Konsole:

```bash
cd /usr/local/tnn
sudo ./tnn
```

Mit dem selbst gesetzten Konsolenpasswort anmelden, dann `Esc`, `c` und
`Enter` fuer die erste Node-Konsole eingeben. Fehler beim Einlesen der
Startkommandos stehen in `/usr/local/tnn/startup.log`.

Die Installation richtet keinen systemd-Dienst automatisch ein. Fuer einen
ersten Test reicht der Konsolenstart; ein vorhandener Dienst kann weiterhin
verwendet werden.

## 3. Bestehende Installation aktualisieren

Im Quellverzeichnis den neuen Stand holen und bauen:

```bash
git pull --ff-only
make -B tnn
```

**TNN vor dem Kopieren sauber beenden**, damit die Einstellungen gespeichert
werden. Bei einem vorhandenen Dienst namens `tnn.service`:

```bash
sudo systemctl stop tnn.service
```

Ohne Dienst TNN auf dem bisher verwendeten Weg beenden. Danach die gestoppte
Installation sichern und **nur die Binaerdatei** ersetzen:

```bash
sudo tar -czf "/var/backups/tnn-$(date +%Y%m%d-%H%M%S).tar.gz" -C /usr/local tnn
sudo install -m 0755 bin/tnn /usr/local/tnn/tnn
```

Falls das Backup fehlschlaegt, nicht mit dem Ersetzen fortfahren.
Anschliessend wie bisher starten, bei dem genannten Dienst:

```bash
sudo systemctl start tnn.service
sudo systemctl status tnn.service --no-pager
```

Ohne Dienst wieder aus `/usr/local/tnn` starten. Mit dem Node-Kommando
`VERSION` den Stand pruefen. Bei einem anderen Installationspfad die Pfade
entsprechend anpassen.

**Beim Update weder `make baseinstall` noch eine Neuinstallation ausfuehren.**
Das obige Vorgehen erhaelt Rufzeichen, Passwoerter, Links und Portkonfiguration.

## 4. Onlinehilfe nachinstallieren

Die Onlinehilfe ist ein separates Programm. Falls `HELP` keine Hilfe anzeigt,
im Quellverzeichnis bauen und installieren:

```bash
make help
sudo install -d -m 0700 /usr/local/tnn/userexe /usr/local/tnn/sysexe
sudo install -m 0700 bin/help /usr/local/tnn/userexe/help
sudo ln -sfn ../userexe/help /usr/local/tnn/sysexe/syshelp
sudo cp --update=none contrib/onlhelp/ohu.txt contrib/onlhelp/ohs.txt /usr/local/tnn/
```

Danach im Node `HELP` beziehungsweise als Sysop `SYSHELP` aufrufen.
Ein Neustart ist nicht erforderlich. Vorhandene Hilfetexte werden durch
`cp --update=none` nicht ueberschrieben; die Node-Konfiguration bleibt unveraendert.
Bei abweichenden Installations- oder Programmpfaden die Befehle entsprechend
anpassen.

## Optionale Tests und weitere Dokumentation

Diese Regressionstests laufen offline und verbinden sich nicht mit anderen Nodes:

```bash
sh tests/gcc-regressions.sh
sh tests/runtime-regressions.sh
sh tests/startup-banner.sh
sh tests/socket-set.sh
sh tests/monitor-hostmode.sh
sh tests/hostmode-reconnect.sh
```

Weitere Unterlagen: [doc](doc/), [Versionshistorie](history/) und
[Lizenzbedingungen](alas.txt).

## Windows: portables 32-Bit-CB-Testpaket

Das Windows-Paket wird aus diesen CB-Quellen gebaut, nicht aus dem
AFU-Repository `tnn179`. Es enthaelt TNN, Hilfsprogramme, eine neutrale
Offline-Konfiguration, lokale Laufzeit-/Crashlogs, Dokumentation, Lizenz,
passenden Quellcode in `SOURCE.zip` und SHA256-Pruefsummen.
Start, Konsolenanmeldung, Minuten-Heartbeat und sauberes Beenden wurden
unter deutschem Windows XP SP3 x86 in VirtualBox getestet.
Es ist Testsoftware; echte TNCs und serielle Schnittstellen sind nicht
verifiziert. XP bleibt ohne Internetzugang.

Unter Debian/Ubuntu mit MinGW-w64, Wine (einschliesslich 32-Bit-Unterstuetzung),
Xvfb, GCC, Make, Git, Perl, Zip und Unzip:

```bash
xvfb-run -a sh tests/package-win32.sh /absoluter/pfad/zur/ausgabe
```

Das Skript verwendet ausschliesslich den eingecheckten Stand (`git archive
HEAD`), keine lokalen Konfigurationen. `BUILD.txt` nennt Repository und
genauen Commit. Direkter Cross-Build in einer separaten Quellkopie:

```bash
make -f tnn32.mak all PREFIX=i686-w64-mingw32-
```

Keine SDK-Header veraendern und das historische `hardpatch`-Target nicht
ausfuehren. Hardwarebeispiele und Diagnosegrenzen stehen in
[der Paket-Anleitung](os/win32/portable/README.txt).

Der GitHub-Workflow `Windows portable CB test package` baut bei Pull Requests,
`v*`-Tags, manueller Ausloesung und veroeffentlichten Releases. ZIP und
Pruefsumme stehen als Workflow-Artefakt bereit; bei einem veroeffentlichten
Release werden sie automatisch angehaengt. Der Workflow erstellt selbst
keinen Release. Bei nachtraeglich angehaengten Builds aus einem neueren
Commit muss der abweichende Quellstand in den Release-Notizen genannt werden;
die automatisch angebotenen Tag-Quellarchive sind dann nicht der passende
Windows-Quellcode. Dafuer ist `SOURCE.zip` im Paket massgeblich.

## Debian-Pakete: amd64, arm64 und armhf

Die `.deb`-Pakete sind fuer Debian 12 / Raspberry Pi OS Bookworm und neuer.
Mit `dpkg --print-architecture` die passende Architektur bestimmen:

- `amd64`: 64-Bit-Intel/AMD-PC.
- `arm64`: Raspberry Pi mit 64-Bit-Betriebssystem.
- `armhf`: ARMv7 mit 32-Bit-Betriebssystem (Pi 2 und neuer).
  **Nicht fuer Pi 1, Pi Zero oder Zero W mit ARMv6.**

Installieren mit `sudo apt install ./tnn-cb_*.deb`.
Installiert werden `/usr/bin/tnn-cb`, Hilfsprogramme und Dokumentation;
kein Dienst wird angelegt oder gestartet. Bestehende Dateien unter
`/usr/local/tnn` und eigene Laufzeitkonfigurationen bleiben unangetastet.
Eine ausfuehrliche Anleitung fuer Neuinstallation und Umstellung steht unter
`/usr/share/doc/tnn-cb/README.Debian` beziehungsweise
[hier im Quellcode](packaging/debian/README.Debian).

Pakete enthalten `BUILD.txt`, ALAS und die kompletten passenden Quellen als
`SOURCE.tar.gz`. ARM-Binaerdateien werden crosskompiliert und mit QEMU getestet;
dies ersetzt keinen Funkhardwaretest. Die Windows-Diagnosefunktion wird nicht
als Linux-Funktion ausgegeben.

Reproduzieren auf einem amd64-Debian-12-Buildsystem mit GCC, Make, Git,
dpkg-dev, Python 3, QEMU User und den Debian-Crosscompilern/Sysroots fuer
AArch64 und ARM hard-float:

```bash
sh tests/package-debian.sh arm64 /absoluter/pfad/zur/ausgabe
python3 tests/debian-package.py /absoluter/pfad/zur/ausgabe/*.deb arm64
```

Entsprechend `amd64` oder `armhf` verwenden. Der Workflow `Debian CB packages`
baut und prueft alle drei Architekturen bei Pull Requests, manueller Ausloesung,
`v*`-Tags und veroeffentlichten Releases; beim Release werden die Pakete und
SHA256-Pruefsummen automatisch angehaengt. Keine lokalen Node-Dateien gelangen
in die Pakete.
