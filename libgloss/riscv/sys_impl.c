#include "internal_syscall.h"

__attribute__((weak)) long
__internal_syscall(long n, int argc, long _a0, long _a1, long _a2, long _a3, long _a4, long _a5)
{
  /* Users of this library need to override this function. */
  __builtin_trap();
}
