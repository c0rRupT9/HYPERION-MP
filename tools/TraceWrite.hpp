
#pragma once

// Packs per-cycle {DEC_instr[15:0], Stall[1], Flush[1]} into an 18-bit word
// and writes it out in Logisim's "v3.0 hex words addressed" RAM image format
// byte-for-byte compatible with what trace_reconstruct.py expects to read
// back in (see unpack_row() in that script):
//
//     instr = raw & 0xFFFF
//     stall = (raw >> 16) & 0b1
//     flush = (raw >> 17) & 0b1
//
#include <cstdio>
#include <fstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
class TraceWriter
{
public:
    explicit TraceWriter(size_t depth_words = 65000) // 256 is enough for study purpose, it hogs CPU juice
        : depth_(depth_words)
    {
        words_.reserve(depth_words);
    }

    std::string next_numbered_path(const std::string &dir,
                                   const std::string &ext = ".txt",
                                   int width = 4)
    {
        fs::create_directories(dir);

        long long best = -1;
        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file())
                continue;
            auto stem = entry.path().stem().string();        // filename without extension
            auto suffix = entry.path().extension().string(); // Extention
            if (suffix != ext)
                continue;
            if (stem.empty() || !std::all_of(stem.begin(), stem.end(), ::isdigit)) // if filename is not all digits skip
                continue;
            long long n = std::stoll(stem); //  convert to long long
            if (n > best)
                best = n;
        }

        long long next = best + 1;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%0*lld", width, next);
        return (fs::path(dir) / (std::string(buf) + ext)).string();
    }

    // Call once per clock cycle, right after DEC latches this cycle's values.
    // stall: DEC/EX stall signal this cycle.
    // flush: IF/DEC flush signal this cycle.
    void record(uint16_t instr, bool stall, bool flush)
    {
        uint32_t raw = static_cast<uint32_t>(instr);
        if (stall)
            raw |= (1u << 16);
        if (flush)
            raw |= (1u << 17);
        words_.push_back(raw);
    }

    // Write the accumulated trace to a Logisim v3.0 hex words addressed file.
    // Pads with zero words up to depth_words if the recorded trace is shorter.
    void write(const std::string &path) const
    {
        std::ofstream out(path);
        out << "v3.0 hex words addressed\n";

        const size_t WORDS_PER_LINE = 8;
        size_t total = depth_;

        for (size_t addr = 0; addr < total; addr += WORDS_PER_LINE)
        {
            for (size_t j = addr; j < addr + WORDS_PER_LINE && j < total; ++j)
            {
                uint32_t w = (j < words_.size()) ? words_[j] : 0u; // Pushes 0
                out << " " << hex_field(w, 5);
            }
            out << "\n";
        }
    }

    static std::string dumpBtb(const BTB &btb)
    {
        std::ostringstream oss;
        oss << "\n=================================== BTB STATE ===================================\n";
        oss << "| " << std::left << std::setw(8) << "PC/Row"
            << "| " << std::left << std::setw(12) << "Target"
            << "| " << std::left << std::setw(10) << "Counter"
            << "| " << std::left << std::setw(12) << "Prediction"
            << "| " << std::left << std::setw(12) << "BranchType" << " |\n";
        oss << "+---------+-------------+-----------+--------------+------------+\n";

        bool entriesFound = false;

        for (int idx = 0; idx <= 31; idx++)
        {
            BTB_RET ret = btb.searchRows(idx);
            if (ret.valid)
            {
                entriesFound = true;

                // 2-bit counter: >= 2 (10, 11) means TAKEN, < 2 (00, 01) means NOT TAKEN
                std::string status = (ret.counter >= 2) ? "TAKEN" : "NOT TAKEN";

                oss << "| " << std::left << std::setw(8) << idx
                    << "| " << std::left << std::setw(12) << ret.predictedTarget
                    << "| " << std::left << std::setw(10) << static_cast<int>(ret.counter)
                    << "| " << std::left << std::setw(12) << status
                    << "| " << std::left << std::setw(12) << (int)ret.branchType << " |\n";
            }
        }

        if (!entriesFound)
            oss << "|                        [BTB contains no valid entries]                       |\n";

        oss << "=================================================================================\n";

        return oss.str();
    }

    static std::string dumpMem(const std::array<word, MEM_SIZE> &mem)
    {

        std::ostringstream oss;

        oss << "\n=================================== MEM STATE ===================================\n"
            << "| " << std::left << std::setw(8) << "Row"
            << "| " << std::left << std::setw(8) << "Value" << "|\n"
            << "+---------+---------+\n";

        bool hasData = false;

        for (size_t idx = 0; idx < MEM_SIZE; ++idx)
        {
            if (mem[idx] != 0)
            {
                hasData = true;
                oss << "| " << std::left << std::setw(8) << idx
                    << "| " << std::left << std::setw(8) << mem[idx] << "|\n";
            }
        }

        if (!hasData)
        {
            oss << "|                  [All Memory Locations are 0]                  |\n";
        }

        oss << "=================================================================================\n";

        return oss.str();
    }

    std::string dumpTrace(size_t &cycle, const bool &stall, IDEX_REG &idexCurr,
                                 const IFID_REG &ifidCurr, const EXMEM_REG &exmemCurr, const word &pc, const word& instrction)
    {
        std::ostringstream oss;
        oss << "--- CYCLE " << std::setw(2) << cycle
            << " [PC=" << std::setw(2) << pc
            << "] " << (stall ? "<< STALL >>" : "") << " ---\n";
        oss << "  IF/ID : instr = " << Instruction(instrction) << "\n";
        oss << "  ID/EX : RS1=" << (int)idexCurr.RS1
            << " RS2=" << (int)idexCurr.RS2
            << " RD=" << (int)idexCurr.RD
            << " regWrite=" << idexCurr.regWrite
            << " aluSrc=" << (int)idexCurr.ALUSrc << "\n";
        oss << "  EX/MEM: RD=" << (int)exmemCurr.RD
            << " regWrite=" << exmemCurr.regWrite << " memRead=" << (int)exmemCurr.memRead
            << " memWrite=" << exmemCurr.memWrite << "\n";
        oss << "------------------------------------------\n";
        return oss.str();
    }

