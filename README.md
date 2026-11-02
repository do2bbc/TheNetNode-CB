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

## Optionale Tests und weitere Dokumentation

Diese Regressionstests laufen offline und verbinden sich nicht mit anderen Nodes:

```bash
sh tests/gcc-regressions.sh
sh tests/runtime-regressions.sh
sh tests/startup-banner.sh
sh tests/socket-set.sh
sh tests/monitor-hostmode.sh
```

Weitere Unterlagen: [doc](doc/), [Versionshistorie](history/) und
[Lizenzbedingungen](alas.txt).
