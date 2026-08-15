"""
HYPERION-MP pipeline trace reconstructor.

Reads a Logisim RAM image (exported via right-click -> "Save Image...")
that was logged with one row per cycle, each row = {DEC_instr[15:0], Stall[1], Flush[1]}
packed into a 24-bit word (6 hex digits), and reconstructs what was sitting
in all 5 pipeline stages (IF, DEC, EXEC, MEM, WB) on every cycle.

Usage: python3 trace_reconstruct.py your_exported_trace.txt
"""

import sys

# ---------------- disassembler (mirrors gen_rom2.py's ISA table) ----------------

R_NAMES  = {0:"NOP",1:"ADD",2:"MUL",3:"XOR",4:"AND",5:"OR",6:"SLT",7:"RES"}
I_NAMES  = {0:"RES",1:"ADDI",2:"MULI",3:"XORI",4:"ANDI",5:"LOAD",6:"SLTI",7:"JALR"}
SB_NAMES = {0:"BGEU",1:"BNE",2:"BLTU",3:"BGE",4:"BEQ",5:"BLT",6:"STORE",7:"RES"}
J_NAMES  = {0:"JAL",1:"LUI",2:"MOVI",3:"PUSH",4:"POP",5:"RES",6:"RES",7:"RES"}

def get_producer_rd(word):
    """Returns the destination register this instruction writes via its normal
    Rd field, or None if it doesn't write one this way. Best-effort: PUSH/POP's
    dedicated SP write port isn't representable here and is intentionally excluded."""
    if word == 0:
        return None
    tag   = (word >> 14) & 0b11
    subop = (word >> 11) & 0b111
    rd    = (word >> 8) & 0b111
    if tag == 0b00 and subop in (1,2,3,4,5,6):      # R-type ALU ops
        return rd
    if tag == 0b01 and subop in (1,2,3,4,5,6,7):    # I-type ALU/LOAD/JALR
        return rd
    if tag == 0b11 and subop in (0,1,2,4):           # JAL, LUI, MOVI, POP
        return rd
    return None

def get_sp_producer(word):
    """PUSH and POP both write SP via the dedicated port (not the normal Rd
    field). Returns 7 (SP's register number) if this instruction produces a
    new SP value, else None."""
    if word == 0:
        return None
    tag   = (word >> 14) & 0b11
    subop = (word >> 11) & 0b111
    if tag == 0b11 and subop in (3, 4):   # PUSH, POP
        return 7
    return None

def get_consumer_slots(word):
    """Returns a list of (register_number, slot_name) pairs for this
    instruction's source registers, excluding R0. Preserves BOTH slots even
    when the same register fills Rs1 and Rs2 (e.g. MUL R4,R1,R1). PUSH/POP
    get special handling: both always depend on SP (register 7) as their
    address base, and PUSH additionally reads its Rd field as a diverted
    source (the value being pushed)."""
    if word == 0:
        return []
    tag = (word >> 14) & 0b11
    rs1 = (word >> 5) & 0b111
    rs2 = (word >> 2) & 0b111
    slots = []
    if tag == 0b00:            # R-type: Rs1, Rs2
        if rs1 != 0: slots.append((rs1, "Rs1"))
        if rs2 != 0: slots.append((rs2, "Rs2"))
    elif tag == 0b01:          # I-type: Rs1 only
        if rs1 != 0: slots.append((rs1, "Rs1"))
    elif tag == 0b10:          # SB-type: Rs1, Rs2
        if rs1 != 0: slots.append((rs1, "Rs1"))
        if rs2 != 0: slots.append((rs2, "Rs2"))
    elif tag == 0b11:          # J-type: PUSH/POP depend on SP
        subop = (word >> 11) & 0b111
        rd = (word >> 8) & 0b111
        if subop in (3, 4):    # PUSH, POP
            slots.append((7, "SP"))
        if subop == 3 and rd != 0:   # PUSH also reads Rd as diverted push value
            slots.append((rd, "Rs2(push-val)"))
    return slots

def get_consumer_regs(word):
    """Returns the set of source register numbers this instruction reads,
    excluding R0 (always zero, never meaningfully forwarded)."""
    return {r for r, _ in get_consumer_slots(word)}

def forwarding_note(exec_word, mem_word, wb_word):
    """Best-effort inference: does the EXEC-stage instruction's source register
    match a producer sitting in EXEC/MEM or MEM/WB this cycle? EXEC/MEM takes
    priority over MEM/WB, matching the priority-encoder forwarding design.
    Shows the full path: source register, producer stage, consumer mnemonic,
    and which operand slot (Rs1/Rs2/SP) it lands in -- checks BOTH slots even
    when the same register fills both (e.g. MUL R4,R1,R1). SP slots are
    checked against the dedicated SP write port, not the normal Rd producer."""
    if exec_word is BUBBLE or exec_word == 0:
        return ""
    slots = get_consumer_slots(exec_word)
    if not slots:
        return ""
    exec_name = disasm(exec_word).split()[0]  # just the mnemonic, keep it short
    notes = []
    mem_rd = get_producer_rd(mem_word) if mem_word not in (BUBBLE, None) else None
    wb_rd  = get_producer_rd(wb_word) if wb_word not in (BUBBLE, None) else None
    mem_sp = get_sp_producer(mem_word) if mem_word not in (BUBBLE, None) else None
    wb_sp  = get_sp_producer(wb_word) if wb_word not in (BUBBLE, None) else None
    for r, slot in slots:
        is_sp_slot = (slot == "SP")
        mem_p = mem_sp if is_sp_slot else mem_rd
        wb_p  = wb_sp if is_sp_slot else wb_rd
        tag_str = " [SP]" if is_sp_slot else ""
        if mem_p is not None and mem_p == r:
            notes.append(f"EXEC/MEM.R{r}->{exec_name}.{slot}{tag_str}")
        elif wb_p is not None and wb_p == r:
            notes.append(f"MEM/WB.R{r}->{exec_name}.{slot}{tag_str}")
    return ",".join(notes)

