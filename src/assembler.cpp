#include "assembler.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pdp11 {

static std::vector<std::string> split_operands(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

std::string Assembler::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string Assembler::upper(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return out;
}

bool Assembler::is_register(const std::string& token, uint16_t& reg) {
    if (token.size() == 2 && (token[0] == 'R' || token[0] == 'r') && token[1] >= '0' && token[1] <= '7') {
        reg = static_cast<uint16_t>(token[1] - '0');
        return true;
    }
    return false;
}

bool Assembler::is_number(const std::string& token, int32_t& value) {
    if (token.empty()) return false;
    try {
        value = parse_number(token);
        return true;
    } catch (...) {
        return false;
    }
}

int32_t Assembler::parse_number(const std::string& token) {
    std::string t = token;
    int sign = 1;
    if (!t.empty() && t[0] == '-') {
        sign = -1;
        t = t.substr(1);
    }

    int base = 8;
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) {
        base = 16;
        t = t.substr(2);
    } else if (t.rfind("0o", 0) == 0 || t.rfind("0O", 0) == 0) {
        base = 8;
        t = t.substr(2);
    } else {
        base = 10;
    }

    int32_t value = 0;
    for (char c : t) {
        int digit = 0;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
        else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
        else throw std::runtime_error("Invalid number: " + token);
        if (digit >= base) throw std::runtime_error("Invalid number: " + token);
        value = value * base + digit;
    }

    return sign * value;
}

std::vector<Assembler::Line> Assembler::parse_lines(const std::string& source) {
    std::vector<Line> lines;
    std::istringstream in(source);
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        std::string raw = line;
        auto comment_pos = line.find(';');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);
        if (line.empty()) continue;

        Line l;
        l.line_no = line_no;
        l.raw = raw;

        auto colon = line.find(':');
        if (colon != std::string::npos) {
            l.label = trim(line.substr(0, colon));
            line = trim(line.substr(colon + 1));
        }

        if (line.empty()) {
            lines.push_back(l);
            continue;
        }

        std::istringstream ss(line);
        ss >> l.opcode;
        if (l.opcode.empty()) {
            lines.push_back(l);
            continue;
        }
        l.opcode = upper(l.opcode);

        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);
        if (!rest.empty()) {
            auto ops = split_operands(rest);
            for (auto& op : ops) {
                l.operands.push_back(trim(op));
            }
        }

        lines.push_back(l);
    }

    return lines;
}

