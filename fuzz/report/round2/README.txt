Q2 Fuzzing - Round 2 (verification of the fixes)
================================================

Setup
-----
Same procedure as round 1, but against the FIXED vulnerable.c:
    AFL_HARDEN=1 afl-clang -g -w vulnerable.c -o vulnerable   (AFL 2.53b)
    afl-fuzz -i inputs -o outputs -- ./vulnerable
Same two seeds (inputs/upper, inputs/head).


(1) What was checked
--------------------
First, every crashing input from round 1 was replayed against the rebuilt
(fixed) binary:

    for f in round1/crashes/id:*; do ./vulnerable < "$f"; echo $?; done

All 6 now exit cleanly with status 1 (usage / rejected input) instead of
dying on a signal. The "surprise!\n" input also no longer crashes.

Then AFL was run fresh on the fixed binary.


(2) Result
----------
From fuzzer_stats:
    unique_crashes : 0
    unique_hangs   : 0
    execs_done     : 96466
    cycles_done    : 4        (AFL completed multiple full cycles)
    stability      : 100.00%

No crashes and no hangs were found; crashes/ and hangs/ are empty. Because
AFL completed several full cycles over the queue with zero new crashes, the
round-1 bugs (the 'head' out-of-bounds write and the 'u' heap overflow) are
fixed, and the two review-found defects fixed alongside them (the 'surprise!'
wild write and the missing NUL-termination in main) do not reappear.

Valid inputs still behave correctly:
    "u 3 hello world"    -> "HELlo world"
    "head 5 hello world" -> "hello"

See ../round1/README.txt for the full description of each bug and its fix.
The final fixed source is ../vulnerable.c (and fuzz/vulnerable.c).
