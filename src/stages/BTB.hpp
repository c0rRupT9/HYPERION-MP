
#pragma once

#include "../types.hpp"
#include <array>
#include <cstdio>

const size_t BTB_SIZE = 32;
const size_t RAS_DEPTH = 8;

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

    std::string dump() const
    {
        // Static buffer holds all 8 formatted register lines 
        static char buf[256];

        char *ptr = buf;
        char *const end = buf + sizeof(buf);

        for (int i = 0; i < 8; i++)
        {
            uint16_t u_val = (uint16_t)(regs[i]);
            int16_t s_val = (int16_t)(regs[i]);

            // snprintf returns characters written; offset pointer directly
            int written = std::snprintf(ptr, end - ptr, "  R%d = %-6d (0x%04X)\n", i, s_val, u_val);
            if (written > 0 && ptr + written < end)
            {
                ptr += written;
            }
            else
            {
                break; // Guard against buffer overflow
            }
        }

        return std::string_view(buf, ptr - buf).data();
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
    {   // 16-bit value  
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

class ReturnAddrStack
{

public:
    ReturnAddrStack() { stack.fill(0); }

    byte push(word pcNext)
    {
        ptr = (ptr + 1) & RAS_DEPTH - 1;
        stack[ptr] = pcNext;

        return ptr;
    }

    byte pop(word &predictedPc)
    {
        predictedPc = stack[ptr];
        ptr = (ptr - 1) & RAS_DEPTH - 1;

        return ptr;
    }

    void ptrUpdate(byte &correctedPtr)
    {
        ptr = correctedPtr;
    }

    std::string dump() const
    {
        // Static buffer holds all 8 formatted register lines 
        static char buf[256];

        char *ptr = buf;
        char *const end = buf + sizeof(buf);

        for (int i = 0; i < 8; i++)
        {
            uint16_t u_val = (uint16_t)(stack[i]);

            // snprintf returns characters written; offset pointer directly
            int written = std::snprintf(ptr, end - ptr, "  Srack%d = %-6d (0x%04X)\n", i, u_val);
            if (written > 0 && ptr + written < end)
            {
                ptr += written;
            }
            else
            {
                break; // Guard against buffer overflow
            }
        }

        return std::string_view(buf, ptr - buf).data();
    }

private:
    std::array<word, RAS_DEPTH> stack;
    int8_t ptr = -1;
};