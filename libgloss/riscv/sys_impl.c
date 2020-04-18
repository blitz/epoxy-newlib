#include "internal_syscall.h"

#include <machine/syscall.h>
#include <stddef.h>
#include <unistd.h>

typedef int cap_t;
typedef unsigned long mword_t;
typedef int syscall_result_t;

/* These are well-known capability indices. */
static const cap_t CAP_EXIT = 0;
static const cap_t CAP_KLOG = 1;

static syscall_result_t invoke(cap_t cap,
			       mword_t arg0,
			       mword_t arg1,
			       mword_t arg2,
			       mword_t arg3)
{
  register mword_t result asm("a0") = (mword_t)(cap);
  register mword_t arg0_ asm("a1") = arg0;
  register mword_t arg1_ asm("a2") = arg1;
  register mword_t arg2_ asm("a3") = arg2;
  register mword_t arg3_ asm("a4") = arg3;

  asm volatile ("ecall" : "+r"(result) : "r"(arg0_), "r"(arg1_), "r"(arg2_), "r"(arg3_) : "memory");

  return (syscall_result_t)(result);
}

/*
  The amount of heap that we can allocate via the brk() "system call".

  TODO: This should be configurable without recompiling the toolchain.
 */
#define USER_HEAP_SIZE (256U << 10)

static void write_string(char const *str)
{
  while (*str != 0) {
    invoke(CAP_KLOG, *(str++), 0, 0, 0);
  }
}

static ssize_t sys_write(int file, void *ptr, size_t len)
{
  switch (file) {
  case STDOUT_FILENO:
  case STDERR_FILENO:

    for (size_t i = 0; i < len; i++) {
      invoke(CAP_KLOG, ((char *)ptr)[i], 0, 0, 0);
    }

    return len;
  default:
    return -1;
  }
}

static long sys_close(int file)
{
  switch (file) {
  case STDOUT_FILENO:
  case STDERR_FILENO:
    return 0;
  default:
    return -1;
  }
}

static long sys_brk(unsigned long new_brk)
{
  static char heap[USER_HEAP_SIZE] __attribute__((aligned(4096)));

  uintptr_t const heap_start = (uintptr_t)(&heap[0]);
  uintptr_t const heap_end = heap_start + sizeof(heap);

  // Query brk
  if (new_brk == 0) {
    return heap_start;
  }

  // Set brk
  if (new_brk >= heap_start && new_brk < heap_end) {
    return new_brk;
  }

  write_string("sys_brk: Out of memory!\n");
  return -1;
}

long
__internal_syscall(long n, int argc, long _a0, long _a1, long _a2, long _a3, long _a4, long _a5)
{
  switch (n) {
  case SYS_write:
    return sys_write(_a0, (void *)(_a1), _a2);

  case SYS_close:
    return sys_close(_a0);

  case SYS_brk:
    return sys_brk(_a0);
    break;

  case SYS_exit:
  case SYS_exit_group:

    // Flush any remaining log messages.
    invoke(CAP_KLOG, '\n', 0, 0, 0);
    invoke(CAP_EXIT, 0, 0, 0, 0);

    // We should not reach this point.
    __builtin_trap();

    break;

  case SYS_fstat:
    // TODO: Implement this to make isatty happy, which checks for st_mode & S_IFCHR.
    break;

  default:
    write_string("__internal_syscall: Unsupported system call!\n");
    break;
  }

  return -1;
}
