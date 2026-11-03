#include "tnn.h"
#include "diagnostics.h"

static HANDLE runtime_file = INVALID_HANDLE_VALUE;
static HANDLE crash_file = INVALID_HANDLE_VALUE;
static DWORD started, sampled;
static int stopped;

static void write_record(HANDLE file, const char *event, DWORD code,
                         const void *address)
{
  SYSTEMTIME now;
  MEMORYSTATUS memory;
  char line[384];
  DWORD written;
  int length;
  if (file == INVALID_HANDLE_VALUE)
    return;
  GetSystemTime(&now);
  memory.dwLength = sizeof(memory);
  GlobalMemoryStatus(&memory);
  length = _snprintf(line, sizeof(line),
      "%04u-%02u-%02uT%02u:%02u:%02uZ event=%s pid=%lu uptime_ms=%lu "
      "code=0x%08lx address=%p available_physical_bytes=%lu\r\n",
      now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
      event, (unsigned long)GetCurrentProcessId(),
      (unsigned long)(GetTickCount() - started), (unsigned long)code,
      address, (unsigned long)memory.dwAvailPhys);
  if (length <= 0 || length >= (int)sizeof(line) ||
      !WriteFile(file, line, length, &written, NULL) ||
      written != (DWORD)length || !FlushFileBuffers(file)) {
    fputs("ERROR: cannot write/flush TNN diagnostic log.\n", stderr);
  }
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *exception)
{
  write_record(crash_file, "UNHANDLED_EXCEPTION",
               exception->ExceptionRecord->ExceptionCode,
               exception->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

static void fatal_signal(int number)
{
  write_record(crash_file, "FATAL_C_SIGNAL", (DWORD)number, NULL);
  TerminateProcess(GetCurrentProcess(), 128 + number);
}

static void process_exit(void)
{
  if (!stopped)
    write_record(runtime_file, "EXIT_WITHOUT_CLEAN_SHUTDOWN", 0, NULL);
}

void win32_diagnostics_init(void)
{
  started = sampled = GetTickCount();
  /* Always rotate at startup so the previous run survives a restart. */
  if (GetFileAttributesA("tnn-runtime.log") != INVALID_FILE_ATTRIBUTES &&
      !MoveFileExA("tnn-runtime.log", "tnn-runtime.previous.log",
                   MOVEFILE_REPLACE_EXISTING))
    fputs("ERROR: cannot rotate tnn-runtime.log.\n", stderr);
  if (GetFileAttributesA("tnn-crash.log") != INVALID_FILE_ATTRIBUTES &&
      !MoveFileExA("tnn-crash.log", "tnn-crash.previous.log",
                   MOVEFILE_REPLACE_EXISTING))
    fputs("ERROR: cannot rotate tnn-crash.log.\n", stderr);
  runtime_file = CreateFileA("tnn-runtime.log", FILE_APPEND_DATA,
      FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  crash_file = CreateFileA("tnn-crash.log", FILE_APPEND_DATA,
      FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (runtime_file == INVALID_HANDLE_VALUE || crash_file == INVALID_HANDLE_VALUE)
    fputs("ERROR: diagnostic files unavailable; use a writable start folder.\n",
          stderr);
  SetUnhandledExceptionFilter(crash_filter);
  signal(SIGSEGV, fatal_signal);
  signal(SIGILL, fatal_signal);
  signal(SIGFPE, fatal_signal);
  signal(SIGABRT, fatal_signal);
  atexit(process_exit);
  write_record(runtime_file, "START_WIN32_XP_CB_TEST", 0,
               GetModuleHandleA(NULL));
}

void win32_diagnostics_tick(void)
{
  DWORD now = GetTickCount();
  if ((DWORD)(now - sampled) < 60000)
    return;
  sampled = now;
  /* Roughly three days per file; stop writing rather than growing forever. */
  if (runtime_file != INVALID_HANDLE_VALUE &&
      GetFileSize(runtime_file, NULL) >= 1024 * 1024) {
    write_record(runtime_file, "LOG_LIMIT_REACHED_RESTART_TO_ROTATE", 0, NULL);
    CloseHandle(runtime_file);
    runtime_file = INVALID_HANDLE_VALUE;
    fputs("WARNING: TNN runtime log reached 1 MiB; heartbeat logging stopped.\n",
          stderr);
    return;
  }
  write_record(runtime_file, "HEARTBEAT", 0, NULL);
}

void win32_diagnostics_exit(int code)
{
  stopped = 1;
  write_record(runtime_file, code ? "ERROR_EXIT" : "CLEAN_EXIT",
               (DWORD)code, NULL);
}
