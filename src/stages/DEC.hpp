// Decoder Stage
#pragma once
#include "../core.hpp"
#include "BTB.hpp"
// Instruction Structure in DECROM
/*
Tag Sub     Instr   hex   bin(16b)
00  000       NOP   0000   0000000000000000
00  001       ADD   0200   0000001000000000
00  010       MUL   0204   0000001000000100
00  011       XOR   0208   0000001000001000
00  100       AND   020c   0000001000001100
00  101        OR   0210   0000001000010000
00  110       SLT   0214   0000001000010100
00  111   RES(R7)   0000   0000000000000000
01  000   RES(I0)   0000   0000000000000000
01  001      ADDI   0202   0000001000000010
01  010      MULI   0206   0000001000000110
01  011      XORI   020a   0000001000001010
01  100      ANDI   020e   0000001000001110
01  101      LOAD   0622   0000011000100010
01  110      SLTI   0216   0000001000010110
01  111      JALR   121b   0001001000011011
10  000      BGEU   0800   0000100000000000
10  001       BNE   0800   0000100000000000
10  010      BLTU   0800   0000100000000000
10  011       BGE   0800   0000100000000000
10  100       BEQ   0800   0000100000000000
10  101       BLT   0800   0000100000000000
10  110     STORE   0042   0000000001000010
10  111      HALT   4000   0100000000000000
11  000       JAL   121b   0001001000011011
11  001       LUI   0219   0000001000011001
11  010      MOVI   021a   0000001000011010
11  011      PUSH   20c2   0010000011000010
11  100       POP   06be   0000011010111110
11  101   RES(J5)   0000   0000000000000000
11  110   RES(J6)   0000   0000000000000000
11  111   RES(J7)   0000   0000000000000000
*/
// Halt is not explicitly encoded here rather CPU is trapped into infinite loops for this purpose
inline constexpr std::array<word, 32> decoderROM = {0x0, 0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x0, 0x0,
                                                0x202, 0x206, 0x20a, 0x20e, 0x622, 0x216, 0x121b, 0x800,
                                                0x800, 0x800, 0x800, 0x800, 0x800, 0x42, 0x0, 0x121b, 0x219,
                                                0x21a, 0x20c2, 0x6be, 0x0, 0x0, 0x0};

class ID
{
public:
    struct Output
    {
        IDEX_REG nextIdex;
        bool actualTaken = false;
        bool mismatch = false;
        bool updateBTB = false;
        bool isBranchOrJalr = false;
        bool isStore = false;
        bool usesRS1 = false;
        bool usesRS2 = false;
        bool rasPtrUpdate = false;
        word targetAdress = 0;
        word targetPc = 0;
        byte counter = 0, correctedPtr = 0;
    };

    struct InstructionDecode
    {
        word rawValue = 0;
        byte tag = 0;
        byte subOp = 0;
        byte RD = 0;
        byte RS1 = 0;
        byte RS2 = 0;
        word imm = 0;
        bool usesRS1 = false;
        bool usesRS2 = false;
    };

    InstructionDecode decoder(const word instr)
    {
        InstructionDecode dec;
        dec.rawValue = instr;
        dec.tag = (instr >> 14) & 0b11;
        dec.subOp = (instr >> 11) & 0b111;
        dec.RD = (byte)(instr >> 8) & 0b111;
        dec.RS1 = (byte)(instr >> 5) & 0b111;
        dec.RS2 = (byte)(instr >> 2) & 0b111;

        switch (dec.tag)
        {
        case R_TYPE:
        {
            dec.imm = 0;
            dec.usesRS1 = true;
            dec.usesRS2 = true;
            break;
        }

        case I_TYPE:
        {
            dec.imm = signExtend_16(instr & 0x1F, 5);
            dec.usesRS1 = true;
            break;
        }

        case SB_TYPE:
        {
            byte immHI = (instr >> 8) & 0x7;
            byte immLO = instr & 0b11;
            dec.imm = signExtend_16((word)(immHI << 2 | immLO), 5);
            dec.usesRS1 = true;
            dec.usesRS2 = true;
            break;
        }

        case J_TYPE:
        {
            dec.imm = signExtend_16(instr & 0xFF, 8);
            dec.usesRS1 = false;
            dec.usesRS2 = false;
            break;
        }
        }

        return dec;
    }

    static word signExtend_16(word imm, byte bits)
    {
        word mask = (1u << bits) - 1;
        imm &= mask;

        word sign_bits = 1u << (bits - 1);

        return (imm ^ sign_bits) - sign_bits;
    }

