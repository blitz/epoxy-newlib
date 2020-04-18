#include "internal_syscall.h"

static long __dead_syscall(long n, long _a0, long _a1, long _a2, long _a3, long _a4, long _a5)
{
    /* Users of this library need to override this function. */
  __builtin_trap();
}

__attribute__((weak)) long (* const __syscall_impl)(long, long, long, long, long, long, long) = &__dead_syscall;
