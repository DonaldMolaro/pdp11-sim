#include "assembler.h"
#include "pdp11.h"
#include "disasm.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

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

static std::string upper(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return out;
}

struct TraceSnapshot {
    uint16_t pc = 0;
    std::string instr;
    uint16_t regs[8]{};
    bool n = false;
    bool z = false;
    bool v = false;
    bool c = false;
    uint8_t bank = 0;
    std::string stop_reason;
};

static std::string js_escape(const std::string& s) {
    std::ostringstream out;
    for (char ch : s) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec;
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

static void capture_snapshot(const CPU& cpu, uint16_t pc, std::vector<TraceSnapshot>* snapshots) {
    if (!snapshots) {
        return;
    }
    TraceSnapshot snap;
    snap.pc = pc;
    snap.instr = disassemble(cpu, pc);
    for (int i = 0; i < 8; ++i) {
        snap.regs[i] = cpu.r[i];
    }
    snap.n = cpu.psw.n;
    snap.z = cpu.psw.z;
    snap.v = cpu.psw.v;
    snap.c = cpu.psw.c;
    snap.bank = cpu.mem_bank;
    snapshots->push_back(snap);
}

static void write_trace_html(const std::string& path, const std::vector<TraceSnapshot>& snapshots) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open HTML trace file: " + path);
    }

    out << "<!doctype html>\n"
        << "<html><head><meta charset=\"utf-8\">"
        << "<title>PDP-11 Trace</title>"
        << "<style>"
        << "body{font-family:Menlo,Consolas,monospace;background:#111827;color:#e5e7eb;margin:0;padding:24px;}"
        << ".wrap{max-width:1100px;margin:0 auto;}"
        << ".hero{display:flex;justify-content:space-between;gap:24px;align-items:end;margin-bottom:20px;}"
        << "h1{margin:0;font-size:28px;} .muted{color:#9ca3af;}"
        << ".card{background:#1f2937;border:1px solid #374151;border-radius:16px;padding:18px;margin-bottom:18px;}"
        << ".row{display:flex;gap:16px;align-items:center;flex-wrap:wrap;}"
        << ".regs{display:grid;grid-template-columns:repeat(4,minmax(120px,1fr));gap:10px;margin-top:12px;}"
        << ".reg,.flag{background:#0f172a;border-radius:12px;padding:10px 12px;border:1px solid #334155;}"
        << ".flags{display:grid;grid-template-columns:repeat(5,minmax(80px,1fr));gap:10px;margin-top:12px;}"
        << "input[type=range]{width:100%;} .instr{font-size:22px;color:#93c5fd;} .pc{color:#fca5a5;}"
        << "table{width:100%;border-collapse:collapse;} td{padding:6px 0;border-bottom:1px solid #273449;}"
        << "</style></head><body><div class=\"wrap\">"
        << "<div class=\"hero\"><div><h1>PDP-11 Machine Trace</h1>"
        << "<div class=\"muted\">Interactive snapshot viewer for simulator state</div></div>"
        << "<div class=\"muted\">Steps: " << snapshots.size() << "</div></div>"
        << "<div class=\"card\"><div class=\"row\"><strong>Step</strong> <span id=\"stepLabel\">0</span></div>"
        << "<input id=\"step\" type=\"range\" min=\"0\" max=\"" << (snapshots.empty() ? 0 : snapshots.size() - 1)
        << "\" value=\"0\"></div>"
        << "<div class=\"card\"><div class=\"pc\" id=\"pc\"></div><div class=\"instr\" id=\"instr\"></div>"
        << "<div class=\"regs\" id=\"regs\"></div><div class=\"flags\" id=\"flags\"></div></div>"
        << "<div class=\"card\"><table><tbody id=\"detail\"></tbody></table></div>"
        << "<script>const trace=[";

    for (size_t i = 0; i < snapshots.size(); ++i) {
        const auto& s = snapshots[i];
        if (i != 0) {
            out << ",";
        }
        out << "{pc:" << s.pc
            << ",instr:\"" << js_escape(s.instr) << "\""
            << ",stop:\"" << js_escape(s.stop_reason) << "\""
            << ",bank:" << static_cast<unsigned>(s.bank)
            << ",n:" << (s.n ? "true" : "false")
            << ",z:" << (s.z ? "true" : "false")
            << ",v:" << (s.v ? "true" : "false")
            << ",c:" << (s.c ? "true" : "false")
            << ",regs:[";
        for (int r = 0; r < 8; ++r) {
            if (r != 0) {
                out << ",";
            }
            out << s.regs[r];
        }
        out << "]}";
    }

    out << "];"
        << "const hex=v=>'0x'+v.toString(16).padStart(4,'0');"
        << "const slider=document.getElementById('step');"
        << "const stepLabel=document.getElementById('stepLabel');"
        << "function render(){const i=Number(slider.value);const s=trace[i];if(!s){return;}"
        << "stepLabel.textContent=i;"
        << "document.getElementById('pc').textContent='PC '+hex(s.pc)+'  Bank '+s.bank;"
        << "document.getElementById('instr').textContent=s.stop ? 'Stopped: '+s.stop : s.instr;"
        << "document.getElementById('regs').innerHTML=s.regs.map((v,idx)=>`<div class=\"reg\">R${idx}: ${hex(v)}</div>`).join('');"
        << "document.getElementById('flags').innerHTML="
           "[['N',s.n],['Z',s.z],['V',s.v],['C',s.c],['BANK',s.bank]].map(([k,v])=>`<div class=\"flag\">${k}: ${v}</div>`).join('');"
        << "document.getElementById('detail').innerHTML="
           "`<tr><td>Step</td><td>${i}</td></tr><tr><td>Instruction / Stop</td><td>${s.stop || s.instr}</td></tr><tr><td>SP</td><td>${hex(s.regs[6])}</td></tr><tr><td>PC at Snapshot</td><td>${hex(s.regs[7])}</td></tr>`;}"
        << "slider.addEventListener('input',render);render();"
        << "</script></div></body></html>\n";
}

