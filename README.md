# TheNetNode-CB

## 1.79cb55

Continued by DO2BBC: https://github.com/do2bbc/TheNetNode-CB

This release includes the modern GCC build corrections, TCP/Convers runtime
fixes and regression tests described below. Original CB author: DAA531.

## Linux build with modern GCC

Build with `make tnn` (verified with GCC 12 on Debian 12). Global variables
declared in headers use `extern` and are defined once in their owning source
file, so the default `-fno-common` works without a `-fcommon` workaround.
Debug information is retained for crash analysis. The Linux build's 72 GCC 12
warnings have been corrected in the source, without disabling warnings. Verify
the complete build with:

```
make -B tnn CC="gcc -Werror" HOSTCC="gcc -Werror"
sh tests/gcc-regressions.sh
sh tests/runtime-regressions.sh
sh tests/startup-banner.sh
sh tests/socket-set.sh
sh tests/monitor-hostmode.sh
```

The regression tests use AddressSanitizer and UndefinedBehaviorSanitizer for
buffer handling, profile passwords, percentage and duration formatting,
language loading, Convers sender filters and the IP forwarding flag.
The profile password parser now copies the complete accepted password (up to
80 bytes) instead of copying only the first 16 bytes without reliable
termination. Existing stored profile passwords are not rewritten.
These corrections do not establish that the reported Convers crash is fixed.

The runtime regression suite additionally checks:

- Send errors release each queued frame exactly once and do not interrupt
  processing of other connections.
- Backpressure retains unread frame data; partial writes and temporary socket
  errors retry without losing or duplicating bytes.
- Linux socket readiness checks use an initialized, nonblocking timeout;
  permanent socket errors are logged and mark the connection for disconnect.
- Failed outgoing TCP Convers links restore the previous user/circuit context.
- In password mode, missing or invalid `/NAME` channel arguments produce an
  explicit response instead of dereferencing a null pointer.

These tests run offline with injected I/O outcomes and a local socket pair.
They do not connect to other packet-radio nodes.

The startup banner writes the author/website string separately from the
formatted port/link counts so it does not overflow `hprintf`'s 256-byte
buffer. Its regression test uses stack protection and sanitizers, with and
without Convers enabled.

For distribution toolchains enabling format diagnostics, also verify with
`make -B tnn CC="gcc -Wformat -Wformat-security -Wmaybe-uninitialized -Werror" HOSTCC="gcc -Wformat -Wformat-security -Wmaybe-uninitialized -Werror"`.
Plain language messages are emitted as text rather than format strings.
The internal socket-set reset initializes both its count and descriptor array;
`tests/socket-set.sh` covers reset, duplicate entries and capacity limits.

TheNetNode from Nordlink>&lt;, CB-Version (TNN 1.79cb55)

Version Info:

TheNetNode (Linux) (CB), Version 1.79cb55 (build date generated automatically)
     Copyright by NORD><LINK, free for non-commercial usage.
         See www.nordlink.org for further information.
  This version compiled for 16 Ports, 400 L2-Links and 200 Circuits.
       *** CB-Version by DAA531, continued by DO2BBC ***
       https://github.com/do2bbc/TheNetNode-CB
