// Instruction Fetch Stage

#pragma once

#include "BTB.hpp"



class IF
{

public:
    IFID_REG run(const BTB &btb, const word &programCounter, const std::array<word, MEM_SIZE> &imem)
    {
        IFID_REG ifid;
        byte rasPtr;
        const BTB_RET btbResult = btb.searchRows(programCounter);
        ifid.counter = btbResult.counter;
        ifid.predictedTaken = btbResult.predictedTaken;
        ifid.btbHit = btbResult.HIT;
        ifid.predictedTarget = btbResult.predictedTarget;
        ifid.pc = programCounter;

        // Incase prediction is correct DEC will assume PC is set by IF stage.
        if (ifid.predictedTaken)
            ifid.pcNext = ifid.predictedTarget;
        else
            ifid.pcNext = ifid.pc + 1;

        ifid.instruction = imem[programCounter];

        return ifid;
    }
};