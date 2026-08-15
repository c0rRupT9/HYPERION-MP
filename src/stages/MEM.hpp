// Memory Stage
#pragma once



class MEM
{
    public:
    MEMWB_REG run(const EXMEM_REG& exmem, ForwardResult& EXMEM_FOR, const ForwardResult& MEMWB_FOR ,std::array<word, MEM_SIZE>& DMEM)
    {
        MEMWB_REG memwbNext {};
        byte RS2 = exmem.RS2;
        word RS2_VAL = exmem.storeValue;

        if(MEMWB_FOR.RD == RS2 && MEMWB_FOR.RD != 0 && MEMWB_FOR.regWrite)
        {
            RS2_VAL = MEMWB_FOR.value;
        }
        

        if(exmem.memWrite)
        {
            if(exmem.spType) DMEM[exmem.spOld] = RS2_VAL;
            else  DMEM[exmem.aluResult] = RS2_VAL;
        }
        if(exmem.memRead)
        {
            memwbNext.ramOut = DMEM[exmem.aluResult];
        }

        EXMEM_FOR.RD = exmem.RD;
        EXMEM_FOR.regWrite = exmem.regWrite;
        EXMEM_FOR.spWrite = exmem.spType;
        EXMEM_FOR.value = exmem.aluResult;

        memwbNext.aluResult = exmem.aluResult;
        memwbNext.memToReg = exmem.memToReg;
        memwbNext.RD = exmem.RD;
        memwbNext.regWrite = exmem.regWrite;
        memwbNext.spType = exmem.spType;

        return memwbNext;
    }




};