def resolves_in_dec(word):
    """True only for instructions with a real DEC-stage comparator/adder:
    SB-type branches and JALR. PUSH/POP consume SP in EXEC, not DEC, so they
    should never get a 'into DEC comparator' forwarding annotation."""
    if word == 0:
        return False
    tag = (word >> 14) & 0b11
    subop = (word >> 11) & 0b111
    if tag == 0b10:
        return True
    if tag == 0b01 and subop == 0b111:  # JALR
        return True
    return False
    if val & (1 << (bits - 1)):
        val -= (1 << bits)
    return val

def sign_extend(val, bits):
    if val & (1 << (bits - 1)):
        val -= (1 << bits)
    return val

def disasm(word):
    if word == 0:
        return "NOP"
    tag   = (word >> 14) & 0b11
    subop = (word >> 11) & 0b111
    if tag == 0b00:  # R-type
        rd  = (word >> 8) & 0b111
        rs1 = (word >> 5) & 0b111
        rs2 = (word >> 2) & 0b111
        return f"{R_NAMES[subop]} R{rd},R{rs1},R{rs2}"
    elif tag == 0b01:  # I-type
        rd  = (word >> 8) & 0b111
        rs1 = (word >> 5) & 0b111
        imm = sign_extend(word & 0b11111, 5)
        return f"{I_NAMES[subop]} R{rd},R{rs1},{imm}"
    elif tag == 0b10:  # SB-type
        imm_hi = (word >> 8) & 0b111
        rs1    = (word >> 5) & 0b111
        rs2    = (word >> 2) & 0b111
        imm_lo = word & 0b11
        imm = sign_extend((imm_hi << 2) | imm_lo, 5)
        return f"{SB_NAMES[subop]} R{rs1},R{rs2},{imm}"
    else:  # J-type
        rd  = (word >> 8) & 0b111
        imm = sign_extend(word & 0xFF, 8)
        return f"{J_NAMES[subop]} R{rd},{imm}"

# ---------------- Logisim hex image parser (handles N*value RLE) ----------------

def parse_logisim_image(path):
    with open(path) as f:
        lines = f.read().split()
    if lines and lines[0].startswith("v"):
        lines = lines[1:]  # drop header token(s) like "v3.0"
    if lines and lines[0] in ("hex", "words", "plain"):
        pass  # header may split into multiple tokens depending on export; ignore text tokens
    words = []
    for tok in lines:
        if tok in ("hex", "words", "plain"):
            continue
        if "*" in tok:
            count, val = tok.split("*")
            words.extend([int(val, 16)] * int(count))
        else:
            try:
                words.append(int(tok, 16))
            except ValueError:
                continue
    return words

# ---------------- unpack a log row ----------------
# Real hardware layout: bits[15:0] = instruction, bit16 = stall, bit17 = flush

def unpack_row(raw):
    instr = raw & 0xFFFF
    stall = (raw >> 16) & 0b1
    flush = (raw >> 17) & 0b1
    return instr, stall, flush

BUBBLE = None  # sentinel meaning "nothing real in this stage"

