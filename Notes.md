# Return Address Stack (RAS) v1: BTB-Trained, DEC-Corrected

## Status: tagged working version for STEPLA-2 (5-stage pipeline)

This is the RAS design considered a good fit specifically for a 7-stage
pipeline with synchronous instruction memory. It is not the final word on
RAS design; see "Where this goes next" below for the direction planned
once STEPLA moves to a deeper (e.g. 10-stage) pipeline.

## Motivation

Originally added to fix a specific measured problem: recursive Fibonacci
ran at ~0.75 IPC, with the overwhelming majority of lost cycles coming from
JALR return mispredictions, not data hazards. A single-entry, PC-indexed
BTB row cannot represent a return whose real target changes depending on
which call site invoked it — every non-leaf recursive call and every
function called from more than one place hits this. No amount of warm-up
fixes it; it's a structural mismatch between what a 1-entry table can
represent and what the program needs.

## The core constraint: synchronous instruction memory

Instruction memory read is registered, the fetched instruction's bits
aren't valid until the cycle *after* the read address is issued. This means
at the exact moment IF is choosing next cycle's PC, it cannot know what
instruction currently sits at the fetch PC. Only DEC, two cycles later, has
the actual decoded opcode in hand to classify an instruction as CALL or
RET. This is a hardware fact, not a design choice; any RAS design for this
pipeline has to route around it somehow.

## Design as built

- **DEC is the sole authority on classification.** Only DEC can determine
  "this instruction is a CALL" or "this instruction is a RET," because only
  DEC has decoded bits available, per the sync-RAM constraint above.
- **DEC trains the BTB** with this classification (a tag on the row for
  that PC), rather than DEC directly owning RAS push/pop itself. This
  avoids paying a guaranteed stall cycle on every single call/return, which
  a strict "wait for DEC before doing anything" model would require.
- **IF speculatively pushes/pops** based on the BTB tag from a prior
  visit to that PC. Both operations are gated by `!stall && !flush`, so a
  doomed (to-be-flushed) instruction cannot corrupt RAS state. The gate and
  the trigger are co-located, so there's no window for a push/pop to slip
  through on a path that's about to be squashed.
- **DEC corrects `ptr`** (via `ptrUpdate`) when a speculative pop turns out
  wrong relative to the resolved return address, repairing the pointer
  without needing a full rollback/checkpoint mechanism.

## Measured result

Recursive fib: IPC 0.75 → 0.825, ~220 fewer flush cycles over the measured
run. Confirms the design pays off specifically on repeated call sites
(loops calling the same function, recursion revisiting the same call
depth), which is the dominant pattern in real call-heavy code.

## Known, accepted limitation: one-shot call sites

Because IF can only push/pop/peek based on a BTB tag that DEC trained on a
*previous* visit, the first-ever execution of a given call or return PC has
no tag to act on. Any call site that executes **exactly once** in the whole
program cannot benefit from the RAS at all, its return falls back to
normal unpredicted-branch resolution latency, every time, with no warm-up
possible (there's no second visit to warm up on).

This was confirmed directly: in a nested-call test program, two calls from
`MAIN` to a function, each executing exactly once, never trained a usable
RAS entry, and the corresponding return read a stale/garbage value instead
of the correct address.

**This is treated as an accepted architectural tradeoff, not a bug.** It is
the direct, unavoidable cost of choosing "front-end owns mutation, reactive
training via DEC" over "DEC owns mutation directly" — see below.

## The alternative considered, and why it wasn't chosen for this branch

A DEC-owned commit model (push/pop mutate state at DEC, gated by decode
classification alone, with no dependence on prior BTB training) would fix
the one-shot-call gap entirely, since decode classification is known on the
very first execution of any PC, unlike a BTB tag. The tradeoff: this model
pays a guaranteed one-cycle stall on every call/return, because IF cannot
act until DEC has resolved the classification one cycle later.

For a 5-stage pipeline, where call/return frequency in real workloads is
high and a guaranteed per-call stall compounds quickly, the BTB-trained /
IF-speculative model tested here measured better on the workloads tried so
far (loop- and recursion-heavy call patterns). The DEC-owned model would be
expected to win specifically on code dominated by one-shot calls not yet
measured head-to-head on an equivalent benchmark.

## A hybrid worth testing (not yet implemented on this branch)

Push and pop don't have symmetric urgency. A call's target is known
immediately from the JAL's own immediate decode never needs the BTB to
tell it where to jump. Only the *bookkeeping* (saving the return address)
has a timing choice. A return's target, by contrast, is genuinely unknown
until something supplies it, so the sync-RAM lag only really bites there.

Hybrid: push unconditionally at DEC (gated by `!stall`, no BTB
dependency; costs nothing extra since DEC already has classification
available the cycle after fetch and nothing else was blocked waiting on
push). Leave pop exactly as implemented in this branch (BTB-trained,
IF-speculative, DEC-corrected). Expected effect: one-shot *calls* stop
being destructive (push succeeds on the only execution), while one-shot
*returns* still miss speculative prediction and fall back to normal
resolution latency — a strictly smaller failure surface than the current
branch, for no added stall cost. Planned as a separate branch with
before/after numbers on the same benchmarks (fib, nested-call test) before
merging. Tradeoff: Since only dec knows if an instruction is a CALL or RET
it needs to train the BTB on RET instructions. This will cause flushes on very
first appearance of RET since BTb cannot specualte at this point, it is still 
efficent but corrupts the RAS once again, since the specualtive pop never occoured
DEC need to force PTR down to simulate a pop in RAS.

## Other known limitations, unrelated to the above

- Fixed 8-entry circular buffer, no overflow/underflow tracking or
  depth counter. Recursion deeper than 8 live frames will silently wrap and
  overwrite earlier entries. Max depth exercised in testing so far: 3.
- Circular-buffer index mask is hardcoded to the current depth; if
  `RAS_DEPTH` changes without updating the mask, out-of-bounds reads are
  possible (this produced non-deterministic garbage values during bring-up
  before the actual root cause — BTB-HIT-gated commit — was found and
  fixed).

## Where this goes next

This BTB-trained model is believed to specifically suit a 7-stage pipeline:
the one-cycle sync-RAM classification lag is a small fraction of total
pipeline depth, and the front-end-speculative approach avoids adding a
whole extra stall cycle onto every call/return in a design where cycles are
already scarce.

Moving to a deeper pipeline (e.g. 10-stage) changes this calculus and is
worth revisiting properly rather than assumed to carry over unchanged:

- A deeper front end may have room to decode (or pre-decode / cache a
  call-return tag alongside the instruction in I-cache, the way real cores
  often dodge this exact lag) earlier relative to fetch, shrinking or
  eliminating the one-cycle classification gap this design routes around.
- More pipeline depth also means more in-flight speculative instructions at
  any moment, which raises the stakes on the "no rollback, only pointer
  correction" approach used here — a real checkpoint/restore scheme
  (snapshotting `ptr`, or the top few entries, per speculative branch in
  flight) may be worth the added complexity once misprediction windows get
  longer.
- Worth exploring: a RAS structure with both a synchronous (BTB-trained,
  speculative, fast) and unsynchronized/combinational (immediate,
  DEC-classification-driven) read/write path feeding the same underlying
  stack, so push can take the cheap unsynced path (as in the hybrid above)
  while pop retains a fast speculative path but with a cheaper,
  possibly-partial penalty on misprediction rather than a full flush,
  depending on how deep the eventual pipeline's misprediction window is.

No firm design yet for the 7-stage case flagged here as the next thing to
think through once that pipeline exists to measure against.