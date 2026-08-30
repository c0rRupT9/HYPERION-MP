"""
32 rows (address = {Tag[1:0], Subop[2:0]}), 16-bit rows.

Bit layout (matches your table exactly):
  [1:0]   ALUSrc     : 00=GPR(2nd operand) 01=LUI 10=Imm 11=PC_Next
  [4:2]   ALUOp      : 000 ADD 001 MUL 010 XOR 011 AND 100 OR 101 SLT 110 PASSTHROUGH 111 SUB
  [5]     MemRead
  [6]     MemWrite
  [7]     MemAddrSel : 0=GPR/forwarded  1=SP/forwarded-SP into ALU's RS1 slot
  [8]     Reserved   (byte/word select - unused for now, always 0)
  [9]     RegWrite
  [10]    MemToReg   : 0=ALU result  1=loaded memory value
  [11]    IsBranch   : conditional branches AND JALR (both can need hazard stalls)
  [12]    IsJump     : JAL only (immediate target, never needs a stall)
  [13]    SPsel      : diverts Rd to the Rs2 slot (PUSH reads its value this way)
  [14]    Halt
  [15]    padding

NOTE - open item, see accompanying explanation:
 
"""

FIELDS = [
    ("ALUSrc",    2),  # bits 1:0
    ("ALUOp",     3),  # bits 4:2
    ("MemRead",   1),  # bit 5
    ("MemWrite",  1),  # bit 6
    ("MemAddrSel",1),  # bit 7
    ("Reserved",  1),  # bit 8
    ("RegWrite",  1),  # bit 9
    ("MemToReg",  1),  # bit 10
    ("IsBranch",  1),  # bit 11
    ("IsJump",    1),  # bit 12
    ("SPsel",     1),  # bit 13
    ("Halt",      1),  # bit 14 
]
ROW_BITS = sum(w for _, w in FIELDS)   # 15
ROW_BITS_PADDED = 16

# ---- named constants ----
ALUSRC_RS2, ALUSRC_LUI, ALUSRC_IMM, ALUSRC_PCNEXT = range(4)
ALUOP_ADD, ALUOP_MUL, ALUOP_XOR, ALUOP_AND, ALUOP_OR, ALUOP_SLT, ALUOP_PASSTHROUGH, ALUOP_SUB = range(8)

DEFAULT = dict(ALUSrc=ALUSRC_RS2, ALUOp=ALUOP_ADD, MemRead=0, MemWrite=0,
               MemAddrSel=0, Reserved=0, RegWrite=0, MemToReg=0,
               IsBranch=0, IsJump=0, SPsel=0, Halt=0)

def sig(**overrides):
    d = DEFAULT.copy()
    d.update(overrides)
    return d

def pack(d):
    val = 0
    # build LSB-first, since the spec numbers bits low-to-high from ALUSrc
    for name, width in reversed(FIELDS):
        val = (val << width) | (d[name] & ((1 << width) - 1))
    return val  # already occupies bits [13:0], fits directly in 16-bit padded row

# ---- ISA table ----
ISA = {}
def add(tag, subop, **kw):
    ISA[(tag, subop)] = sig(**kw)

# ---------- R-type (tag=00) ----------
add(0b00, 0b000)                                                       # NOP
add(0b00, 0b001, RegWrite=1, ALUOp=ALUOP_ADD)                          # ADD
add(0b00, 0b010, RegWrite=1, ALUOp=ALUOP_MUL)                          # MUL
add(0b00, 0b011, RegWrite=1, ALUOp=ALUOP_XOR)                          # XOR
add(0b00, 0b100, RegWrite=1, ALUOp=ALUOP_AND)                          # AND
add(0b00, 0b101, RegWrite=1, ALUOp=ALUOP_OR)                           # OR
add(0b00, 0b110, RegWrite=1, ALUOp=ALUOP_SLT)                          # SLT
# 111 RES -> default

# ---------- I-type (tag=01) ----------
# 000 RES -> default
add(0b01, 0b001, RegWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_ADD)       # ADDI
add(0b01, 0b010, RegWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_MUL)       # MULI
add(0b01, 0b011, RegWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_XOR)       # XORI
add(0b01, 0b100, RegWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_AND)       # ANDI
add(0b01, 0b101, RegWrite=1, MemRead=1, MemToReg=1,
                 ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_ADD)                   # LOAD
add(0b01, 0b110, RegWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_SLT)       # SLTI
add(0b01, 0b111, RegWrite=1, ALUSrc=ALUSRC_PCNEXT, ALUOp=ALUOP_PASSTHROUGH,
                 IsJump=1)                                           # JALR

# ---------- SB-type (tag=10) ----------
add(0b10, 0b000, IsBranch=1)                                           # BGEU
add(0b10, 0b001, IsBranch=1)                                           # BNE
add(0b10, 0b010, IsBranch=1)                                           # BLTU
add(0b10, 0b011, IsBranch=1)                                           # BGE
add(0b10, 0b100, IsBranch=1)                                           # BEQ
add(0b10, 0b101, IsBranch=1)                                           # BLT
add(0b10, 0b110, MemWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_ADD)       # STORE
add(0b10, 0b111, Halt=1)                                               # HALT


# ---------- J-type (tag=11) ----------
add(0b11, 0b000, RegWrite=1, ALUSrc=ALUSRC_PCNEXT, ALUOp=ALUOP_PASSTHROUGH,
                 IsJump=1)                                             # JAL
add(0b11, 0b001, RegWrite=1, ALUSrc=ALUSRC_LUI, ALUOp=ALUOP_PASSTHROUGH)  # LUI
add(0b11, 0b010, RegWrite=1, ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_PASSTHROUGH)  # MOVI
add(0b11, 0b011, MemWrite=1, MemAddrSel=1, SPsel=1,
                 ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_ADD)                   # PUSH: SP+1 (imm = 1) 
add(0b11, 0b100, RegWrite=1, MemRead=1, MemToReg=1, MemAddrSel=1,
                 ALUSrc=ALUSRC_IMM, ALUOp=ALUOP_SUB)                   # POP: addr = SP-1 (imm = 1)
                                                                        # SP+1 write-back: external, see note above
# 101-111 RESERVED -> default

# ---- build the 32-row image ----
rows = []
for tag in range(4):
    for subop in range(8):
        d = ISA.get((tag, subop), DEFAULT)
        rows.append(pack(d))

hexdigits = ROW_BITS_PADDED // 4  # 4 hex digits
out_path = "controlrom.hex"
with open(out_path, "w") as f:
   # f.write("v3.0 hex words plain\n") Uncomment only when building for Logisim File
    f.write(" ".join(f"{r:0{hexdigits}x}" for r in rows))
    f.write("\n")

print(f"Wrote {len(rows)} rows, {ROW_BITS_PADDED}-bit width, to {out_path}")
print()
names = ["NOP","ADD","MUL","XOR","AND","OR","SLT","RES(R7)",
         "RES(I0)","ADDI","MULI","XORI","ANDI","LOAD","SLTI","JALR",
         "BGEU","BNE","BLTU","BGE","BEQ","BLT","STORE","HALT",
         "JAL","LUI","MOVI","PUSH","POP","RES(J5)","RES(J6)","RES(J7)"]
print(f"{'Tag':4}{'Sub':5}{'Instr':>8}   hex   bin(16b)")
for i, r in enumerate(rows):
    tag, subop = divmod(i, 8)
    print(f"{tag:02b}  {subop:03b}  {names[i]:>8}   {r:0{hexdigits}x}   {r:016b}")