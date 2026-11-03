#ifndef TNN_WIN32_WINCLUDE_H
#define TNN_WIN32_WINCLUDE_H

/* Keep Win32's unsigned types separate from TNN's signed protocol types. */
#define BYTE WIN32_BYTE
#define WORD WIN32_WORD
#define BOOLEAN WIN32_BOOLEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef BYTE
#undef WORD
#undef BOOLEAN
#undef FALSE
#undef TRUE
#undef ERROR

#include <io.h>
#include <conio.h>
#ifdef _MSC_VER
/* Wird nur im VC benoetig. */
#include <direct.h>
#endif /* MSC_VER */
#include <share.h>
#include <stdarg.h>
#include <process.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include "sys/dirent32.h"
#include "sys/fnmatch.h"
#include "sys/strings.h"

#pragma comment(lib, "ws2_32.lib" )

typedef void *pthread_t;
#ifdef _MSC_VER
/* Wird nur im VC benoetig. */
typedef void *pid_t;
#endif /* MSC_VER */

#define NAME_MAX 260

/* End of os/win32/winclude.h */

#endif
