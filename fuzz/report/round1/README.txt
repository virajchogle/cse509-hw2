Q2 Fuzzing - Round 1
====================

Setup
-----
Target built per the handout:
    AFL_HARDEN=1 afl-clang -g -w vulnerable.c -o vulnerable   (AFL 2.53b)

Seeds (inputs/), one per input form the program accepts:
    inputs/upper : "u 3 hello world\n"      (the 'u' / upper-case command)
    inputs/head  : "head 5 hello world\n"   (the 'head' command)

Fuzzer command:
    afl-fuzz -i inputs -o outputs -- ./vulnerable

Result: 6 unique crashes (see fuzzer_stats; crashes/ holds the inputs).
Signals: sig:11 = SIGSEGV (bad memory access),
         sig:06 = SIGABRT (glibc heap-corruption guard firing).


(1) How the bugs were identified, and which bugs were found
-----------------------------------------------------------
Each crashing input in crashes/ was fed back into the program
(`./vulnerable < crashes/<file>`) and the signal / stderr was examined.
gdb could not ptrace under the emulated build environment, so bugs were
localized from the crashing input bytes + the glibc diagnostic message +
source reading. The 6 crashes reduce to TWO underlying bugs, both caused by
using the attacker-controlled length N without validating it.

BUG A - 'head' command: out-of-bounds write via rest[len]
    Crash: id:000001  (sig:11 / SIGSEGV)
    Input: starts "head 442444...P4\0..." -> a large N.
    In process():
        len = strtol(input + 4, &rest, 10);
        rest += 1;
        rest[len] = '\0';        // <-- writes at an attacker-chosen offset
    N is never checked. A large N writes the NUL far past the input buffer
    (arbitrary/out-of-bounds write) -> SIGSEGV. A negative N writes before
    the buffer. This is the most serious bug (a controlled write primitive).

BUG B - 'u' command: heap buffer overflow via unchecked / negative len
    Crashes: id:000000, id:000002, id:000003, id:000004, id:000005
             (sig:11 SIGSEGV and sig:06 "malloc(): corrupted top size" /
              "malloc assertion failure")
    Representative inputs:
        id:000004  "u -2u 333..."              -> explicit negative N (-2)
        id:000000  "u 3333333333333333333333head 33"
                    -> 23-digit N; strtol() overflows to LONG_MAX, and the
                       cast to int wraps to -1 (a negative len)
        id:000002/3/5 -> variations that drive the write past the heap chunk,
                       tripping glibc's heap-corruption abort.
    In process():
        len = strtol(input + 2, &rest, 10);   // can be negative/overflowed
        out = malloc(len + strlen(input));     // size computed from len
        if (len > (int)strlen(input)) ...      // only an UPPER-bound check
        for (int i = 0; i != len; i++)         // if len<0 this never ends
            out[i] = ...;                      //   -> runs off the heap chunk
        out[len] = 0;
        strcat(out, rest + len);               // further OOB with bad len
    A negative len: (a) the loop condition `i != len` is never satisfied for
    a negative len (i only ever increases), so it writes out[0], out[1], ...
    far past the allocation; (b) malloc(len + strlen) with negative len makes
    the buffer too small / size wraps. Both corrupt the heap -> SIGSEGV or the
    glibc abort.

Two more real defects (found by source review; AFL did not happen to hit the
exact bytes for them within this round's run - both are fixed in round 2):

BUG C - 'surprise!' easter egg: wild write
        `else if (strcmp(input,"surprise!\n")==0) { *(char*)1 = 2; }`
        writes to address 1 -> guaranteed SIGSEGV.
        Reproduce: printf 'surprise!\n' | ./vulnerable   (dies on SIGSEGV)
        Plain AFL cannot guess the exact magic string "surprise!\n", so it was
        first found by inspection here. See ../round3, where an AFL dictionary
        lets the fuzzer reproduce this crash directly (round3/crashes/id:000000).

BUG D - missing NUL termination in main()
        `read(STDIN_FILENO, input, INPUTSIZE)` can fill all 100 bytes with no
        terminator (input[] is only zero-initialized before the read). The
        later strlen()/strcmp()/strtol()/printf("%s") then read past the
        buffer. Latent out-of-bounds read.


(2) How each bug was fixed, and why the fix works
-------------------------------------------------
All fixes are in the round-2 copy of vulnerable.c. Summary:

BUG A (head rest[len]): after parsing N, compute avail = strlen(rest) and
    reject `len < 0 || (size_t)len > avail`. Then rest[len] is guaranteed to
    land within the string (index 0..avail, i.e. at most the existing NUL),
    so the write is always in bounds.

BUG B (u heap overflow): use a `long len`, and before allocating/looping,
    reject `len < 0 || (size_t)len > avail` where avail = strlen(rest). This
    removes negative/overflowed lengths, so the `i != len` loop always
    terminates within [0,avail). Allocate `malloc(avail + 1)` - exactly big
    enough for the uppercased prefix (len) + the untouched remainder
    (avail-len) + the NUL that strcat writes - so neither the loop, out[len]=0,
    nor strcat() can overflow.

BUG C (easter egg): removed the `*(char*)1 = 2;` wild write; "surprise!\n" is
    now treated as an unknown command (returns 1, prints usage). No crash.

BUG D (NUL termination): read at most INPUTSIZE-1 bytes and set input[n]='\0'
    using the actual byte count, guaranteeing the buffer is always a valid C
    string regardless of input length.

Also hardened: both commands now verify strtol() actually parsed a digit
(`rest == input+offset` means no number) and only skip the separator space
when one is actually present, so a truncated command like "u 5" no longer
advances `rest` past the end of the buffer.

Valid behavior is preserved:
    "u 3 hello world"   -> "HELlo world"
    "head 5 hello world"-> "hello"
