Q2 Fuzzing - Round 1
====================

Setup
-----
    AFL_HARDEN=1 afl-clang -g -w vulnerable.c -o vulnerable     (AFL 2.53b)
    afl-fuzz -i inputs -o outputs -- ./vulnerable

Result: 6 unique crashes (see fuzzer_stats; crash inputs in crashes/).
    sig:11 = SIGSEGV,  sig:06 = SIGABRT (glibc heap-corruption guard).


Bugs found (by AFL) and how they were identified
-------------------------------------------------
Each crashing input was replayed into the program and the signal / stderr
examined. The 6 crashes reduce to two bugs, both caused by using the
attacker-controlled length N without validating it.

BUG 1 - 'head' command: out-of-bounds write via rest[len]
    Crash: id:000001 (SIGSEGV). Input begins "head 442444...".
        len = strtol(input + 4, &rest, 10);
        rest += 1;
        rest[len] = '\0';      // writes at an attacker-chosen offset
    N is never checked, so a large N writes the NUL far past the input buffer
    (a controlled out-of-bounds write) -> SIGSEGV.

BUG 2 - 'u' command: heap overflow via unchecked / negative len
    Crashes: id:000000, id:000002, id:000003, id:000004, id:000005
             (SIGSEGV and "malloc(): corrupted top size").
        id:000004  "u -2u 333..."              -> explicit negative N
        id:000000  "u 3333333333333333333333head 33"
                     -> 23-digit N; strtol() saturates to LONG_MAX and the
                        cast wraps to a negative len
        out = malloc(len + strlen(input));
        for (int i = 0; i != len; i++) out[i] = ...;   // len<0 never ends
        strcat(out, rest + len);
    A negative len makes the `i != len` loop write past the allocation (i only
    increases), and sizes the buffer wrong, corrupting the heap -> crash.


Fixes
-----
BUG 1: after parsing N, compute avail = strlen(rest) and reject
    `len < 0 || (size_t)len > avail`. Then rest[len] always lands within the
    string (index 0..avail), so the write is in bounds.

BUG 2: use a `long len`; before allocating/looping reject
    `len < 0 || (size_t)len > avail`. This removes negative/overflowed lengths,
    so the loop always terminates in [0,avail). Allocate `malloc(avail + 1)`,
    exactly enough for the uppercased prefix (len) + remainder (avail-len) +
    NUL, so neither the loop, out[len]=0, nor strcat() can overflow.

Also hardened: both commands verify strtol() parsed a digit, and only skip the
separator space when one is present, so a truncated command ("u 5") cannot
advance rest past the end of the buffer.

After these fixes the 'u' and 'head' crashes no longer reproduce. The next
bug (the "surprise!" branch) needs a dictionary to reach - see round2.
