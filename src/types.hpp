// types.hpp
// Pure Data structs defining Stage Boundries. No logic is explained or Implemented here
// only describing what needs to physically ride from one stage to other, mirroring Logisim pipeline registers
// There are 5 stages IF, ID, EX, MEM, WB devided into 4 registeries as IFID, IDEX, EXMEM and MEMWB 
// where data exists and will be refrenced

#pragma once
#include <cstdint>

// Global Definitions
using word = uint16_t;
using byte = uint8_t;
static constexpr int MEM_SIZE = 65536;
inline bool debugTrace;
inline bool debugTraceInline;


// Includes values returned by BTB 
struct BTB_RET 
{
    byte counter = 0;
    word predictedTarget = 0;     // word that needs to go into PC if preditedTake is true
    bool HIT = false;             // This only mean a match was found in BTB (pcUpperCurrent == pcUpperBTB)
    bool predictedTaken = false;  // If the match was found then (predictedTaken = HIT && counterMSB)
    bool valid = false;
};


// ====================================================== IF-ID ==================================================================
// Data handed from IF(Instruction Fetch) to ID(Instruction Decode) Stages + Everything IF knows about its predictions, since
// ID will determine wether that prediction was correct or not


struct IFID_REG
{
    byte counter = 0;
    word instruction = 0;        // current 16-bit instruction, instruction = 0 incase of flushes
    word pc = 0;                 // current PC stored in PC_REG
    word pcNext = 0;             // Next PC PC_REG will take i.e pc++
    word predictedTarget = 0;    // The target branch PC provided by Branch Target Buffer
    bool btbHit = false;         // Matching target found
    bool predictedTaken = false; // Branch is taken or not
};


// ====================================================== ID-EX ==================================================================
// Data handed from ID to EX(Execute). This is the widest struct carrying Instructional identities that is already decoded 
// in this stage, so EX/MEM/WB don't need to decode opcodes again; in-short decode and let control signals of each signal ride
// forward. Registers tag such RD, RS1, RS2 need to ride forward too, for forwarding as well as hazard detection
struct IDEX_REG
{
    // Register Tags corresponding to each instruction
    byte RD = 0, RS1 = 0, RS2 = 0;

    // Immediates passed on to ALU or EXEC Multiplexer to select RS2_VALUE or these Immediates in their place
    word IMM = 0, LUI_IMM = 0, pcNext = 0;

    // Values of registers returned by the GPR(General Purpose Register)
    word RS1_VALUE = 0, RS2_VALUE = 0;

    // ---------------------------------------------- Control Signals -----------------------------------------------------------
    // 
    byte ALUSrc = 0;  // RS1 = 0, IMM = 1, LUI_IMM = 2, pcNext = 3 
    byte ALUOp = 0;   // ALU_ADD = 0, ALU_MUL = 1, ALU_XOR = 2, ALU_AND = 3, ALU_OR = 4, ALU_SLT = 5, ALU_PASSTHROUGH = 6, ALU_SUB = 7
    bool memRead = false, memWrite = false, spType = false; // spType tells if an instruction is SP related or not 
    bool regWrite = false, memToReg = false; // memToReg tells if RAM output will be stored or ALU calculated value

    // Control-Signals used in ID are not forwarded, hence are not included here. isJump, isBranch, spSel
};

// ====================================================== EX-MEM ==================================================================
// Data handed from EX stage to MEM stage, there are two main values forwarded here. aluResult and storeValue (RS2_VAL),
// NOTE that they are not competing with each other aluResult is the value to be stored in nonMEM instruction and address
// in MEM related operation. Going forward storeValue(RS2_VAL) will be dropped. Since, ALU also handles SP calculations and
// our stack gorws downwards we need pre-decrement and post-imcrement aproach we will also forward spOld from ALU incase we are 
// pushing into our stack 

struct EXMEM_REG
{
    word aluResult = 0, storeValue = 0, spOld = 0;
    byte RD = 0, RS2 = 0; // RS2 tag for forwarding.
    bool memRead = false, memWrite = false, spType = false; // spType && memWrite = PUSH, spType && memRead = POP
    bool regWrite = false, memToReg = false;

};

// ====================================================== MEM-WB ==================================================================
// Cluster going from MEM to WB(Write Back) Unit. Thereare two physical values riding here Output from the RAM and ALU output.
// INNASE you use a load operation data from ALU will be adress and RAM output will be the data stored memToReg defines it

struct MEMWB_REG
{
    word aluResult = 0, ramOut = 0;
    byte RD = 0;
    bool spType = false; // For writing to SP register
    bool regWrite = false, memToReg = false;
    
};

//======================================================= Hazard & Forwarding ======================================================

// There are three hazards that can occour, since we are resolving branches in Decoder stage branchLoadStall (2 cycle-Stall), 
// branchArithStall (1-cycle) and load use Stall is only one cycle STORE, PUSH also stalls one cycle for now. If an instruction in EXEC
// is a load and the instruction behind it needs that value it's exerted. 
struct HazardResult
{
    bool loadUseStall = false;
    bool branchArithStall = false;
    bool branchLoadStall = false;
    bool stall = false; // OR of every-other computed only once each cycle
};

// Results Forwarded from EXMEM, MEMWB registers.
struct ForwardResult
{
    byte RD;
    bool regWrite = false;
    bool spWrite = false;
    word value = 0;
};

