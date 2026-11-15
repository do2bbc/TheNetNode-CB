TNN 1.79 - experimental native Win64 CB test package
=====================================================

This package contains native x64 Windows executables. It targets the Windows
Vista API/subsystem level and has been started and AXIP/UDP-tested on Windows
7 x64 in VirtualBox. The external modem/TNC test is not included in this
package and is to be performed separately.

This is experimental test software, not a production-ready release. No
commercial or operational fitness is implied. Windows 7 is obsolete; keep
the test machine isolated and do not expose services to the public Internet.

FIRST START
-----------
1. Extract the complete ZIP to a writable folder.
2. Double-click START-TNN.cmd. No installer or additional runtime is needed.
3. Console password for this offline-only example: TESTONLY
4. Change the node call XX0XX, alias and BOTH passwords in TNN179.PAS before
   enabling any interface. The example passwords are public.
5. To shut down from the console, press Esc, type QUIT, then Enter.
   Plain QUIT without Esc is text for the selected channel.

No ports, peers or radio transmissions are enabled initially. Configurations,
passwords and saved state from the author's running node are NOT included.
All network services are unencrypted legacy protocols. Configure only the
interfaces you need and restrict them with Windows Firewall.

The package includes the node, USEREXE helper programs, documentation,
license, matching source in SOURCE.zip and SHA256SUMS.txt. BUILD.txt identifies
the source commit used to build this package.
