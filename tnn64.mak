# Experimental 64-bit Windows build. Build from a separate source copy so
# the x86 and x64 object files cannot be mixed.
PREFIX ?= x86_64-w64-mingw32-
WIN32_API_VERSION ?= 0x0600
WIN32_SUBSYSTEM_VERSION ?= 6.00
WIN32_BINDIR ?= os/win32/tnn64
WIN32_EXE ?= tnn64.exe

include tnn32.mak
