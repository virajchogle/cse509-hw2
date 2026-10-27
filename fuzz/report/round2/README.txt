Q2 Fuzzing - Round 2 (dictionary-assisted)
==========================================

Setup
-----
Fuzzed the round-1-fixed program (the 'u' and 'head' bugs are already patched,
so the only remaining crash is the "surprise!" branch):

    else if (strcmp(input, "surprise!\n") == 0) { *(char *)1 = 2; }

That branch is guarded by an exact string compare, so plain random mutation
essentially never produces the 10-byte magic string "surprise!\n". AFL's
dictionary feature is the standard way past a magic-string compare, so this
round supplies the token via -x.

Dictionary (tokens.dict, included here):
    surprise="surprise!\x0a"

Command:
    AFL_HARDEN=1 afl-clang -g -w vulnerable.c -o vulnerable
    afl-fuzz -i inputs -o outputs -x tokens.dict -- ./vulnerable


Bug found (by AFL) and how it was identified
---------------------------------------------
fuzzer_stats: unique_crashes = 1 (6 cycles completed).

    crashes/id:000000,...  -> bytes exactly "surprise!\n"  (SIGSEGV)

Replaying it crashes immediately:
    ./vulnerable < crashes/id:000000,sig:11,src:000000,op:havoc,rep:32
    # -> Segmentation fault

The input is the "surprise!" easter egg. Its body dereferences the literal
address 1 (`*(char *)1 = 2;`), a wild write that always faults with SIGSEGV.
Because the 'u' and 'head' bugs were fixed before this round, this is the only
crash AFL reports here.


Fix
---
Removed the easter-egg branch. "surprise!\n" is now handled like any other
unrecognized command (returns 1 and prints the usage message); it no longer
performs the wild write, so it cannot crash. This does not change the
program's intended functionality (the two documented commands, 'u' and 'head').

After this fix, re-running the fuzzer on the final vulnerable.c produces no
crashes.
