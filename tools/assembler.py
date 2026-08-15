#!/usr/bin/env python3
"""
HYPERION-MP Assembler
Reads HYPERION-MP assembly

Usage: python3 assembler.py program.asm
Output printed to stdout, and written to program.txt (paste-ready C++ array).

--------------------------------------------------------------------------
ISA reference (16-bit words):
  R-type  [Tag=00][Subop 3][Rd 3][Rs1 3][Rs2 3][rsv 2]
  I-type  [Tag=01][Subop 3][Rd 3][Rs1 3][Imm 5]
  SB-type [Tag=10][Subop 3][ImmHi 3][Rs1 3][Rs2 3][ImmLo 2]
  J-type  [Tag=11][Subop 3][Rd 3][Imm 8]

Mnemonics:
  R:  ADD MUL XOR AND OR SLT       Rd, Rs1, Rs2
  I:  ADDI MULI XORI ANDI SLTI     Rd, Rs1, Imm
      LOAD                        Rd, Rs1, Imm      (addr = Rs1+Imm)
      JALR                        Rd, Rs1, Imm      (target = Rs1+Imm, NOT pc-relative)
  SB: BGEU BNE BLTU BGE BEQ BLT    Rs1, Rs2, target  (target: label or signed imm, PC-relative)
      STORE                       Rs1, Rs2, Imm     (addr = Rs1+Imm, data = Rs2, NOT pc-relative)
  J:  JAL                         Rd, target         (label or signed imm, PC-relative)
      LUI MOVI                    Rd, Imm            (plain immediate, not pc-relative)
      PUSH                        Rd[, Imm]          (Imm defaults to 1)
      POP                         Rd[, Imm]          (Imm defaults to 1)
  Pseudo:
      NOP                         -> encodes as the literal all-zero word
      MOV   Rd, Rs                -> ADD Rd, Rs, R0

Registers: R0-R7 (R7 is SP by convention, R0 is hardwired zero).
Numbers: decimal, 0x.. hex, 0b.. binary, optional leading '-' for negatives.
Comments: ';' to end of line.
Labels:   'label:' on its own, or 'label: INSTR ...' on one line.
--------------------------------------------------------------------------
"""

import sys
import re

# ---------------------------------------------------------------- ISA tables
R_OPS  = {'ADD':1, 'MUL':2, 'XOR':3, 'AND':4, 'OR':5, 'SLT':6}
I_OPS  = {'ADDI':1, 'MULI':2, 'XORI':3, 'ANDI':4, 'LOAD':5, 'SLTI':6, 'JALR':7}
SB_OPS = {'BGEU':0, 'BNE':1, 'BLTU':2, 'BGE':3, 'BEQ':4, 'BLT':5, 'STORE':6}
J_OPS  = {'JAL':0, 'LUI':1, 'MOVI':2, 'PUSH':3, 'POP':4}

REGS = {f'R{i}': i for i in range(8)}

PC_RELATIVE = {'BGEU','BNE','BLTU','BGE','BEQ','BLT','JAL'}  # target = PC_of_this_instr + imm

# ---------------------------------------------------------------- lexer
TOKEN_RE = re.compile(r'[A-Za-z_][A-Za-z0-9_]*|-?0[xX][0-9a-fA-F]+|-?0[bB][01]+|-?\d+|[:,]')

def lex(text):
    tokens = []
    for line_num, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.split(';', 1)[0]
        for m in TOKEN_RE.finditer(line):
            tokens.append((m.group(0), line_num))
    return tokens

def parse_number(tok):
    neg = tok.startswith('-')
    if neg:
        tok = tok[1:]
    if tok.lower().startswith('0x'):
        val = int(tok, 16)
    elif tok.lower().startswith('0b'):
        val = int(tok, 2)
    else:
        val = int(tok, 10)
    return -val if neg else val

# ---------------------------------------------------------------- bit-packing
def sign_check(val, bits, ctx):
    lo, hi = -(1 << (bits - 1)), (1 << (bits - 1)) - 1
    if not (lo <= val <= hi):
        raise ValueError(f"{ctx}: immediate {val} out of range for {bits}-bit signed field ({lo}..{hi})")

def encode_R(subop, rd, rs1, rs2):
    return (0b00 << 14) | (subop << 11) | (rd << 8) | (rs1 << 5) | (rs2 << 2)

def encode_I(subop, rd, rs1, imm, ctx):
    sign_check(imm, 5, ctx)
    return (0b01 << 14) | (subop << 11) | (rd << 8) | (rs1 << 5) | (imm & 0x1F)

def encode_SB(subop, rs1, rs2, imm, ctx):
    sign_check(imm, 5, ctx)
    imm5 = imm & 0x1F
    immHi = (imm5 >> 2) & 0x7
    immLo = imm5 & 0x3
    return (0b10 << 14) | (subop << 11) | (immHi << 8) | (rs1 << 5) | (rs2 << 2) | immLo

def encode_J(subop, rd, imm, ctx):
    sign_check(imm, 8, ctx)
    return (0b11 << 14) | (subop << 11) | (rd << 8) | (imm & 0xFF)

# ---------------------------------------------------------------- assembler
class AsmError(Exception):
    pass

def group_lines(tokens):
    """Split flat token list back into per-source-line groups (by line_num),
    since instructions never span lines in this syntax."""
    lines = {}
    for tok, ln in tokens:
        lines.setdefault(ln, []).append(tok)
    return [lines[k] for k in sorted(lines)]