    Output run(const IFID_REG &ifid, const ForwardResult &EXMEM_FOR, const ForwardResult &MEMWB_FOR,
               const RegisterFile &registerFile)
    {
        Output idout;

        ControlSignals signals{};
        InstructionDecode decode = decoder(ifid.instruction);
        signals = signalGenerator(decode.tag, decode.subOp);

        if (signals.spSel) // Push uses RS2 instead of RD
            decode.RS2 = decode.RD;

        if (signals.spType)
        {
            decode.RS1 = SP;
        } // If load is loading SP then we need to stall
        // so declare the values before hand we can forward SP or read in EXEC unit.
        // Since we wont be branching using SP we can forward it in EXEC unit only.
        // If you really need to branch using SP use ADD then branch case.

        word RS1_Val = registerFile.read(decode.RS1);
        word RS2_Val = registerFile.read(decode.RS2);

        // Forwarding from EXMEM and MEMWB with priority
        // if else if branches exit once matching contender is found
        idout.usesRS1 = decode.usesRS1;
        idout.usesRS2 = (signals.spSel) ? true : decode.usesRS2; // Since PUSH uses RD as RS2

        if (decode.RS1 != R0)
        {
            if (decode.RS1 == EXMEM_FOR.RD && EXMEM_FOR.regWrite)
                RS1_Val = EXMEM_FOR.value;
            else if (decode.RS1 == MEMWB_FOR.RD && MEMWB_FOR.regWrite)
                RS1_Val = MEMWB_FOR.value;
        }

        if (decode.RS2 != R0)
        {
            if (decode.RS2 == EXMEM_FOR.RD && EXMEM_FOR.regWrite)
                RS2_Val = EXMEM_FOR.value;
            else if (decode.RS2 == MEMWB_FOR.RD && MEMWB_FOR.regWrite)
                RS2_Val = MEMWB_FOR.value;
        }

        if (signals.isBranch || signals.isJump)
        {
                idout.updateBTB = true;

            // BTB comparison Logic
            idout.actualTaken = (signals.isBranch) ? BranchComp(RS1_Val, RS2_Val, (BranchCond)decode.subOp) : true;
            idout.isBranchOrJalr = (signals.isBranch || (signals.isJump && (decode.tag == I_TYPE))) ? true : false;
            // Offset calculation
            //  Include JALR case where type is 0b1 while it is a jump but instead of PC use RS1
            idout.targetAdress = (signals.isJump && (decode.tag == I_TYPE)) ? RS1_Val + decode.imm : ifid.pc + decode.imm;
            idout.targetPc = (idout.actualTaken) ? idout.targetAdress : ifid.pc + 1;

            // counter update logic
            byte currentCounter = ifid.counter;
            if (!ifid.btbHit)
                currentCounter = (idout.actualTaken) ? 1 : 2;

            if (idout.actualTaken)
                currentCounter = (currentCounter >= 3) ? 3 : currentCounter + 1;
            else
                currentCounter = (currentCounter <= 0) ? 0 : currentCounter - 1;

            idout.counter = currentCounter;

            // Mismatch calculation logic
            bool branchMismatch = idout.actualTaken ^ ifid.predictedTaken;
            bool offsetMismatch = idout.actualTaken && ifid.predictedTaken && (ifid.predictedTarget != idout.targetAdress);
            idout.mismatch = (branchMismatch || offsetMismatch);
            idout.rasPtrUpdate = offsetMismatch && ifid.rasPOP; // RAS update
            idout.correctedPtr = ifid.rasPtr + 1;
        }
        idout.isStore = signals.memWrite; // Store Hazard Case Handling
        idout.nextIdex.ALUOp = signals.aluOP;
        idout.nextIdex.ALUSrc = signals.aluSRC;
        idout.nextIdex.memRead = signals.memRead;
        idout.nextIdex.memToReg = signals.memToReg;
        idout.nextIdex.memWrite = signals.memWrite;
        idout.nextIdex.regWrite = signals.regWrite;
        idout.nextIdex.spType = signals.spType;
        idout.nextIdex.RD = decode.RD;
        idout.nextIdex.RS1 = decode.RS1;
        idout.nextIdex.RS2 = decode.RS2;
        idout.nextIdex.IMM = decode.imm;
        idout.nextIdex.LUI_IMM = (decode.imm << 8);
        idout.nextIdex.pcNext = ifid.pc + 1;
        idout.nextIdex.RS1_VALUE = RS1_Val;
        idout.nextIdex.RS2_VALUE = RS2_Val;

        return idout;
    }

private:

    struct ControlSignals
    {
        // There are a total of 14 control Signals some will be forwarded to other stages while three of them will
        // be used up in decoder stage
        bool spSel, isJump, isBranch, memToReg, regWrite, reserve, spType, memWrite, memRead;
        byte aluOP, aluSRC;
    };

    ControlSignals signalGenerator(byte type, byte subOp)
    {
        // Control Signal uses a constant array representing a ROM with instructions
        ControlSignals ctrl{};
        byte opcode = (type << 3 | subOp);
        word signals = decoderROM[opcode];

        // NOTE: There are exactly 2 padding Bits on the MSB side, we will shift in regard to that

        ctrl.spSel =    (signals >> 13) & 1;
        ctrl.isJump =   (signals >> 12) & 1;
        ctrl.isBranch = (signals >> 11) & 1;
        ctrl.memToReg = (signals >> 10) & 1;
        ctrl.regWrite = (signals >> 9)  & 1;
        ctrl.spType =   (signals >> 7)  & 1;
        ctrl.memWrite = (signals >> 6)  & 1;
        ctrl.memRead =  (signals >> 5)  & 1;
        ctrl.aluOP =    (signals >> 2)  & 0b111;
        ctrl.aluSRC =    signals        & 3;

        return ctrl;
    }

    bool BranchComp(word RS1_Val, word RS2_Val, BranchCond cond)
    {
        switch (cond)
        {
        case BranchCond::BGEU:
            return (RS1_Val > RS2_Val);

        case BranchCond::BNE:
            return (RS1_Val != RS2_Val);

        case BranchCond::BLU:
            return (RS1_Val < RS2_Val);

        case BranchCond::BGE:
            return (int16_t)RS1_Val > (int16_t)RS2_Val;

        case BranchCond::BEQ:
            return (RS1_Val == RS2_Val);

        case BranchCond::BLT:
            return (int16_t)RS1_Val < (int16_t)RS2_Val;

        default:
            return false;
        }
    }
};