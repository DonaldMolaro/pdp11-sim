#include "assembler.h"
#include "pdp11.h"
#include "disasm.h"

#include <fstream>
#include <iostream>

using namespace pdp11;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: pdp11sim <file.asm> [max_steps] [--trace]\n";
        return 1;
    }

    std::string path = argv[1];
    uint64_t max_steps = 100000;
    bool trace = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace") {
            trace = true;
            continue;
        }
        max_steps = static_cast<uint64_t>(std::stoull(arg));
    }

    try {
        Assembler asmblr;
        AsmResult res = asmblr.assemble_file(path);

        CPU cpu;
        cpu.reset();
        cpu.r[7] = res.start;
        cpu.r[6] = 0xFFFE; // stack grows down
        cpu.load_words(res.start, res.words);
        if (trace) {
            for (uint64_t i = 0; i < max_steps && !cpu.halted; ++i) {
                uint16_t pc = cpu.r[7];
                std::cout << "PC=" << std::hex << pc << std::dec
                          << "  " << disassemble(cpu, pc) << "\n";
                cpu.step();
            }
        } else {
            cpu.run(max_steps);
        }

        std::cout << "HALT=" << (cpu.halted ? "yes" : "no") << "\n";
        for (int i = 0; i < 8; ++i) {
            std::cout << "R" << i << "=" << std::hex << cpu.r[i] << std::dec << "\n";
        }
        std::cout << "N=" << cpu.psw.n << " Z=" << cpu.psw.z << " V=" << cpu.psw.v << " C=" << cpu.psw.c << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }

    return 0;
}
