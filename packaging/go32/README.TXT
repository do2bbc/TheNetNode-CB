TheNetNode-CB GO32 - DOS test package

Requires a 386 or later CPU and DOS with a DPMI provider. CWSDPMI.EXE is
included for systems without one. This package has been smoke-tested in
DOSBox, not on physical DOS hardware or radio interfaces.

Extract the package to C:\TNN. From C:\, set the path and start the node:

  SET PATH=C:\TNN;%PATH%
  C:\TNN\TNN.EXE

On first startup TNN creates TNN179.PAS with public default passwords and
the placeholder call XX0XX. Stop TNN before changing the call, alias and
passwords. The NET/ROM sysop password must contain exactly 80 characters.
Do not expose a node with the default credentials.

GO32 does not provide the Linux/Win32 AXIP, native TCP-KISS, HTTPD or
IP-Convers interfaces. Serial and radio hardware have not been tested.
Keep existing configuration and data when updating an installation.

CWSDPMI.EXE is Copyright (C) 1995-2010 Charles W Sandmann. It is included
unmodified; see CWSDPMI.DOC for its redistribution conditions. Users have
the right to receive its source and binary updates, available at:
https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7s.zip
https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7b.zip