def parse_line(toks, line_num):
    """Returns (label_or_None, mnemonic_or_None, [operand_tokens])."""
    label = None
    i = 0
    if len(toks) >= 2 and toks[1] == ':':
        label = toks[0]
        i = 2
    if i >= len(toks):
        return label, None, []
    mnem = toks[i].upper()
    i += 1
    operands = [t for t in toks[i:] if t != ',']
    return label, mnem, operands

def resolve_reg(tok, ctx):
    r = tok.upper()
    if r not in REGS:
        raise AsmError(f"{ctx}: '{tok}' is not a valid register (expected R0-R7)")
    return REGS[r]

def resolve_imm(tok, labels, ctx, pc_relative, this_addr):
    if tok in labels:
        target = labels[tok]
        return target - this_addr if pc_relative else target
    try:
        return parse_number(tok)
    except ValueError:
        raise AsmError(f"{ctx}: '{tok}' is not a valid number or known label")

def assemble(text):
    tokens = lex(text)
    lines = group_lines(tokens)

    # ---- pass 1: find every label's word address ----
    labels = {}
    addr = 0
    for toks in lines:
        if not toks:
            continue
        label, mnem, _ = parse_line(toks, 0)
        if label is not None:
            labels[label] = addr
        if mnem is not None:
            addr += 1  # every instruction is exactly one 16-bit word

    # ---- pass 2: encode ----
    words = []
    addr = 0
    for line_idx, toks in enumerate(lines, 1):
        if not toks:
            continue
        label, mnem, ops = parse_line(toks, line_idx)
        if mnem is None:
            continue
        ctx = f"line ~{line_idx} ('{mnem}')"

        if mnem == 'NOP':
            words.append(0x0000)
        elif mnem == 'MOV':
            if len(ops) != 2:
                raise AsmError(f"{ctx}: MOV needs Rd, Rs")
            rd = resolve_reg(ops[0], ctx)
            rs = resolve_reg(ops[1], ctx)
            words.append(encode_R(R_OPS['ADD'], rd, rs, 0))  # ADD Rd, Rs, R0
        elif mnem in R_OPS:
            if len(ops) != 3:
                raise AsmError(f"{ctx}: {mnem} needs Rd, Rs1, Rs2")
            rd, rs1, rs2 = (resolve_reg(o, ctx) for o in ops)
            words.append(encode_R(R_OPS[mnem], rd, rs1, rs2))
        elif mnem in I_OPS:
            if len(ops) != 3:
                raise AsmError(f"{ctx}: {mnem} needs Rd, Rs1, Imm")
            rd = resolve_reg(ops[0], ctx)
            rs1 = resolve_reg(ops[1], ctx)
            imm = resolve_imm(ops[2], labels, ctx, False, addr)
            words.append(encode_I(I_OPS[mnem], rd, rs1, imm, ctx))
        elif mnem in SB_OPS and mnem != 'STORE':
            if len(ops) != 3:
                raise AsmError(f"{ctx}: {mnem} needs Rs1, Rs2, target")
            rs1 = resolve_reg(ops[0], ctx)
            rs2 = resolve_reg(ops[1], ctx)
            imm = resolve_imm(ops[2], labels, ctx, mnem in PC_RELATIVE, addr)
            words.append(encode_SB(SB_OPS[mnem], rs1, rs2, imm, ctx))
        elif mnem == 'STORE':
            if len(ops) != 3:
                raise AsmError(f"{ctx}: STORE needs Rs1, Rs2, Imm")
            rs1 = resolve_reg(ops[0], ctx)
            rs2 = resolve_reg(ops[1], ctx)
            imm = resolve_imm(ops[2], labels, ctx, False, addr)
            words.append(encode_SB(SB_OPS['STORE'], rs1, rs2, imm, ctx))
        elif mnem in ('JAL', 'LUI', 'MOVI'):
            if len(ops) != 2:
                raise AsmError(f"{ctx}: {mnem} needs Rd, Imm/target")
            rd = resolve_reg(ops[0], ctx)
            imm = resolve_imm(ops[1], labels, ctx, mnem in PC_RELATIVE, addr)
            words.append(encode_J(J_OPS[mnem], rd, imm, ctx))
        elif mnem in ('PUSH', 'POP'):
            if len(ops) == 1:
                rd = resolve_reg(ops[0], ctx)
                imm = 1
            elif len(ops) == 2:
                rd = resolve_reg(ops[0], ctx)
                imm = resolve_imm(ops[1], labels, ctx, False, addr)
            else:
                raise AsmError(f"{ctx}: {mnem} needs Rd[, Imm]")
            words.append(encode_J(J_OPS[mnem], rd, imm, ctx))
        else:
            raise AsmError(f"{ctx}: unknown mnemonic '{mnem}'")

        addr += 1

    return labels, words

def to_hex(words, name="program"):
    hexvals = " ".join(f"0x{w:04X}" for w in words)
    return hexvals

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 assembler.py program.asm")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        text = f.read()

    try:
        labels, words = assemble(text)
    except AsmError as e:
        print(f"Assembly error: {e}")
        sys.exit(1)

    print("--- LABELS ---")
    for k, v in labels.items():
        print(f"  {k} = word {v} (0x{v:04X})")

    print("\n--- WORDS ---")
    for i, w in enumerate(words):
        print(f"  [{i:3}] 0x{w:04X}  {w:016b}")

    hex = to_hex(words)
    print("\n--- Hex Values ---")
    print(hex)

    out_path = sys.argv[1].rsplit('.', 1)[0] + ".hex"
    with open(out_path, 'w') as f:
        f.write(hex + "\n")
    print(f"\nWrote {out_path}")