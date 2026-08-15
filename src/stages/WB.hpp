// WriteBack unit

#pragma once


class WB
{

    public:
    void run(const MEMWB_REG & memwb, RegisterFile &registerFile, ForwardResult &MEMWB_FOR, ForwardResult &MEMWB_FORSP)
    {
        MEMWB_FOR.regWrite = memwb.regWrite;
        MEMWB_FORSP.spWrite = memwb.spType;

        if (memwb.regWrite)
        {
            if (memwb.memToReg)
            {
                registerFile.writeNormal(memwb.RD, memwb.ramOut);
                MEMWB_FOR.value = memwb.ramOut;
            }
            else
            {
                registerFile.writeNormal(memwb.RD, memwb.aluResult);
                MEMWB_FOR.value = memwb.aluResult;
            }
            MEMWB_FOR.RD = memwb.RD;
        }
        if (memwb.spType)
        {
            registerFile.writeSP(memwb.aluResult);
            MEMWB_FORSP.value = memwb.aluResult;
        }
        registerFile.commit();
    }
};