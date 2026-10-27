Q2 Fuzzing - Round 3 (dictionary-assisted run)
==============================================

Motivation
----------
In round 1, plain AFL found the 'u' heap overflow and the 'head' out-of-bounds
write, but it did NOT reproduce the "surprise!" bug. That bug is guarded by an
exact string compare:

    else if (strcmp(input, "surprise!\n") == 0) { *(char *)1 = 2; }

Random mutation essentially never produces the exact 10-byte magic string
"surprise!\n", so AFL could not reach the wild write on its own. In round 1 it
was documented as found by source inspection only.

This round uses an AFL dictionary so the fuzzer can get past the magic-string
compare and actually reproduce the crash (dictionaries are AFL's standard
mechanism for exactly this situation).

Setup
-----
Fuzzed the ORIGINAL (unfixed) vulnerable.c, so all bugs are present:
    AFL_HARDEN=1 afl-clang -g -w vulnerable.c -o vulnerable

Dictionary (tokens.dict, included here and in fuzz/):
    surprise="surprise!\x0a"

Command (note the -x dictionary flag):
    afl-fuzz -i inputs -o outputs -x tokens.dict -- ./vulnerable

Same two seeds as rounds 1-2 (inputs/upper, inputs/head).

Result
------
From fuzzer_stats: unique_crashes = 7. The crashes cover all three memory bugs,
and crucially the "surprise!" bug is now AFL-reproduced:

    crashes/id:000000,...                -> bytes exactly "surprise!\n"  (SIGSEGV)
        This is the "surprise!" wild write, found via the dictionary token
        (op field "ext_AO" = dictionary-extra overwrite is what first inserted
        the token; havoc then produced the clean 10-byte crash).
    crashes/id:000002,sig:11,op:ext_AO   -> "head 444..."   (head OOB write)
    crashes/id:000003,sig:11             -> "u 444..."      (u overflow, SIGSEGV)
    crashes/id:000001/4/5/6,sig:06       -> "u ..."  "malloc(): corrupted top
                                            size" (u heap overflow variants)

Reproduce the surprise crash directly:
    ./vulnerable < crashes/id:000000,sig:11,src:000000,op:havoc,rep:64
    # -> Segmentation fault

Conclusion
----------
With the dictionary, AFL reproduces the "surprise!" bug as a real crash input
(crashes/id:000000), not just a source-inspection finding. All three memory
bugs (u overflow, head OOB write, surprise wild write) are now AFL-reproduced.
The fixes for all of them are in the final vulnerable.c; round 2 shows the
fixed binary produces zero crashes.
