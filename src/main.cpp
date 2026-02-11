#include "assembler.h"
#include "pdp11.h"
#include "disasm.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace pdp11;

static uint16_t parse_u16(const std::string& s) {
    int base = 10;
    std::string t = s;
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) {
        base = 16;
        t = t.substr(2);
    } else if (t.rfind("0o", 0) == 0 || t.rfind("0O", 0) == 0) {
        base = 8;
        t = t.substr(2);
    }
    return static_cast<uint16_t>(std::stoul(t, nullptr, base));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: pdp11sim <file.asm> [max_steps] [--trace] [--trace-mem] [--watch=addr[:len]] [--map file] [--dump-symbols]\n";
        return 1;
    }

    std::string path = argv[1];
    uint64_t max_steps = 100000;
    bool trace = false;
    bool trace_mem = false;
    bool dump_symbols = false;
    std::string map_path;
    bool watch_enabled = false;
    uint16_t watch_start = 0;
    uint16_t watch_end = 0;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace") {
            trace = true;
            continue;
        }
        if (arg == "--trace-mem") {
            trace_mem = true;
            continue;
        }
        if (arg == "--dump-symbols") {
            dump_symbols = true;
            continue;
        }
        if (arg.rfind("--map", 0) == 0) {
            auto pos = arg.find('=');
            if (pos != std::string::npos) {
                map_path = arg.substr(pos + 1);
            } else if (i + 1 < argc) {
                map_path = argv[++i];
            }
            continue;
        }
        if (arg.rfind("--watch", 0) == 0) {
            auto pos = arg.find('=');
            std::string spec;
            if (pos != std::string::npos) {
                spec = arg.substr(pos + 1);
            } else if (i + 1 < argc) {
                spec = argv[++i];
            }
            if (!spec.empty()) {
                auto colon = spec.find(':');
                if (colon == std::string::npos) {
                    watch_start = parse_u16(spec);
                    watch_end = watch_start;
                } else {
                    watch_start = parse_u16(spec.substr(0, colon));
                    uint16_t len = parse_u16(spec.substr(colon + 1));
                    watch_end = static_cast<uint16_t>(watch_start + (len == 0 ? 0 : len - 1));
                }
                watch_enabled = true;
            }
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
        cpu.mem_watch.enabled = watch_enabled;
        cpu.mem_watch.trace_all = trace_mem;
        cpu.mem_watch.start = watch_start;
        cpu.mem_watch.end = watch_end;

        if (dump_symbols || !map_path.empty()) {
            if (dump_symbols) {
                for (const auto& kv : res.symbols) {
                    std::cout << "0x" << std::hex << kv.second << std::dec << " " << kv.first << "\n";
                }
            }
            if (!map_path.empty()) {
                std::ofstream out(map_path);
                if (!out) {
                    throw std::runtime_error("Failed to open map file: " + map_path);
                }
                for (const auto& kv : res.symbols) {
                    out << "0x" << std::hex << kv.second << std::dec << " " << kv.first << "\n";
                }
            }
        }
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
