// Instruction Fetch Stage

#pragma once

#include "BTB.hpp"



class IF
{
public:
    IFID_REG run(const BTB &btb, const word &programCounter, ReturnAddrStack& ras, const bool& flush, const bool& stall ,const std::array<word, MEM_SIZE> &imem)
    {
        IFID_REG ifid;
        byte rasPtr;
        byte type = 0;
        const BTB_RET btbResult = btb.searchRows(programCounter);
        ifid.branchType = btbResult.branchType;
        ifid.counter = btbResult.counter;
        ifid.predictedTaken = btbResult.predictedTaken;
        ifid.btbHit = btbResult.HIT;
        ifid.predictedTarget = btbResult.predictedTarget;
        ifid.pc = programCounter;
        ifid.instruction = imem[programCounter];
        if(!flush && !stall){
            if(btbResult.branchType == 2) rasPtr = ras.pop(ifid.predictedTarget, ifid.pc);
            else if(btbResult.branchType == 1) rasPtr = ras.push(ifid.pc + 1, ifid.pc);
        }
        // Incase prediction is correct DEC will assume PC is set by IF stage.
        if (ifid.predictedTaken)
            ifid.pcNext = ifid.predictedTarget;
        else
            ifid.pcNext = ifid.pc + 1;
        
        ifid.rasPtr = rasPtr;


        return ifid;
    }
};