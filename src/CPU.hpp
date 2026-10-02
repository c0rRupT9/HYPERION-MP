#ifndef CPU_HPP
#define CPU_HPP

#include "core.hpp"
#include "stages/IF.hpp"
#include "stages/DEC.hpp"
#include "stages/EXE.hpp"
#include "stages/MEM.hpp"
#include "stages/WB.hpp"
#include "../tools/TraceWrite.hpp"

namespace risc
{
    class CPU
    {
        IFID_REG ifidCurr{}, ifidNext{};
        IDEX_REG idexCurr{}, idexNext{};
        EXMEM_REG exmemCurr{}, exmemNext{};
        MEMWB_REG memwbCurr{}, memwbNext{};
        BTB btb;
        RegisterFile regs;
        ReturnAddrStack ras;

        IF ifStage;
        ID idStage;
        EX exStage;
        MEM memStage;
        WB wbStage;
        TraceWriter trace;

        word pc = 0, pcNext = 0, lastPc = 0, samePcCount = 0;
        bool halted = false;
        std::string log;

        // MEM_SIZE is declared in TYPES_HPP as a STATIC CONST
        std::array<word, MEM_SIZE> imem{}, dmem{};
        ForwardResult EXMEM_FOR{}, MEMWB_FOR{}, MEMWB_FORSP{};

        HazardResult hazard(const IDEX_REG &idexCurr, const IDEX_REG &idexNext,
                            const EXMEM_REG &exmemCurr, const ID::Output &decOut)
        {
            HazardResult result;
            bool loadStoreStall = false;
            bool loadUseStall = false;

            // Helper: verify destination is non-zero and matches source registers
            auto matches_rs = [&](uint8_t rd) noexcept
            {
                return (rd != 0) && (((idexNext.RS1 == rd) && decOut.usesRS1) || ((idexNext.RS2 == rd) && decOut.usesRS2));
            };
            // STORE and PUSH cases will be evaluated by loadStoreStall
            loadStoreStall = decOut.usesRS1 && (idexCurr.memRead && idexNext.RS1 == idexCurr.RD) && (idexCurr.RD != 0);
            // Will invoke only when current instruction in DEC is not a STORE or PUSH
            loadUseStall = (idexCurr.memRead && matches_rs(idexCurr.RD)) && !decOut.isStore;
            result.branchArithStall = decOut.isBranchOrJalr && idexCurr.regWrite && matches_rs(idexCurr.RD);

            bool branchMemStall = exmemCurr.memRead && matches_rs(exmemCurr.RD);
            result.branchLoadStall = decOut.isBranchOrJalr && (result.loadUseStall || branchMemStall);
            result.loadUseStall = loadStoreStall || loadUseStall;
            result.stall = result.branchArithStall || result.branchLoadStall || result.loadUseStall;
            return result;
        }

        void step(size_t cycle)
        {
            // WB Stage
            wbStage.run(memwbCurr, regs, MEMWB_FOR, MEMWB_FORSP);

            // MEM Stage
            memwbNext = memStage.run(exmemCurr, EXMEM_FOR, MEMWB_FOR, dmem);

            // EX Stage
            word SP = regs.read(7);
            exmemNext = exStage.run(idexCurr, EXMEM_FOR, MEMWB_FOR, MEMWB_FORSP, SP);

            // ID Stage
            auto decOut = idStage.run(ifidCurr, EXMEM_FOR, MEMWB_FOR, regs);
            idexNext = decOut.nextIdex;

            // Hazard Detection
            HazardResult result = hazard(idexCurr, idexNext, exmemCurr, decOut);
            bool stall = result.stall;

            // IF Stage
            ifidNext = ifStage.run(btb, pc, ras, decOut.mismatch, stall, imem);

            // Save cuurent pc for BTB update
            word branchPc = ifidCurr.pc;

            decOut.mismatch = decOut.mismatch & !stall; // Prevents false flushes in real systems Stall overrides everything
            // in logisim a priority encoder and a priority MUX encodes it 
            // this golden model mimics that property even though it will work just fine without it


            // Refer to line 277, function risc::CPU::run.
            if (debugTrace)
                trace.record(ifidCurr.instruction, stall, decOut.mismatch);

            // Pipeline Control & Latch Updates
            if (stall)
            {
                // STALL: Freeze PC & IF/ID, Inject Bubble into ID/EX
                pcNext = pc; // Freeze PC
                // ifidCurr remains unchanged
                idexCurr = IDEX_REG{}; // NOP Bubble
                exmemCurr = exmemNext;
                memwbCurr = memwbNext;
            }
            else if (decOut.mismatch)
            {
                if (decOut.rasPtrUpdate)
                    ras.ptrUpdate(decOut.correctedPtr);
                // MISPREDICTION FLUSH: Update PC, Flush IF/ID
                pcNext = decOut.targetPc;
                ifidCurr = IFID_REG{}; // Flush IF/ID latch
                idexCurr = idexNext;
                exmemCurr = exmemNext;
                memwbCurr = memwbNext;

                if (decOut.updateBTB)
                    btb.update(branchPc, decOut.targetAdress, decOut.counter, 1);
            }
            else
            {
                // NORMAL STEP
                pcNext = ifidNext.pcNext; // Respect BTB target prediction from IF stage
                ifidCurr = ifidNext;
                idexCurr = idexNext;
                exmemCurr = exmemNext;
                memwbCurr = memwbNext;

                if (decOut.updateBTB)
                    btb.update(branchPc, decOut.targetAdress, decOut.counter, 1);
            }

            if (debugTraceInline)
                log += trace.dumpTrace(cycle, stall, idexCurr, ifidCurr, exmemCurr, pc, ifidCurr.instruction);

            if (pc == lastPc)
                samePcCount = samePcCount + 1;
            else
                samePcCount = 0;
            if (samePcCount > 2) // Since maximum amount of times PC can stay same is BranchMemStall case
                halted = true;

            lastPc = pc;

            // DO NOT
            // regs.dump();

            // Advance PC to PC_NEXT
            pc = pcNext;
        }

    public:
        void loadProgram(const std::vector<word> &program)
        {
            for (size_t i = 0; i < program.size() && i < MEM_SIZE; ++i)
            {
                imem[i] = program[i];
            }
        }

        void run(size_t cycles)
        {
            for (size_t i = 0; i < cycles; i++)
            {
                step(i);
                if (halted)
                    break;
            }

            // debugTrace is a global Declaration, if you dont require Debug Traces set it to false in TYPES_HPP
            if (debugTraceInline)
                std::cout << log; // logs
            if (debugTrace)
            {
                // Either Create a log file or dump all the streams to terminal pointer

                // Use when python Trace_constructor needed
                trace.write(trace.next_numbered_path("../hexTraces"));
            }

            std::string finalLog;
             finalLog += regs.dump();
             finalLog += trace.dumpBtb(btb);
             finalLog += trace.dumpMem(dmem);
             finalLog += ras.dump();
            std::cout << finalLog;
            std::cout << " Final PC: " << pc << std::endl;
        }
    };

} // Namespace risc
#endif // CPU_HPP