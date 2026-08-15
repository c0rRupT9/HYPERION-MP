
#pragma once

#include "../types.hpp"
#include <array>
#include <cstdio>


const size_t BTB_SIZE = 32;

// ============================== Register file ==============================
// 8 GPRs. R0 hardwired zero. R7 = SP, with its OWN dedicated write port
// separate from the normal Rd-addressed port.
class RegisterFile
{
private:
    std::array<word, 8> regs;
    bool pendingNormal = false, pendingSP = false;
    byte pendingNormalReg = 0;
    word pendingNormalVal = 0, pendingSPVal = 0;

public:
    RegisterFile() { regs.fill(0); }

    word read(byte r) const
    {
        if (r == 0)
            return 0; // R0 always reads zero
        return regs[r];
    }

    // Normal write port targets whatever Rd says. Never touches R0.
    void writeNormal(byte rd, word value)
    {
        if (rd == 0)
            return; // writes to R0 are silently discarded
        pendingNormal = true;
        pendingNormalReg = rd;
        pendingNormalVal = value;
    }

    // Dedicated SP write port. always targets register 7, independent of
    // whatever the normal port is doing this same cycle (this is exactly
    // what makes POP's simultaneous "write Rd" + "write SP" possible).
    void writeSP(word value)
    {
        pendingSP = true;
        pendingSPVal = value;
    }


    void commit()
    {
        if (pendingNormal)
            regs[pendingNormalReg] = pendingNormalVal;
        if (pendingSP)
            regs[7] = pendingSPVal;
        pendingNormal = pendingSP = false;
    }

    void dump() const
    {
        for (int i = 0; i < 8; i++)
            printf("  R%d = %d (0x%04X)\n", i, (int16_t)regs[i], regs[i]);
    }
};

class BTB
{
    struct btbRow
    {
        byte valid;
        word predictedTarget;
        word pcUpperBTB;
        byte counter;
    };

    std::array<btbRow, BTB_SIZE> rows = {0};

public:
    BTB_RET searchRows(word currentPC) const
    {
        BTB_RET returnResult;
        word pcCurrentUpper = (currentPC >> 5) & 0x7FF;   // Upper 11-bits for comparing the result
        word pcCurrentLower = currentPC & (BTB_SIZE - 1); // Lower 5-bits to search BTB rows

        const btbRow &row = rows[pcCurrentLower];
        byte counterMSB = (row.counter >> 1) & 0b1;

        returnResult.predictedTarget = row.predictedTarget;
        returnResult.HIT = row.valid && (pcCurrentUpper == row.pcUpperBTB);
        returnResult.predictedTaken = counterMSB && returnResult.HIT;
        returnResult.counter = row.counter;
        returnResult.valid = row.valid;

        return returnResult;
    }

    void update(word currentPC, word calculatedTarget, byte calculatedCounter, byte validBit)
    {
        word pcCurrentUpper = (currentPC >> 5) & 0x7FF;
        word pcCurrentLower = currentPC & 0b11111;
        btbRow &row = rows[pcCurrentLower];
        row.predictedTarget = calculatedTarget;
        row.pcUpperBTB = pcCurrentUpper;
        row.counter = calculatedCounter;
        row.valid = validBit;
        return;
    }
};
