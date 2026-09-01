// Instruction Fetch Stage

#pragma once

#include "BTB.hpp"
#include "DEC.hpp"

class IF
{
public:
    IFID_REG run(const BTB &btb, word programCounter, ReturnAddrStack &ras, bool flush, bool stall, const std::array<word, MEM_SIZE> &imem)
    {
        IFID_REG ifid;
        byte rasPtr = 0;
        byte type = 0, RD = 0, RS1 = 0, opcode = 0;
        word imm = 0;
        const BTB_RET btbResult = btb.searchRows(programCounter);
        ifid.counter = btbResult.counter;
        ifid.predictedTaken = btbResult.predictedTaken;
        ifid.btbHit = btbResult.HIT;
        ifid.predictedTarget = btbResult.predictedTarget;
        ifid.pc = programCounter;
        ifid.instruction = imem[programCounter];
        preDecode(ifid.instruction, opcode, RD, RS1, imm);

        if (!stall && !flush)
        {
            if (opcode == JAL && !ifid.btbHit) // Invoke only on very first appearance of JAL later handled by BTB
            {
                ifid.predictedTaken = true;
                ifid.predictedTarget = ifid.pc + imm;
            }

            bool isRasPop = (opcode == JALR && RS1 == JAX);
            bool isRasPush = ((opcode == JAL || opcode == JALR) && RD == JAX);

            // 2. Execute POP logic if applicable
            if (isRasPop)
            {
                rasPtr = ras.pop(ifid.predictedTarget);
                ifid.predictedTaken = true;
                ifid.rasPOP = true;
            }

            // 3. Execute PUSH logic if applicable (Can happen in the same cycle as POP)
            if (isRasPush)
            {
                rasPtr = ras.push(ifid.pc + 1);
                ifid.rasPush = true;
            }
        }

        // Incase prediction is correct DEC will assume PC is set by IF stage.
        if (ifid.predictedTaken)
            ifid.pcNext = ifid.predictedTarget;
        else
            ifid.pcNext = ifid.pc + 1;

        ifid.rasPtr = rasPtr; // forward the pointer to stack

        return ifid;
    }

private:
    static void preDecode(word instr, byte &opcode, byte &RD, byte &RS1, word &imm)
    {

        opcode = (instr >> 11) & 0x1F;
        RD = (byte)(instr >> 8) & 0b111;
        RS1 = (byte)(instr >> 5) & 0b111;
        imm = ID::signExtend_16(instr & 0xFF, 8);
    }
};