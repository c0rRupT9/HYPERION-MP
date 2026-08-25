#pragma once

#include <cstdio>
#include <fstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <charconv>
#include <array>
#include <string_view>

namespace fs = std::filesystem;

class TraceWriter
{
public:
    explicit TraceWriter(size_t depth_words = 65000)
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
            auto stem = entry.path().stem().string();
            auto suffix = entry.path().extension().string();
            if (suffix != ext)
                continue;
            if (stem.empty() || !std::all_of(stem.begin(), stem.end(), ::isdigit))
                continue;
            long long n = std::stoll(stem);
            if (n > best)
                best = n;
        }

        long long next = best + 1;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%0*lld", width, next);
        return (fs::path(dir) / (std::string(buf) + ext)).string();
    }

    void record(uint16_t instr, bool stall, bool flush)
    {
        uint32_t raw = static_cast<uint32_t>(instr);
        if (stall)
            raw |= (1u << 16);
        if (flush)
            raw |= (1u << 17);
        words_.push_back(raw);
    }

    void write(const std::string &path) const
    {
        std::ofstream out(path);
        if (!out.is_open())
            return;

        out << "v3.0 hex words addressed\n";

        const size_t WORDS_PER_LINE = 8;
        size_t total = depth_;

        for (size_t addr = 0; addr < total; addr += WORDS_PER_LINE)
        {
            for (size_t j = addr; j < addr + WORDS_PER_LINE && j < total; ++j)
            {
                uint32_t w = (j < words_.size()) ? words_[j] : 0u;
                out << " " << hex_field(w, 5);
            }
            out << "\n";
        }
    }

    static std::vector<word> loadHexFile(const std::string &filename)
    {
        std::vector<word> program;
        std::ifstream file(filename);

        if (!file.is_open())
        {
            std::cerr << "Error: Could not open file '" << filename << "'\n";
            exit(1);
        }

        std::string line;
        int lineNumber = 0;

        while (std::getline(file, line))
        {
            lineNumber++;
            std::stringstream ss(line);
            std::string token;
            while (ss >> token)
            {
                try
                {
                    unsigned long val = std::stoul(token, nullptr, 16);
                    program.push_back(static_cast<word>(val));
                }
                catch (const std::exception &)
                {
                    std::cerr << "Warning: Invalid hex token '" << token
                              << "' at line " << lineNumber << " - skipping.\n";
                }
            }
        }

        return program;
    }

    static std::string dumpBtb(const BTB &btb)
    {
        std::string oss;
        oss.reserve(2048);
        oss += "\n=================================== BTB STATE ===================================\n";
        oss += "| PC/Row  | Target      | Counter   | Prediction |\n";
        oss += "+---------+-------------+-----------+------------+\n";

        bool entriesFound = false;

        for (int idx = 0; idx <= 31; idx++)
        {
            BTB_RET ret = btb.searchRows(idx);
            if (ret.valid)
            {
                entriesFound = true;
                std::string_view status = (ret.counter >= 2) ? "TAKEN" : "NOT TAKEN";

                char rowBuf[64];
                int len = std::snprintf(rowBuf, sizeof(rowBuf), "| %-7d | %-11u | %-9d | %-10s |\n",
                                        idx, ret.predictedTarget, static_cast<int>(ret.counter), status.data());
                oss.append(rowBuf, len);
            }
        }

        if (!entriesFound)
            oss += "|                        [BTB contains no valid entries]                       |\n";

        oss += "=================================================================================\n";
        return oss;
    }

    static std::string dumpMem(const std::array<word, MEM_SIZE> &mem)
    {
        std::string oss;
        oss.reserve(2048);
        oss += "\n=================================== MEM STATE ===================================\n"
               "| Row     | Value   |\n"
               "+---------+---------+\n";

        bool hasData = false;

        for (size_t idx = 0; idx < MEM_SIZE; ++idx)
        {
            if (mem[idx] != 0)
            {
                hasData = true;
                char rowBuf[64];
                int len = std::snprintf(rowBuf, sizeof(rowBuf), "| %-7zu | %-7u |\n", idx, mem[idx]);
                oss.append(rowBuf, len);
            }
        }

        if (!hasData)
        {
            oss += "|                   [All Memory Locations are 0]                  |\n";
        }

        oss += "=================================================================================\n";
        return oss;
    }

    std::string dumpTrace(size_t cycle, bool stall, const IDEX_REG &idexCurr,
                          const IFID_REG &, const EXMEM_REG &exmemCurr, word pc, word instruction)
    {
        std::string oss;
        oss.reserve(200);

        oss += "--- CYCLE ";
        char numBuf[16];
        auto [p1, _] = std::to_chars(numBuf, numBuf + sizeof(numBuf), cycle);
        oss.append(numBuf, p1 - numBuf);

        oss += " [PC=";
        auto [p2, __] = std::to_chars(numBuf, numBuf + sizeof(numBuf), pc);
        oss.append(numBuf, p2 - numBuf);
        oss += "] ";
        if (stall)
            oss += "<< STALL >>";
        oss += " ---\n";

        oss += "  IF/ID : instr = " + disassembleInstruction(instruction) + "\n";

        oss += "  ID/EX : RS1=" + std::to_string(idexCurr.RS1) +
               " RS2=" + std::to_string(idexCurr.RS2) +
               " RD=" + std::to_string(idexCurr.RD) +
               " regWrite=" + std::to_string(idexCurr.regWrite) +
               " aluSrc=" + std::to_string(idexCurr.ALUSrc) + "\n";

        oss += "  EX/MEM: RD=" + std::to_string(exmemCurr.RD) +
               " regWrite=" + std::to_string(exmemCurr.regWrite) +
               " memRead=" + std::to_string(exmemCurr.memRead) +
               " memWrite=" + std::to_string(exmemCurr.memWrite) + "\n";

        oss += "------------------------------------------\n";
        return oss;
    }

