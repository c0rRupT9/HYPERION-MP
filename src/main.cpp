#include "CPU.hpp"
#include <vector>
struct Config
{
    std::string input = "";
    bool debug = false, debugTraceInline = false;
    int cycles = 100; // default value
};

Config parse(int argc, char *argv[])
{
    Config conf{};

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "-d" || arg == "--debug")
        {
            conf.debug = true;
        }
        else if (arg == "-t" || arg == "--trace")
        {
            conf.debugTraceInline = true;
        }
        else if (arg == "-c" || arg == "--cycles")
        {
            if (i + 1 < argc)
            {
                try
                {
                    conf.cycles = std::stoi(argv[++i]);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error: Invalid cycle count value: " << argv[i] << "\nDefaulting to: " << conf.cycles << " cycles\n";
                }
            }
            else
            {
                std::cerr << "Error: Option " << arg << " requires a cycle count value. Defulting to: " << conf.cycles << " cycles\n";
            }
        }

        else if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " <program.hex> [options]\n"
                      << "Options:\n"
                      << "  -d, --debug        Enable debug assertions/state dumps\n"
                      << "  -t, --trace        Enable cycle-by-cycle stage logging\n"
                      << "  -c, --cycles [int] Number of cycles to provide \n";
            exit(0);
        }
        else if (arg[0] != '-')
        {
            conf.input = arg;
        }
    }
    return conf;
}


int main(int argc, char *argv[])
{
    Config conf = parse(argc, argv);
    debugTrace = conf.debug;
    debugTraceInline = conf.debugTraceInline;

    if (conf.input.empty())
    {
        std::cerr << "Error: No input hex file provided.\n";
        std::cerr << "Usage: " << argv[0] << " <program.hex> [options]\n";
        return 1;
    }

    std::vector<word> program = TraceWriter::loadHexFile(conf.input);
    if (program.empty())
    {
        std::cerr << "Provided file is Empty.\n";
        return 1;
    }
    risc::CPU cpu;
    cpu.loadProgram(program);
    cpu.run(conf.cycles);
    return 0;
}