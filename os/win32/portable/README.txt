TNN 1.79 - portable Win32 CB TEST BUILD
====================================

This package targets 32-bit Windows XP (Win32 API 5.1, MSVCRT runtime).
It also targets the 32-bit compatibility environment of XP x64 and newer
Intel/AMD Windows. It is NOT certified on real Windows XP. A Wine test is
not proof that Windows XP or a physical TNC works.
Windows XP is obsolete: keep it on an isolated test network, not exposed
to the Internet. This is beta software, not a production-ready release.
No commercial or operational fitness is implied.

FIRST START
-----------
1. Extract the complete ZIP to a writable folder.
2. Double-click START-TNN.cmd. No installer or additional runtime is needed.
3. Console password for this offline-only example: TESTONLY
4. Change the node call XX0XX, alias and BOTH passwords in TNN179.PAS
   before enabling any interface. The example passwords are public.
5. To shut down from the console, press Esc, type QUIT, then Enter.
   Plain QUIT without Esc is text for the selected channel.

No ports, peers or radio transmissions are enabled initially. Configurations,
passwords and saved state from the author's running node are NOT included.
All network services must be treated as unencrypted legacy protocols.
Enabling a service can listen on all network adapters; use Windows Firewall
and an isolated LAN. Do not forward these ports through your router.

PORTS
-----
The build contains the Windows implementations for:
  KISS, SMACK, RMNC-KISS, token ring, TF (experimental), 6PACK,
  AX25IP/AX25UDP, loopback, Telnet, HTTP, IP-Convers and IRC.
Being compiled does NOT mean tested with the corresponding hardware.
In particular 6PACK and serial hardware need tests on real Windows.

Linux kernel AX.25 (KAX25/KAX25KJD), Linux network interfaces, IPX and
Vanessa/SCC ISA card drivers are NOT provided by this portable package.
USB serial adapters require a separate Windows-XP-compatible vendor driver.
There is no universal driver for every radio or sound-card modem.

EXAMPLES (not active by default)
-------------------------------
Add groups to tnn.ini, restart TNN, then configure the matching TNN ports.
Use unused port numbers. Keep the groups in device/kisstype/port order.

Serial KISS on COM1, 9600 baud:
  device COM1
  speed 9600
  kisstype 0
  port 0
Other serial types: SMACK=1, RMNC=2, token ring=3, TF=6, 6PACK=12.
COM10 and above require the Windows device form \\.\COM10.

AX25IP/UDP:
  device AX25IP
  kisstype 8
  port 1
Configure your own UDP port and peers in ax25ip.cfg, using the TNN manual.
No AX25IP peer file is supplied to avoid accidental connections.

Telnet:
  device 2323
  kisstype 22
  port 2
HTTP: kisstype 23. IP-Convers: kisstype 24. IRC: kisstype 25.
For these TCP interfaces "device" is the listening TCP port.
The HTTP document root is initially empty.
Missing TNN179.STA/GRAPH.STA warnings on the first start are normal:
there are deliberately no saved statistics from another node.

Ports also need suitable PORT parameters/mode in TNN179.TNB or at the
console. ATTACH and PORT commands are described in the included TNN docs.
Settings may be saved by TNN on exit; back up your own configuration.

FILES / BUILD
-------------
tnn32.exe: node; USEREXE contains help, msg, msy and top utilities.
The old pfhadd target actually contains a direct parallel-port I/O program
which is not a Windows application and is deliberately not included.
License: ALAS.txt. Complete matching source and build instructions are
included in SOURCE.zip. Do not redistribute without the license/source.
The version retains its upstream afu61 lineage; "(CB)" identifies disabled
amateur-radio callsign validation, not a separate upstream patch level.

LOCAL DIAGNOSTICS
-----------------
tnn-runtime.log records UTC start time, PID, uptime, available physical
memory once per minute, and clean/error exits. The start record's address
is the executable load address. It does NOT prove that every port works.
tnn-crash.log records unhandled Windows exceptions (code and address) or
fatal C signals (signal number; no address available).
Both files are in the start directory alongside START-TNN.cmd when using
the launcher. Each restart keeps one ".previous.log" copy. Save those
files BEFORE restarting repeatedly, otherwise older evidence is replaced.
The runtime log stops at approximately 1 MiB per run and prints a warning;
the separate crash file remains open. On unwritable folders, errors are
shown on the console rather than silently pretending logging works.

No commands, passwords, radio text, network payloads or memory dumps are
intentionally recorded in THESE files. Legacy debug.log, STARTUP.LOG and
other node files are separate and have not been made content-free.
For support send the two diagnostic logs (plus previous copies if useful)
and the package's SHA256SUMS.txt. Do not send TNN179.PAS or a complete node
folder: those may contain passwords or private traffic.

There is no automatic upload, notification or restart. Hard power loss,
forced process termination, stack exhaustion or severe memory corruption
may prevent a crash record. A missing clean exit / stopped heartbeat is
only a clue, not proof of a particular crash. Logs cannot detect every hang.