private:
    static std::string hex_field(uint64_t val, int width)
    {
        char buf[16];
        auto [ptr, __ec] = std::to_chars(buf, buf + sizeof(buf), val, 16);
        size_t len = ptr - buf;

        if (len < static_cast<size_t>(width))
        {
            std::string res(width, '0');
            std::copy(buf, ptr, res.end() - len);
            return res;
        }
        return std::string(buf, len);
    }

    std::string disassembleInstruction(word instruction)
    {
        static ID dec;
        auto out = dec.decoder(instruction);
        word opCode = (out.tag << 3) | out.subOp;

        char regBuf[8];
        char immBuf[16];

        auto formatReg = [&](uint8_t reg)
        {
            auto [ptr, __ec] = std::to_chars(regBuf, regBuf + sizeof(regBuf), reg);
            return std::string_view(regBuf, ptr - regBuf);
        };
        auto formatImm = [&](int16_t imm)
        {
            auto [ptr, __ec] = std::to_chars(immBuf, immBuf + sizeof(immBuf), imm);
            return std::string_view(immBuf, ptr - immBuf);
        };

        std::string result;
        result.reserve(64);
        result = InstructionString[opCode];

        switch (out.tag)
        {
        case 0b00:
            result += " RD: R";
            result += formatReg(out.RD);
            result += " RS1: R";
            result += formatReg(out.RS1);
            result += " RS2: R";
            result += formatReg(out.RS2);
            break;

        case 0b01:
            result += " RD: R";
            result += formatReg(out.RD);
            result += " RS1: R";
            result += formatReg(out.RS1);
            result += " Imm: ";
            result += formatImm(static_cast<int16_t>(out.imm));
            break;

        case 0b10:
            result += " RS1: R";
            result += formatReg(out.RS1);
            result += " RS2: R";
            result += formatReg(out.RS2);
            result += " Imm: ";
            result += formatImm(static_cast<int16_t>(out.imm));
            break;

        case 0b11:
            if (opCode == 0b1101)
            {
                result += " RS2: R";
                result += formatReg(out.RD);
                result += " Imm: ";
                result += formatImm(static_cast<int16_t>(out.imm));
            }
            else
            {
                result += " RD: R";
                result += formatReg(out.RD);
                result += " Imm: ";
                result += formatImm(static_cast<int16_t>(out.imm));
            }
            break;
        }

        return result;
    }

    inline static const std::array<std::string_view, 32> InstructionString = {
        "NOP", "ADD", "MUL", "XOR", "AND", "OR", "SLT", "RES",
        "RES", "ADDI", "MULI", "XORI", "ANDI", "LOAD", "SLTI", "JALR", "BGEU", "BNE", "BLU", "BGE", "BEQ", "BLT", "STORE",
        "RES", "JAL", "LUI", "MOVI", "PUSH", "POP", "RES", "RES", "RES"};

    size_t depth_;
    std::vector<uint32_t> words_;
};