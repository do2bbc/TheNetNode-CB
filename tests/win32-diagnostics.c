#include "tnn.h"
#include "diagnostics.h"

int main(int argc, char **argv)
{
  win32_diagnostics_init();
  if (argc != 2)
    return 2;
  if (!strcmp(argv[1], "exception"))
    RaiseException(0xe042544e, EXCEPTION_NONCONTINUABLE, 0, NULL);
  else if (!strcmp(argv[1], "signal"))
    raise(SIGSEGV);
  else if (!strcmp(argv[1], "heartbeat")) {
    Sleep(61000);
    win32_diagnostics_tick();
    win32_diagnostics_exit(0);
  } else if (!strcmp(argv[1], "clean"))
    win32_diagnostics_exit(0);
  else if (!strcmp(argv[1], "error"))
    win32_diagnostics_exit(7);
  else if (strcmp(argv[1], "unclean"))
    return 2;
  return 0;
}
