// Execute Stage.

#pragma once



class EX
{


    word ALU (word a, word b, byte aluOp)
    {
        word res;
        switch(aluOp)
        {
            case 0:
                res = a + b;
                break;
            case 1:
                res =  a * b;
                break;
            case 2:
                res = a ^ b;
                break;
            case 3:
                res = a & b;
                break;
            case 4:
                res = a | b;
                break;
            case 5: 
                res = (int16_t)a < (int16_t)b ? 1 : 0;
                break;
            case 6:
                res = b;
                break;
            case 7:
                res = a - b;
                break;
        }
        return res;
    }

  
    public:
    EXMEM_REG run(const IDEX_REG& idex, const ForwardResult& EXMEM_FOR, const ForwardResult& MEMWB_FOR,
         const ForwardResult& MEMWB_FORSP, const word& SP)
    {
        EXMEM_REG exmemNext{};
        word a, b;
        word RS1Value = idex.RS1_VALUE;
        word RS2Value = idex.RS2_VALUE;
        byte RS1Tag = idex.RS1;
        byte RS2Tag = idex.RS2;

        // Forwarding Unit Logic
        // There are Two kinds of Forwarders Involved in EXEC unit, since we dont branch using SP noramlly we forward SP only 
        // when R7 is explictly mentioned or the instruction in EXEC is a PUSH-POP event. Since, SP-Type does not explictly 
        // mention SP register in any way we need to make special Ammends and look spType signals in MEM and WB units.


        if(idex.spType) {RS1Value = SP;}  // Register 7 is SP register 
         // MEM unit can store DMEM output or ALU_output into GPR but for SP we only need
        // ALU_OUT since the new SP was calculated by ALU while POP stores two values at the same time.

        if(RS1Tag != 0)
        {
            if((RS1Tag == EXMEM_FOR.RD && EXMEM_FOR.regWrite) || RS1Tag == 7 && EXMEM_FOR.spWrite)
            RS1Value = EXMEM_FOR.value;
            else if(MEMWB_FORSP.spWrite && (RS1Tag == 7)) 
            RS1Value = MEMWB_FORSP.value;
            else if(RS1Tag == MEMWB_FOR.RD && MEMWB_FOR.regWrite) 
            RS1Value = MEMWB_FOR.value;

        }

        if(RS2Tag != 0)
        {
            if((RS2Tag == EXMEM_FOR.RD && EXMEM_FOR.regWrite) || RS2Tag == 7 && EXMEM_FOR.spWrite)
            RS2Value = EXMEM_FOR.value;
            else if(MEMWB_FORSP.spWrite && (RS2Tag == 7)) // Incase we are pushing the stack RD in MEMWEB can match 
            RS2Value = MEMWB_FORSP.value;
            else if(RS2Tag == MEMWB_FOR.RD && MEMWB_FOR.regWrite) 
            RS2Value = MEMWB_FOR.value;


        }

        a = RS1Value;

        switch(idex.ALUSrc)
        {
            case 0:
                b = RS2Value;
                break;
            case 1:
                b = idex.LUI_IMM;
                break;
            case 2:
                b = idex.IMM;
                break;
            case 3:
                b = idex.pcNext;
                break;
        }
        exmemNext.RS2 = idex.RS2; // Tag for forwarding into EX/MEM unit
        exmemNext.aluResult = ALU(a, b, idex.ALUOp);
        exmemNext.RD = idex.RD;
        exmemNext.storeValue = RS2Value;
        exmemNext.spOld = RS1Value;
        exmemNext.memRead = idex.memRead;
        exmemNext.memWrite = idex.memWrite;
        exmemNext.memToReg = idex.memToReg;
        exmemNext.regWrite = idex.regWrite;
        exmemNext.spType = idex.spType;

        return exmemNext;


    }
         
};