def stage_str(word):
    if word is BUBBLE:
        return "(bubble)"
    return disasm(word)

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 trace_reconstruct.py <exported_trace.txt>")
        sys.exit(1)

    raw_words = parse_logisim_image(sys.argv[1])
    n = len(raw_words)

    dec_seq   = []
    stall_seq = []
    flush_seq = []
    for w in raw_words:
        instr, stall, flush = unpack_row(w)
        dec_seq.append(instr)
        stall_seq.append(stall)
        flush_seq.append(flush)

    # Reconstruct EXEC(N) = bubble if Stall(N-1) else DEC(N-1)
    exec_seq = [BUBBLE] * n
    for i in range(1, n):
        if stall_seq[i-1]:
            exec_seq[i] = BUBBLE
        else:
            exec_seq[i] = dec_seq[i-1]

    # MEM(N) = EXEC(N-1), WB(N) = MEM(N-1)  -- always, unconditional shift
    mem_seq = [BUBBLE] * n
    wb_seq  = [BUBBLE] * n
    for i in range(1, n):
        mem_seq[i] = exec_seq[i-1]
    for i in range(1, n):
        wb_seq[i] = mem_seq[i-1]

    # IF(N) best-effort inference = DEC(N+1), unless that transition was a stall/flush artifact
    if_seq = [None] * n
    for i in range(n - 1):
        if_seq[i] = dec_seq[i+1]

    # ---- auto-truncate: stop a few cycles after the last real instruction ----
    # find the last cycle where DEC held a genuine (non-zero) instruction, or a
    # stall/flush event occurred -- then print a small drain tail past it and stop.
    last_active = 0
    for i in range(n):
        if dec_seq[i] != 0 or stall_seq[i] or flush_seq[i]:
            last_active = i
    DRAIN_TAIL = 6  # extra cycles to show WB actually finishing the last instruction
    print_limit = min(n, last_active + DRAIN_TAIL + 1)

    col_w = 22
    header = (f"{'Cyc':>4} | {'IF':<{col_w}} | {'DEC':<{col_w}} | {'EXEC':<{col_w}} | "
              f"{'MEM':<{col_w}} | {'WB':<{col_w}} | Event")
    print(header)
    print("-" * len(header))
    forwarding_events = []
    for i in range(print_limit):
        event = ""
        if flush_seq[i]:
            event += "FLUSH(IF/DEC) "
        if stall_seq[i]:
            event += "STALL(DEC/EX) "
        if_disp = disasm(if_seq[i]) if if_seq[i] is not None else "?"
        if stall_seq[i]:
            if_disp = f"(frozen)"
        fwd = forwarding_note(exec_seq[i], mem_seq[i], wb_seq[i])
        if fwd:
            forwarding_events.append((i, fwd))
        dec_fwd = forwarding_note(dec_seq[i], mem_seq[i], wb_seq[i]) if resolves_in_dec(dec_seq[i]) else ""
        if dec_fwd:
            forwarding_events.append((i, dec_fwd + "  (into DEC comparator)"))
        print(f"{i:>4} | {if_disp:<{col_w}} | {stage_str(dec_seq[i]):<{col_w}} | "
              f"{stage_str(exec_seq[i]):<{col_w}} | {stage_str(mem_seq[i]):<{col_w}} | "
              f"{stage_str(wb_seq[i]):<{col_w}} | {event}")
    if print_limit < n:
        print(f"\n... truncated {n - print_limit} trailing empty cycles "
              f"(RAM depth {n}, last real activity at cycle {last_active}) ...")

    print(f"\n--- Forwarding events ---")
    if forwarding_events:
        for cyc, note in forwarding_events:
            print(f"  cycle {cyc:>3}: {note}")
    else:
        print("  (none detected)")
    print("  (best-effort, inferred from register-number overlap -- not a directly")
    print("   logged signal; PUSH/POP's dedicated SP write port is not represented")
    print("   Forwrding to MEM unit is not represented)")
    # ---- IPC computation ----
    # Skip leading pipeline-fill emptiness symmetrically with trailing drain:
    # find the first cycle with genuine activity (nonzero DEC, or a stall/flush),
    # same as last_active finds the last. Rows before that are not real dispatches
    # at all -- just pre-fetch/reset emptiness, not program content.
    first_active = None
    for i in range(n):
        if dec_seq[i] != 0 or stall_seq[i] or flush_seq[i]:
            first_active = i
            break
    if first_active is None:
        first_active = 0

    # A "fresh dispatch" into DEC happens whenever DEC's content this cycle is NOT
    # a held-over repeat from a stall. We exclude dispatches that are provably
    # flush-injected bubbles: content 0 AND immediately follows a logged Flush.
    # A real hand-written NOP occurring in that exact spot would be misclassified
    # too -- rare, disclosed rather than silently assumed away.
    dispatched = 0
    bubbles_excluded = 0
    for i in range(first_active, last_active + 1):
        is_fresh = (i == first_active) or (not stall_seq[i-1])
        if not is_fresh:
            continue
        is_flush_bubble = (dec_seq[i] == 0) and (i > 0 and flush_seq[i-1])
        if is_flush_bubble:
            bubbles_excluded += 1
        else:
            dispatched += 1

    # Total elapsed cycles from first real activity to full drain of the last
    # dispatched instruction (3 more edges: EXEC->MEM->WB).
    total_cycles = (last_active - first_active) + 1 + 3
    ipc = dispatched / total_cycles if total_cycles else 0.0
    cpi = total_cycles / dispatched if dispatched else float('inf')

    print(f"\n--- IPC ---")
    print(f"Measured window               : cycles {first_active}-{last_active} "
          f"(+3 to drain to WB) = {total_cycles} cycles")
    print(f"Instructions dispatched (net) : {dispatched}  "
          f"(excluded {bubbles_excluded} flush-injected bubble(s))")
    print(f"IPC = {ipc:.3f}   CPI = {cpi:.3f}")
    print("Note: fixed pipeline fill/drain overhead dominates short test programs --")
    print("      IPC is only meaningful over a program long enough to amortize it.")
    print("A real NOP written immediately after a flush would be misclassified as a")
    print("bubble and excluded -- rare edge case, not corrected for.")

if __name__ == "__main__":
    main()