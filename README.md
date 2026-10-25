# CSE 509 — Computer System Security, Assignment 2

Coursework for CSE 509. Two parts: a Return-Oriented Programming exploit and an
AFL fuzzing campaign.

> ⚠️ Educational use. The exploit targets a deliberately vulnerable, setuid
> program in a sandboxed course VM. The gadget/string addresses in `xploit4.c`
> are build-specific — regenerate them on your own build (see below).

## Q1 — ROP (`xploits/`, `targets/`)

`targets/target4.c` has a classic stack overflow: `bar()` does
`memcpy(buf, input, len)` into a 256-byte stack buffer with no bounds check,
while `main()` reads up to 4096 bytes. The binary is statically linked (musl),
non-PIE, built with no stack canary and a **non-executable stack**, so the
exploit uses ROP instead of stack shellcode.

`xploits/xploit4.c` builds a chain that performs
`execve("/bin/sh", NULL, NULL)` (`rax=59`, `rdi="/bin/sh"`, `rsi=0`, `rdx=0`).
Since `target4` is setuid-root and `main()` calls `setuid(0)`, this yields a
root shell.

- Overflow offset to the saved return address: **264 bytes** (256 `buf` + 8 saved `rbp`).
- `rdx` is zeroed with a `xor %edx,%edx; div %rbp` gadget (this build has no
  `pop rdx`/`pop rcx`), with `rax=0, rbp=1` so the divide can't fault.

Build & run (on the course VM):

```sh
cd targets && sudo make install     # builds + installs /tmp/target4 (setuid)
cd ../xploits && make && ./xploit4   # -> root shell
```

The gadget and `/bin/sh` addresses are documented in the header of
`xploits/xploit4.c` together with the exact `find_gadgets.py` / `readelf`
commands to regenerate them for your build.

## Q2 — Fuzzing (`fuzz/`)

`fuzz/vulnerable.c` is a small stdin text utility (`u <N> <string>`,
`head <N> <string>`) fuzzed with **AFL 2.53b**. Bugs found and fixed:

1. **`head` out-of-bounds write** — `rest[len] = '\0'` with an unchecked length.
2. **`u` heap overflow** — negative / overflowed length drives the copy loop and
   `strcat` off the heap buffer.
3. **`surprise!` easter egg** — a wild write `*(char*)1 = 2` (removed).
4. **Missing NUL-termination** in `main()` — a full 100-byte read left the
   buffer unterminated (out-of-bounds reads).

`fuzz/vulnerable.c` here is the **fixed** version. The fuzzing evidence
(per-round `fuzzer_stats`, `crashes/`, `hangs/`, and `README.txt` write-ups)
is under `fuzz/report/`.

Build & fuzz:

```sh
cd fuzz && make                      # AFL_HARDEN=1 afl-clang ...
afl-fuzz -i inputs -o outputs -- ./vulnerable
```

## Layout

```
find_gadgets.py        # provided: ROP gadget finder
targets/               # target4.c + Makefile (vulnerable setuid program)
xploits/               # xploit4.c (the ROP exploit) + helpers
fuzz/                  # vulnerable.c (fixed), seeds, and report/
fuzz/report/round1/    # original binary: 6 crashes + analysis
fuzz/report/round2/    # fixed binary: 0 crashes (verification)
```