Assembler::OperandEnc Assembler::encode_operand(const std::string& token, uint16_t pc,
                                                const std::unordered_map<std::string, uint16_t>& symbols,
                                                bool allow_undefined) {
    OperandEnc enc{};
    std::string t = trim(token);

    if (t.empty()) {
        throw std::runtime_error("Empty operand");
    }

    uint16_t reg = 0;
    if (is_register(t, reg)) {
        enc.spec = reg; // mode 0
        return enc;
    }

    if (t.size() >= 3 && t.front() == '(' && t.back() == ')') {
        std::string inner = t.substr(1, t.size() - 2);
        if (!is_register(inner, reg)) {
            throw std::runtime_error("Invalid register deferred: " + t);
        }
        enc.spec = static_cast<uint16_t>((1 << 3) | reg);
        return enc;
    }

    if (t.size() >= 4 && t.front() == '(' && t.back() == '+' && t[t.size() - 2] == ')') {
        std::string inner = t.substr(1, t.size() - 3);
        if (!is_register(inner, reg)) {
            throw std::runtime_error("Invalid autoincrement: " + t);
        }
        enc.spec = static_cast<uint16_t>((2 << 3) | reg);
        return enc;
    }

    if (t.size() >= 4 && t.front() == '-' && t[1] == '(' && t.back() == ')') {
        std::string inner = t.substr(2, t.size() - 3);
        if (!is_register(inner, reg)) {
            throw std::runtime_error("Invalid autodecrement: " + t);
        }
        enc.spec = static_cast<uint16_t>((4 << 3) | reg);
        return enc;
    }

    if (t.rfind("#", 0) == 0) {
        std::string value = trim(t.substr(1));
        int32_t imm = 0;
        if (!is_number(value, imm)) {
            auto it = symbols.find(upper(value));
            if (it == symbols.end()) {
                if (!allow_undefined) {
                    throw std::runtime_error("Undefined symbol: " + value);
                }
                imm = 0;
            } else {
                imm = it->second;
            }
        }
        enc.spec = static_cast<uint16_t>((2 << 3) | 7); // autoinc PC
        enc.has_extra = true;
        enc.extra = imm;
        return enc;
    }

    if (t.rfind("@#", 0) == 0) {
        std::string value = trim(t.substr(2));
        int32_t imm = 0;
        if (!is_number(value, imm)) {
            auto it = symbols.find(upper(value));
            if (it == symbols.end()) {
                if (!allow_undefined) {
                    throw std::runtime_error("Undefined symbol: " + value);
                }
                imm = 0;
            } else {
                imm = it->second;
            }
        }
        enc.spec = static_cast<uint16_t>((3 << 3) | 7); // autoinc deferred PC
        enc.has_extra = true;
        enc.extra = imm;
        return enc;
    }

    auto paren = t.find('(');
    if (paren != std::string::npos && t.back() == ')') {
        std::string disp = trim(t.substr(0, paren));
        std::string inner = trim(t.substr(paren + 1, t.size() - paren - 2));
        if (!is_register(inner, reg)) {
            throw std::runtime_error("Invalid index: " + t);
        }
        int32_t value = 0;
        if (!disp.empty() && !is_number(disp, value)) {
            auto it = symbols.find(upper(disp));
            if (it == symbols.end()) {
                if (!allow_undefined) {
                    throw std::runtime_error("Undefined symbol: " + disp);
                }
                value = 0;
            } else {
                value = it->second;
            }
        }
        enc.spec = static_cast<uint16_t>((6 << 3) | reg);
        enc.has_extra = true;
        enc.extra = value;
        return enc;
    }

    // Symbol or number as PC-relative
    int32_t value = 0;
    if (!is_number(t, value)) {
        auto it = symbols.find(upper(t));
        if (it == symbols.end()) {
            if (!allow_undefined) {
                throw std::runtime_error("Undefined symbol: " + t);
            }
            value = 0;
        } else {
            value = it->second;
        }
    }
    enc.spec = static_cast<uint16_t>((6 << 3) | 7); // index PC
    enc.has_extra = true;
    enc.extra = value - static_cast<int32_t>(pc + 4); // PC after extension
    return enc;
}

uint16_t Assembler::encode_double_op(const std::string& opcode) {
    if (opcode == "MOV") return 0010000; // octal
    if (opcode == "CMP") return 0020000;
    if (opcode == "BIT") return 0030000;
    if (opcode == "BIC") return 0040000;
    if (opcode == "BIS") return 0050000;
    if (opcode == "ADD") return 0060000;
    if (opcode == "SUB") return 0160000;

    if (opcode == "MOVB") return 0110000;
    if (opcode == "CMPB") return 0120000;
    if (opcode == "BITB") return 0130000;
    if (opcode == "BICB") return 0140000;
    if (opcode == "BISB") return 0150000;
    return 0;
}

uint16_t Assembler::encode_single_op(const std::string& opcode) {
    if (opcode == "CLR") return 0005000;
    if (opcode == "INC") return 0005200;
    if (opcode == "DEC") return 0005300;
    if (opcode == "TST") return 0005700;
    if (opcode == "ROR") return 0006000;
    if (opcode == "ROL") return 0006100;
    if (opcode == "ASR") return 0006200;
    if (opcode == "ASL") return 0006300;
    if (opcode == "JMP") return 0000100;

    if (opcode == "CLRB") return 0105000;
    if (opcode == "INCB") return 0105200;
    if (opcode == "DECB") return 0105300;
    if (opcode == "TSTB") return 0105700;
    return 0;
}

