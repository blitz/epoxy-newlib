#include "internal_syscall.h"

#include <machine/syscall.h>
#include <stddef.h>
#include <unistd.h>

typedef uintptr_t mword_t;

// Supervisor Binary Interface v0.2
// https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc

enum {
  SBI_EXT_LEGACY_PUTCHAR = 1u,
  SBI_EXT_LEGACY_GETCHAR = 2u,
  SBI_EXT_LEGACY_SHUTDOWN = 8u,
};

enum {
  // Nothing here yet.
  SBI_FUN_NONE = 0u,
};

static int sbi_ecall1(unsigned ext_id, unsigned fun_id, mword_t param0)
{
  register mword_t _param0 asm("a0") = param0;
  register mword_t _param1 asm("a1") = 0;
  register mword_t _fun_id asm("a6") = fun_id;
  register mword_t _ext_id asm("a7") = ext_id;

  asm volatile("ecall"
               : "+r"(_param0), "+r"(_param1), "+r"(_fun_id), "+r"(_ext_id)
               :
               : "memory", "ra", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a2", "a3", "a4", "a5",
                 "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11");

  // TODO Is the SBI backend guaranteed to preserve register content?
  // Without clobbering the s registers, we get corruption.
  return _param0;
}

// Well-known SBI interfaces

static void sbi_putc(char c)
{
  sbi_ecall1(SBI_EXT_LEGACY_PUTCHAR, SBI_FUN_NONE, (uint8_t)c);
}

static int sbi_getc(void)
{
  return sbi_ecall1(SBI_EXT_LEGACY_GETCHAR, SBI_FUN_NONE, 0);
}

static void sbi_shutdown(void)
{
  sbi_ecall1(SBI_EXT_LEGACY_SHUTDOWN, SBI_FUN_NONE, 0);
}

/*
  The amount of heap that we can allocate via the brk() "system call".

  TODO: This should be configurable without recompiling the toolchain.
 */
#define USER_HEAP_SIZE (256U << 10)

static void write_string(char const *str)
{
  while (*str != 0) {
    sbi_putc(*(str++));
  }
}

static ssize_t sys_write(int file, void *ptr, size_t len)
{
  switch (file) {
  case STDOUT_FILENO:
  case STDERR_FILENO:

    for (size_t i = 0; i < len; i++) {
      sbi_putc(((const char *)ptr)[i]);
    }

    return len;
  default:
    return -1;
  }
}

static ssize_t sys_read(int file, void *ptr, size_t len)
{
  switch (file) {
  case STDIN_FILENO:
  case STDERR_FILENO:

    if (len > 0) {
      int c;

      do {
	c = sbi_getc();
      } while (c < 0);

      *(char *)ptr = c;

      return 1;
    } else {
      return -1;
    }

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

  case SYS_read:
    return sys_read(_a0, (void *)(_a1), _a2);

  case SYS_close:
    return sys_close(_a0);

  case SYS_brk:
    return sys_brk(_a0);
    break;

  case SYS_exit:
  case SYS_exit_group:

    sbi_shutdown();

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