static void dump_cpu_state(const CPU& cpu, uint16_t pc, bool include_instr) {
    if (include_instr) {
        std::cout << "PC=0x" << std::hex << std::setw(4) << std::setfill('0') << pc
                  << std::dec << "  " << disassemble(cpu, pc) << "\n";
    }
    std::cout << "STATE"
              << " R0=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.r[0]
              << " R1=0x" << std::setw(4) << cpu.r[1]
              << " R2=0x" << std::setw(4) << cpu.r[2]
              << " R3=0x" << std::setw(4) << cpu.r[3]
              << " R4=0x" << std::setw(4) << cpu.r[4]
              << " R5=0x" << std::setw(4) << cpu.r[5]
              << " SP=0x" << std::setw(4) << cpu.r[6]
              << " PC=0x" << std::setw(4) << cpu.r[7]
              << std::dec
              << " N=" << cpu.psw.n
              << " Z=" << cpu.psw.z
              << " V=" << cpu.psw.v
              << " C=" << cpu.psw.c
              << "\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: pdp11sim <file.asm> [max_steps] [--trace] [--trace-state] [--trace-html file] [--trace-mem] [--watch=addr[:len]] [--map file] [--dump-symbols] [--break=label|0xADDR]\n";
        return 1;
    }

    std::string path = argv[1];
    uint64_t max_steps = 100000;
    bool trace = false;
    bool trace_state = false;
    std::string trace_html_path;
    bool trace_mem = false;
    bool dump_symbols = false;
    std::string map_path;
    bool watch_enabled = false;
    uint16_t watch_start = 0;
    uint16_t watch_end = 0;
    std::vector<std::string> break_specs;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace") {
            trace = true;
            continue;
        }
        if (arg == "--trace-state") {
            trace_state = true;
            continue;
        }
        if (arg.rfind("--trace-html", 0) == 0) {
            auto pos = arg.find('=');
            if (pos != std::string::npos) {
                trace_html_path = arg.substr(pos + 1);
            } else if (i + 1 < argc) {
                trace_html_path = argv[++i];
            }
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
        if (arg.rfind("--break", 0) == 0) {
            auto pos = arg.find('=');
            std::string spec;
            if (pos != std::string::npos) {
                spec = arg.substr(pos + 1);
            } else if (i + 1 < argc) {
                spec = argv[++i];
            }
            if (!spec.empty()) {
                break_specs.push_back(spec);
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

        if (!break_specs.empty()) {
            for (const auto& spec : break_specs) {
                bool is_num = !spec.empty() && (std::isdigit(static_cast<unsigned char>(spec[0])) ||
                                                spec.rfind("0x", 0) == 0 || spec.rfind("0X", 0) == 0 ||
                                                spec.rfind("0o", 0) == 0 || spec.rfind("0O", 0) == 0);
                if (is_num) {
                    cpu.breakpoints.insert(parse_u16(spec));
                } else {
                    auto it = res.symbols.find(upper(spec));
                    if (it == res.symbols.end()) {
                        throw std::runtime_error("Unknown breakpoint label: " + spec);
                    }
                    cpu.breakpoints.insert(it->second);
                }
            }
        }

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
        std::vector<TraceSnapshot> html_snapshots;
        std::vector<TraceSnapshot>* html_trace = trace_html_path.empty() ? nullptr : &html_snapshots;

        if (trace || trace_state || html_trace) {
            for (uint64_t i = 0; i < max_steps && !cpu.halted; ++i) {
                uint16_t pc = cpu.r[7];
                if (!cpu.breakpoints.empty() && cpu.breakpoints.find(pc) != cpu.breakpoints.end()) {
                    cpu.break_hit = true;
                    cpu.break_addr = pc;
                    break;
                }
                capture_snapshot(cpu, pc, html_trace);
                if (trace) {
                    std::cout << "PC=" << std::hex << pc << std::dec
                              << "  " << disassemble(cpu, pc) << "\n";
                }
                if (trace_state) {
                    dump_cpu_state(cpu, pc, !trace);
                }
                cpu.step();
            }
        } else {
            cpu.run(max_steps);
        }

        if (cpu.break_hit) {
            std::cout << "BREAK at 0x" << std::hex << cpu.break_addr << std::dec << "\n";
            if (trace_state) {
                dump_cpu_state(cpu, cpu.break_addr, !trace);
            }
        }
        if (html_trace) {
            capture_snapshot(cpu, cpu.r[7], html_trace);
            html_snapshots.back().stop_reason = cpu.halted ? "HALT" :
                (cpu.break_hit ? "breakpoint" : "step limit");
            write_trace_html(trace_html_path, html_snapshots);
            std::cout << "TRACE_HTML=" << trace_html_path << "\n";
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