AsmResult Assembler::assemble(const std::string& source) {
    auto lines = parse_lines(source);

    std::unordered_map<std::string, uint16_t> symbols;
    uint16_t pc = 0;
    uint16_t start = 0;

    for (const auto& line : lines) {
        if (!line.label.empty()) {
            symbols[upper(line.label)] = pc;
        }

        if (line.opcode.empty()) {
            continue;
        }

        if (line.opcode == ".ORIG") {
            if (line.operands.size() != 1) {
                throw std::runtime_error(".ORIG requires one operand");
            }
            int32_t value = parse_number(line.operands[0]);
            pc = static_cast<uint16_t>(value);
            start = pc;
            continue;
        }

        if (line.opcode == ".WORD") {
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        if (line.opcode == "HALT" || line.opcode == "RTS" || line.opcode == "TRAP") {
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        uint16_t base = encode_double_op(line.opcode);
        if (base != 0) {
            if (line.operands.size() != 2) {
                throw std::runtime_error("Expected two operands on line " + std::to_string(line.line_no));
            }
            auto src = encode_operand(line.operands[0], pc, symbols, true);
            auto dst = encode_operand(line.operands[1], pc + (src.has_extra ? 2 : 0), symbols, true);
            pc = static_cast<uint16_t>(pc + 2 + (src.has_extra ? 2 : 0) + (dst.has_extra ? 2 : 0));
            continue;
        }

        base = encode_single_op(line.opcode);
        if (base != 0) {
            if (line.operands.size() != 1) {
                throw std::runtime_error("Expected one operand on line " + std::to_string(line.line_no));
            }
            auto dst = encode_operand(line.operands[0], pc, symbols, true);
            pc = static_cast<uint16_t>(pc + 2 + (dst.has_extra ? 2 : 0));
            continue;
        }

        if (line.opcode == "BR" || line.opcode == "BEQ" || line.opcode == "BNE") {
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        if (line.opcode == "JSR") {
            if (line.operands.size() != 2) {
                throw std::runtime_error("JSR requires two operands");
            }
            uint16_t reg = 0;
            if (!is_register(line.operands[0], reg)) {
                throw std::runtime_error("JSR first operand must be register");
            }
            auto dst = encode_operand(line.operands[1], pc, symbols, true);
            pc = static_cast<uint16_t>(pc + 2 + (dst.has_extra ? 2 : 0));
            continue;
        }

        throw std::runtime_error("Unknown opcode on line " + std::to_string(line.line_no) + ": " + line.opcode);
    }

    std::vector<uint16_t> words;
    words.reserve(1024);
    pc = start;

    for (const auto& line : lines) {
        if (line.opcode == ".ORIG") {
            pc = static_cast<uint16_t>(parse_number(line.operands[0]));
            continue;
        }

        if (!line.label.empty()) {
            // already recorded
        }

        if (line.opcode.empty()) {
            continue;
        }

        if (line.opcode == ".WORD") {
            int32_t value = 0;
            if (line.operands.size() != 1) {
                throw std::runtime_error(".WORD requires one operand");
            }
            if (!is_number(line.operands[0], value)) {
                auto it = symbols.find(upper(line.operands[0]));
                if (it == symbols.end()) {
                    throw std::runtime_error("Undefined symbol: " + line.operands[0]);
                }
                value = it->second;
            }
            words.push_back(static_cast<uint16_t>(value));
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        if (line.opcode == "HALT") {
            words.push_back(0x0000);
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        if (line.opcode == "TRAP") {
            if (line.operands.size() != 1) {
                throw std::runtime_error("TRAP requires one operand");
            }
            std::string op = trim(line.operands[0]);
            if (!op.empty() && op[0] == '#') {
                op = trim(op.substr(1));
            }
            int32_t value = 0;
            if (!is_number(op, value)) {
                throw std::runtime_error("TRAP operand must be numeric");
            }
            if (value < 0 || value > 255) {
                throw std::runtime_error("TRAP vector out of range");
            }
            words.push_back(static_cast<uint16_t>(0104000 | (value & 0xFF)));
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        if (line.opcode == "RTS") {
            if (line.operands.size() != 1) {
                throw std::runtime_error("RTS requires one operand");
            }
            uint16_t reg = 0;
            if (!is_register(line.operands[0], reg)) {
                throw std::runtime_error("RTS operand must be register");
            }
            words.push_back(static_cast<uint16_t>(0000020 | reg));
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        uint16_t base = encode_double_op(line.opcode);
        if (base != 0) {
            auto src = encode_operand(line.operands[0], pc, symbols, false);
            uint16_t pc_after_src = static_cast<uint16_t>(pc + 2 + (src.has_extra ? 2 : 0));
            auto dst = encode_operand(line.operands[1], pc_after_src - 2, symbols, false);

            uint16_t instr = static_cast<uint16_t>(base | (src.spec << 6) | dst.spec);
            words.push_back(instr);
            if (src.has_extra) {
                words.push_back(static_cast<uint16_t>(src.extra));
            }
            if (dst.has_extra) {
                words.push_back(static_cast<uint16_t>(dst.extra));
            }
            pc = static_cast<uint16_t>(pc + 2 + (src.has_extra ? 2 : 0) + (dst.has_extra ? 2 : 0));
            continue;
        }

        base = encode_single_op(line.opcode);
        if (base != 0) {
            auto dst = encode_operand(line.operands[0], pc, symbols, false);
            uint16_t instr = static_cast<uint16_t>(base | dst.spec);
            words.push_back(instr);
            if (dst.has_extra) {
                words.push_back(static_cast<uint16_t>(dst.extra));
            }
            pc = static_cast<uint16_t>(pc + 2 + (dst.has_extra ? 2 : 0));
            continue;
        }

        if (line.opcode == "BR" || line.opcode == "BEQ" || line.opcode == "BNE") {
            if (line.operands.size() != 1) {
                throw std::runtime_error("Branch requires one operand");
            }
            int32_t target = 0;
            if (!is_number(line.operands[0], target)) {
                auto it = symbols.find(upper(line.operands[0]));
                if (it == symbols.end()) {
                    throw std::runtime_error("Undefined symbol: " + line.operands[0]);
                }
                target = it->second;
            }
            int32_t offset = (target - static_cast<int32_t>(pc + 2)) / 2;
            if (offset < -128 || offset > 127) {
                throw std::runtime_error("Branch out of range on line " + std::to_string(line.line_no));
            }
            uint16_t op = 0;
            if (line.opcode == "BR") op = 0000400;
            if (line.opcode == "BNE") op = 0001000;
            if (line.opcode == "BEQ") op = 0001400;
            words.push_back(static_cast<uint16_t>(op | (offset & 0xFF)));
            pc = static_cast<uint16_t>(pc + 2);
            continue;
        }

        if (line.opcode == "JSR") {
            uint16_t reg = 0;
            if (!is_register(line.operands[0], reg)) {
                throw std::runtime_error("JSR first operand must be register");
            }
            auto dst = encode_operand(line.operands[1], pc, symbols, false);
            uint16_t instr = static_cast<uint16_t>(0004000 | (reg << 6) | dst.spec);
            words.push_back(instr);
            if (dst.has_extra) {
                words.push_back(static_cast<uint16_t>(dst.extra));
            }
            pc = static_cast<uint16_t>(pc + 2 + (dst.has_extra ? 2 : 0));
            continue;
        }

        throw std::runtime_error("Unknown opcode on line " + std::to_string(line.line_no) + ": " + line.opcode);
    }

    AsmResult result;
    result.start = start;
    result.words = std::move(words);
    result.symbols = std::move(symbols);
    return result;
}

AsmResult Assembler::assemble_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return assemble(ss.str());
}

} // namespace pdp11