private:
    static std::string hex_field(uint64_t val, int width)
    {
        std::ostringstream ss;
        ss << std::hex << std::nouppercase << std::setw(width) << std::setfill('0') << val;
        return ss.str();
    }

     std::string Instruction(const word& instruction)
    {
        ID dec;
        std::string Rd, Rs1, Rs2, Imm;
        auto out = dec.decoder(instruction);
        word opCode = (out.tag << 3) | (out.subOp);
        if (out.tag == 0b00)
        {
            Rd = " RD: R" + std::to_string((int)out.RD);
            Rs1 = " RS1: R" + std::to_string((int)out.RS1);
            Rs2 = " RS2: R" + std::to_string((int)out.RS2);
            Imm = "";
        }
        if (out.tag == 0b01)
        {
            Rd = " RD: R" + std::to_string((int)out.RD);
            Rs1 = " RS1: R" + std::to_string((int)out.RS1);
            Rs2 = "";
            Imm = " Imm: " + std::to_string((int)((int16_t)out.imm));
        }
        if (out.tag == 0b10)
        {
            Rd = "";
            Rs1 = " RS1: R" + std::to_string((int)out.RS1);
            Rs2 = " RS2: R" + std::to_string((int)out.RS2);
            Imm = " Imm: " + std::to_string((int)((int16_t)out.imm));
        }
        if (out.tag == 0b11)
        {
            Rd = (opCode == 0b1101) ? "" : " RD: R" + std::to_string((int)out.RD);
            Rs1 = "";
            Rs2 = (opCode == 0b1101) ? " RS2: R" + std::to_string((int)out.RD) : "";
            Imm = " Imm: " + std::to_string((int)((int16_t)out.imm));
        }
        std::string instructionName = InstructionString[opCode] + Rd + Rs1 + Rs2 + Imm;
        return instructionName;
    }

    const std::array<std::string, 32> InstructionString = {"NOP", "ADD", "MUL", "XOR", "AND", "OR", "SLT", "RES",
                                                           "RES", "ADDI", "MULI", "XORI", "ANDI", "LOAD", "SLTI", "JALR", "BGEU", "BNE", "BLU", "BGE", "BEQ", "BLT", "STORE",
                                                           "RES", "JAL", "LUI", "MOVI", "PUSH", "POP", "RES", "RES", "RES"};
    size_t depth_;
    std::vector<uint32_t> words_;
};

// TREACEWRITE_HPP