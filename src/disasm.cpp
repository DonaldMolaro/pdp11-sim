#include "disasm.h"

#include <iomanip>
#include <sstream>

namespace pdp11 {

static std::string fmt_word(uint16_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << v;
    return oss.str();
}

static std::string reg_name(uint16_t r) {
    return "R" + std::to_string(r);
}

static std::string format_operand(const CPU& cpu, uint16_t spec, uint16_t& pc_next) {
    uint16_t mode = (spec >> 3) & 0x7;
    uint16_t reg = spec & 0x7;

    auto read_ext = [&]() {
        uint16_t word = cpu.read_word(pc_next);
        pc_next = static_cast<uint16_t>(pc_next + 2);
        return word;
    };

    switch (mode) {
        case 0:
            return reg_name(reg);
        case 1:
            return "(" + reg_name(reg) + ")";
        case 2:
            if (reg == 7) {
                uint16_t imm = read_ext();
                return "#" + fmt_word(imm);
            }
            return "(" + reg_name(reg) + ")+";
        case 3:
            if (reg == 7) {
                uint16_t abs = read_ext();
                return "@#" + fmt_word(abs);
            }
            return "@(" + reg_name(reg) + ")+";
        case 4:
            return "-(" + reg_name(reg) + ")";
        case 5:
            return "@-(" + reg_name(reg) + ")";
        case 6: {
            int16_t disp = static_cast<int16_t>(read_ext());
            if (reg == 7) {
                uint16_t target = static_cast<uint16_t>(pc_next + disp);
                return fmt_word(target);
            }
            return fmt_word(static_cast<uint16_t>(disp)) + "(" + reg_name(reg) + ")";
        }
        case 7: {
            int16_t disp = static_cast<int16_t>(read_ext());
            if (reg == 7) {
                uint16_t target = cpu.read_word(static_cast<uint16_t>(pc_next + disp));
                return "@" + fmt_word(target);
            }
            return "@" + fmt_word(static_cast<uint16_t>(disp)) + "(" + reg_name(reg) + ")";
        }
        default:
            return "?";
    }
}

std::string disassemble(const CPU& cpu, uint16_t pc) {
    uint16_t instr = cpu.read_word(pc);
    uint16_t pc_next = static_cast<uint16_t>(pc + 2);

    if (instr == 0x0000) {
        return "HALT";
    }

    if ((instr & 0xFFC0) == 0000100) {
        return "JMP " + format_operand(cpu, instr & 0x3F, pc_next);
    }

    if ((instr & 0xFE00) == 0004000) {
        uint16_t reg = (instr >> 6) & 0x7;
        return "JSR " + reg_name(reg) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    }

    if ((instr & 0xFFF8) == 0000020) {
        uint16_t reg = instr & 0x7;
        return "RTS " + reg_name(reg);
    }

    if ((instr & 0xFF00) == 0104000) {
        uint16_t vec = instr & 0xFF;
        return "TRAP #" + fmt_word(vec);
    }

    if ((instr & 0xFFC0) == 0005000) return "CLR " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0005200) return "INC " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0005300) return "DEC " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0005700) return "TST " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0006000) return "ROR " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0006100) return "ROL " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0006200) return "ASR " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0006300) return "ASL " + format_operand(cpu, instr & 0x3F, pc_next);

    if ((instr & 0xFFC0) == 0105000) return "CLRB " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0105200) return "INCB " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0105300) return "DECB " + format_operand(cpu, instr & 0x3F, pc_next);
    if ((instr & 0xFFC0) == 0105700) return "TSTB " + format_operand(cpu, instr & 0x3F, pc_next);

    if ((instr & 0xFF00) == 0000400) {
        int8_t off = static_cast<int8_t>(instr & 0xFF);
        uint16_t target = static_cast<uint16_t>(pc_next + static_cast<int16_t>(off) * 2);
        return "BR " + fmt_word(target);
    }
    if ((instr & 0xFF00) == 0001000) {
        int8_t off = static_cast<int8_t>(instr & 0xFF);
        uint16_t target = static_cast<uint16_t>(pc_next + static_cast<int16_t>(off) * 2);
        return "BNE " + fmt_word(target);
    }
    if ((instr & 0xFF00) == 0001400) {
        int8_t off = static_cast<int8_t>(instr & 0xFF);
        uint16_t target = static_cast<uint16_t>(pc_next + static_cast<int16_t>(off) * 2);
        return "BEQ " + fmt_word(target);
    }

    uint16_t op = instr & 0xF000;
    if (op == 0010000) return "MOV " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0020000) return "CMP " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0030000) return "BIT " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0040000) return "BIC " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0050000) return "BIS " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0060000) return "ADD " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0160000) return "SUB " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);

    if (op == 0110000) return "MOVB " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0120000) return "CMPB " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0130000) return "BITB " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0140000) return "BICB " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);
    if (op == 0150000) return "BISB " + format_operand(cpu, (instr >> 6) & 0x3F, pc_next) + ", " + format_operand(cpu, instr & 0x3F, pc_next);

    std::ostringstream oss;
    oss << "DATA " << fmt_word(instr);
    return oss.str();
}

} // namespace pdp11
