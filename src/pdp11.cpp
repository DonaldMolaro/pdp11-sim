#include "pdp11.h"

#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace pdp11 {

CPU::CPU() : mem(kMemSize, 0) {
    in_char = []() -> int { return std::getc(stdin); };
    out_char = [](uint8_t v) { std::putchar(static_cast<int>(v)); };
    reset();
}

void CPU::reset() {
    for (auto &reg : r) {
        reg = 0;
    }
    psw = {};
    halted = false;
    mem_bank = 0;
    files.clear();
    mem_watch = {};
    breakpoints.clear();
    break_hit = false;
    break_addr = 0;
}

void CPU::load_words(uint16_t address, const std::vector<uint16_t>& words) {
    for (size_t i = 0; i < words.size(); ++i) {
        write_word_code(address + static_cast<uint16_t>(i * 2), words[i]);
    }
}

static inline uint32_t phys_addr(uint16_t addr, uint8_t bank) {
    return (static_cast<uint32_t>(bank & 0x3) << 16) | addr;
}

uint16_t CPU::read_word(uint16_t address) const {
    uint32_t p = phys_addr(address, mem_bank);
    uint16_t lo = mem[p];
    uint16_t hi = mem[(p + 1) & (CPU::kMemSize - 1)];
    if (mem_watch.trace_all || (mem_watch.enabled && address >= mem_watch.start && address <= mem_watch.end)) {
        std::cout << "MEM R PC=0x" << std::hex << std::setw(4) << std::setfill('0') << r[7]
                  << " addr=0x" << std::setw(4) << address
                  << " size=2 val=0x" << std::setw(4) << static_cast<uint16_t>(lo | (hi << 8))
                  << std::dec << "\n";
    }
    return static_cast<uint16_t>(lo | (hi << 8));
}

void CPU::write_word(uint16_t address, uint16_t value) {
    uint32_t p = phys_addr(address, mem_bank);
    mem[p] = static_cast<uint8_t>(value & 0xFF);
    mem[(p + 1) & (CPU::kMemSize - 1)] = static_cast<uint8_t>((value >> 8) & 0xFF);
    if (mem_watch.trace_all || (mem_watch.enabled && address >= mem_watch.start && address <= mem_watch.end)) {
        std::cout << "MEM W PC=0x" << std::hex << std::setw(4) << std::setfill('0') << r[7]
                  << " addr=0x" << std::setw(4) << address
                  << " size=2 val=0x" << std::setw(4) << value
                  << std::dec << "\n";
    }
}

uint16_t CPU::read_word_code(uint16_t address) const {
    uint32_t p = phys_addr(address, 0);
    uint16_t lo = mem[p];
    uint16_t hi = mem[(p + 1) & (CPU::kMemSize - 1)];
    return static_cast<uint16_t>(lo | (hi << 8));
}

void CPU::write_word_code(uint16_t address, uint16_t value) {
    uint32_t p = phys_addr(address, 0);
    mem[p] = static_cast<uint8_t>(value & 0xFF);
    mem[(p + 1) & (CPU::kMemSize - 1)] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

uint8_t CPU::read_byte(uint16_t address) const {
    uint32_t p = phys_addr(address, mem_bank);
    if (mem_watch.trace_all || (mem_watch.enabled && address >= mem_watch.start && address <= mem_watch.end)) {
        std::cout << "MEM R PC=0x" << std::hex << std::setw(4) << std::setfill('0') << r[7]
                  << " addr=0x" << std::setw(4) << address
                  << " size=1 val=0x" << std::setw(2) << static_cast<uint16_t>(mem[p])
                  << std::dec << "\n";
    }
    return mem[p];
}

void CPU::write_byte(uint16_t address, uint8_t value) {
    uint32_t p = phys_addr(address, mem_bank);
    mem[p] = value;
    if (mem_watch.trace_all || (mem_watch.enabled && address >= mem_watch.start && address <= mem_watch.end)) {
        std::cout << "MEM W PC=0x" << std::hex << std::setw(4) << std::setfill('0') << r[7]
                  << " addr=0x" << std::setw(4) << address
                  << " size=1 val=0x" << std::setw(2) << static_cast<uint16_t>(value)
                  << std::dec << "\n";
    }
}

uint16_t CPU::fetch_word() {
    uint16_t value = read_word_code(r[7]);
    r[7] = static_cast<uint16_t>(r[7] + 2);
    return value;
}

void CPU::set_nz(uint16_t value) {
    psw.n = (value & 0x8000) != 0;
    psw.z = value == 0;
}

void CPU::set_nz_byte(uint8_t value) {
    psw.n = (value & 0x80) != 0;
    psw.z = value == 0;
}

CPU::EA CPU::resolve_ea(uint16_t spec, Access access, int size) {
    uint16_t mode = (spec >> 3) & 0x7;
    uint16_t reg = spec & 0x7;
    uint16_t delta = static_cast<uint16_t>(size);
    if (size == 1 && (reg == 6 || reg == 7)) {
        delta = 2;
    }

    EA ea;

    switch (mode) {
        case 0: // Register
            ea.is_reg = true;
            ea.reg = &r[reg];
            return ea;
        case 1: // Register deferred
            ea.addr = r[reg];
            return ea;
        case 2: { // Autoincrement
            ea.addr = r[reg];
            r[reg] = static_cast<uint16_t>(r[reg] + delta);
            if (reg == 7) {
                ea.is_code = true; // immediate operand lives in code space
            }
            return ea;
        }
        case 3: { // Autoincrement deferred
            uint16_t ptr = r[reg];
            r[reg] = static_cast<uint16_t>(r[reg] + delta);
            ea.addr = (reg == 7) ? read_word_code(ptr) : read_word(ptr);
            return ea;
        }
        case 4: { // Autodecrement
            r[reg] = static_cast<uint16_t>(r[reg] - delta);
            ea.addr = r[reg];
            return ea;
        }
        case 5: { // Autodecrement deferred
            r[reg] = static_cast<uint16_t>(r[reg] - delta);
            ea.addr = read_word(r[reg]);
            return ea;
        }
        case 6: { // Index
            int16_t disp = static_cast<int16_t>(fetch_word());
            ea.addr = static_cast<uint16_t>(r[reg] + disp);
            if (reg == 7) {
                ea.is_code = true; // PC-relative literal lives in code space
            }
            return ea;
        }
        case 7: { // Index deferred
            int16_t disp = static_cast<int16_t>(fetch_word());
            uint16_t ptr = static_cast<uint16_t>(r[reg] + disp);
            ea.addr = (reg == 7) ? read_word_code(ptr) : read_word(ptr);
            return ea;
        }
        default:
            throw std::runtime_error("Invalid addressing mode");
    }
}

uint16_t CPU::read_operand(uint16_t spec) {
    EA ea = resolve_ea(spec, Access::Read, 2);
    if (ea.is_reg) {
        return *ea.reg;
    }
    if (ea.is_code) {
        return read_word_code(ea.addr);
    }
    return read_word(ea.addr);
}

void CPU::write_operand(uint16_t spec, uint16_t value) {
    EA ea = resolve_ea(spec, Access::Write, 2);
    if (ea.is_reg) {
        *ea.reg = value;
        return;
    }
    write_word(ea.addr, value);
}

uint8_t CPU::read_operand_byte(uint16_t spec) {
    EA ea = resolve_ea(spec, Access::Read, 1);
    if (ea.is_reg) {
        return static_cast<uint8_t>(*ea.reg & 0xFF);
    }
    if (ea.is_code) {
        return static_cast<uint8_t>(read_word_code(ea.addr) & 0xFF);
    }
    return read_byte(ea.addr);
}

void CPU::write_operand_byte(uint16_t spec, uint8_t value, bool sign_extend_to_reg) {
    EA ea = resolve_ea(spec, Access::Write, 1);
    if (ea.is_reg) {
        if (sign_extend_to_reg) {
            int8_t s = static_cast<int8_t>(value);
            *ea.reg = static_cast<uint16_t>(static_cast<int16_t>(s));
        } else {
            *ea.reg = static_cast<uint16_t>((*ea.reg & 0xFF00) | value);
        }
        return;
    }
    write_byte(ea.addr, value);
}

uint16_t CPU::operand_address(uint16_t spec) {
    EA ea = resolve_ea(spec, Access::AddressOnly, 2);
    if (ea.is_reg) {
        return *ea.reg;
    }
    return ea.addr;
}

void CPU::step() {
    if (halted) {
        return;
    }

    uint16_t pc_before = r[7];
    uint16_t instr = fetch_word();

    if (instr == 0x0000) { // HALT
        halted = true;
        return;
    }

    if ((instr & 0xFF00) == 0104000) { // TRAP 104000 + vector
        uint8_t vec = static_cast<uint8_t>(instr & 0xFF);
        if (vec == 1) { // putc from R0 low byte
            if (out_char) {
                out_char(static_cast<uint8_t>(r[0] & 0xFF));
            }
            return;
        }
        if (vec == 2) { // getc into R0 low byte
            int ch = in_char ? in_char() : EOF;
            if (ch == EOF) {
                r[0] = 0;
                psw.z = true;
            } else {
                r[0] = static_cast<uint16_t>(ch & 0xFF);
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 3) { // puts from address in R0 (null-terminated)
            uint16_t addr = r[0];
            while (true) {
                uint8_t ch = read_byte(addr);
                if (ch == 0) break;
                if (out_char) {
                    out_char(ch);
                }
                addr = static_cast<uint16_t>(addr + 1);
            }
            return;
        }
        if (vec == 4) { // print signed decimal from R0
            int16_t value = static_cast<int16_t>(r[0]);
            std::ostringstream oss;
            oss << value;
            auto s = oss.str();
            for (char c : s) {
                if (out_char) {
                    out_char(static_cast<uint8_t>(c));
                }
            }
            return;
        }
        if (vec == 5) { // read line into buffer at R0, max bytes in R1 (includes null)
            uint16_t addr = r[0];
            uint16_t max = r[1];
            uint16_t count = 0;
            bool saw_char = false;
            while (count + 1 < max) {
                int ch = in_char ? in_char() : EOF;
                if (ch == EOF) break;
                saw_char = true;
                if (ch == '\n') break;
                write_byte(static_cast<uint16_t>(addr + count),
                           static_cast<uint8_t>(ch & 0xFF));
                ++count;
            }
            if (max > 0) {
                write_byte(static_cast<uint16_t>(addr + count), 0);
            }
            r[0] = count;
            psw.z = (!saw_char && count == 0);
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 6) { // print unsigned hex from R0
            std::ostringstream oss;
            oss << "0x" << std::hex << static_cast<uint16_t>(r[0]);
            auto s = oss.str();
            for (char c : s) {
                if (out_char) {
                    out_char(static_cast<uint8_t>(c));
                }
            }
            return;
        }
        if (vec == 7) { // print unsigned decimal from R0
            std::ostringstream oss;
            oss << static_cast<uint16_t>(r[0]);
            auto s = oss.str();
            for (char c : s) {
                if (out_char) {
                    out_char(static_cast<uint8_t>(c));
                }
            }
            return;
        }
        if (vec == 8) { // println string from address in R0
            uint16_t addr = r[0];
            while (true) {
                uint8_t ch = read_byte(addr);
                if (ch == 0) break;
                if (out_char) {
                    out_char(ch);
                }
                addr = static_cast<uint16_t>(addr + 1);
            }
            if (out_char) {
                out_char('\n');
            }
            return;
        }
        if (vec == 9) { // read signed integer into R0
            int ch = in_char ? in_char() : EOF;
            while (ch != EOF && std::isspace(static_cast<unsigned char>(ch))) {
                ch = in_char ? in_char() : EOF;
            }
            if (ch == EOF) {
                r[0] = 0;
                psw.z = true;
                psw.n = false;
                psw.v = false;
                psw.c = false;
                return;
            }

            int sign = 1;
            if (ch == '-') {
                sign = -1;
                ch = in_char ? in_char() : EOF;
            } else if (ch == '+') {
                ch = in_char ? in_char() : EOF;
            }

            bool any = false;
            int32_t value = 0;
            while (ch != EOF && ch >= '0' && ch <= '9') {
                any = true;
                value = value * 10 + (ch - '0');
                ch = in_char ? in_char() : EOF;
            }
            if (!any) {
                r[0] = 0;
                psw.z = true;
            } else {
                value *= sign;
                r[0] = static_cast<uint16_t>(value);
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 10) { // read hex into R0
            int ch = in_char ? in_char() : EOF;
            while (ch != EOF && std::isspace(static_cast<unsigned char>(ch))) {
                ch = in_char ? in_char() : EOF;
            }
            if (ch == EOF) {
                r[0] = 0;
                psw.z = true;
                psw.n = false;
                psw.v = false;
                psw.c = false;
                return;
            }
            if (ch == '0') {
                int next = in_char ? in_char() : EOF;
                if (next == 'x' || next == 'X') {
                    ch = in_char ? in_char() : EOF;
                } else {
                    ch = next;
                }
            }
            bool any = false;
            uint16_t value = 0;
            while (ch != EOF) {
                int digit = -1;
                if (ch >= '0' && ch <= '9') digit = ch - '0';
                else if (ch >= 'a' && ch <= 'f') digit = 10 + (ch - 'a');
                else if (ch >= 'A' && ch <= 'F') digit = 10 + (ch - 'A');
                else break;
                any = true;
                value = static_cast<uint16_t>((value << 4) | (digit & 0xF));
                ch = in_char ? in_char() : EOF;
            }
            if (!any) {
                r[0] = 0;
                psw.z = true;
            } else {
                r[0] = value;
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 20) { // open file: R0=addr, R1=mode
            uint16_t addr = r[0];
            std::string path;
            for (int i = 0; i < 1024; ++i) {
                uint8_t ch = read_byte(static_cast<uint16_t>(addr + i));
                if (ch == 0) break;
                path.push_back(static_cast<char>(ch));
            }
            std::ios::openmode mode = std::ios::binary;
            switch (r[1]) {
                case 0: mode |= std::ios::in; break;
                case 1: mode |= std::ios::out | std::ios::trunc; break;
                case 2: mode |= std::ios::out | std::ios::app; break;
                case 3: mode |= std::ios::in | std::ios::out; break;
                default: mode |= std::ios::in; break;
            }
            auto fs = std::make_unique<std::fstream>(path, mode);
            if (!fs->is_open()) {
                r[0] = 0xFFFF;
                psw.z = true;
            } else {
                size_t handle = 0;
                for (; handle < files.size(); ++handle) {
                    if (!files[handle]) {
                        break;
                    }
                }
                if (handle == files.size()) {
                    files.push_back(nullptr);
                }
                files[handle] = std::move(fs);
                r[0] = static_cast<uint16_t>(handle);
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 21) { // read file: R0=handle, R1=buf, R2=max
            uint16_t handle = r[0];
            uint16_t addr = r[1];
            uint16_t max = r[2];
            if (handle >= files.size() || !files[handle] || max == 0) {
                r[0] = 0;
                psw.z = true;
                psw.n = false;
                psw.v = false;
                psw.c = false;
                return;
            }
            std::string buf;
            buf.resize(max);
            files[handle]->read(&buf[0], max);
            std::streamsize count = files[handle]->gcount();
            for (std::streamsize i = 0; i < count; ++i) {
                write_byte(static_cast<uint16_t>(addr + i),
                           static_cast<uint8_t>(buf[static_cast<size_t>(i)]));
            }
            r[0] = static_cast<uint16_t>(count);
            psw.z = (count == 0);
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 22) { // write file: R0=handle, R1=buf, R2=len
            uint16_t handle = r[0];
            if (handle >= files.size() || !files[handle]) {
                r[0] = 0;
                psw.z = true;
                psw.n = false;
                psw.v = false;
                psw.c = false;
                return;
            }
            uint16_t addr = r[1];
            uint16_t len = r[2];
            std::string buf;
            buf.resize(len);
            for (uint16_t i = 0; i < len; ++i) {
                buf[i] = static_cast<char>(read_byte(static_cast<uint16_t>(addr + i)));
            }
            files[handle]->write(buf.data(), len);
            if (files[handle]->bad()) {
                r[0] = 0;
                psw.z = true;
            } else {
                r[0] = len;
                psw.z = (len == 0);
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 23) { // close file: R0=handle
            uint16_t handle = r[0];
            if (handle >= files.size() || !files[handle]) {
                r[0] = 0xFFFF;
                psw.z = true;
            } else {
                files[handle]->close();
                files[handle].reset();
                r[0] = 0;
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 24) { // seek file: R0=handle, R1=offset (signed), R2=whence
            uint16_t handle = r[0];
            if (handle >= files.size() || !files[handle]) {
                r[0] = 0xFFFF;
                psw.z = true;
                psw.n = false;
                psw.v = false;
                psw.c = false;
                return;
            }
            std::ios::seekdir dir = std::ios::beg;
            switch (r[2]) {
                case 0: dir = std::ios::beg; break;
                case 1: dir = std::ios::cur; break;
                case 2: dir = std::ios::end; break;
                default: dir = std::ios::beg; break;
            }
            int16_t off = static_cast<int16_t>(r[1]);
            files[handle]->clear();
            files[handle]->seekg(off, dir);
            files[handle]->seekp(off, dir);
            if (files[handle]->fail()) {
                r[0] = 0xFFFF;
                psw.z = true;
            } else {
                r[0] = 0;
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 25) { // tell file: R0=handle
            uint16_t handle = r[0];
            if (handle >= files.size() || !files[handle]) {
                r[0] = 0xFFFF;
                psw.z = true;
                psw.n = false;
                psw.v = false;
                psw.c = false;
                return;
            }
            files[handle]->clear();
            std::streampos pos = files[handle]->tellg();
            if (pos < 0) {
                pos = files[handle]->tellp();
            }
            if (pos < 0) {
                r[0] = 0xFFFF;
                psw.z = true;
            } else {
                r[0] = static_cast<uint16_t>(pos & 0xFFFF);
                psw.z = false;
            }
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
        if (vec == 26) { // set memory bank: R0=0..3
            mem_bank = static_cast<uint8_t>(r[0] & 0x3);
            r[0] = 0;
            psw.z = false;
            psw.n = false;
            psw.v = false;
            psw.c = false;
            return;
        }
    }

    uint16_t opcode = instr & 0xF000;

    // Single operand group
    if ((instr & 0xFFC0) == 0000100) { // JMP 0001dd
        uint16_t dst = instr & 0x3F;
        r[7] = operand_address(dst);
        return;
    }

    if ((instr & 0xFE00) == 0004000) { // JSR 004Rdd
        uint16_t reg = (instr >> 6) & 0x7;
        uint16_t dst = instr & 0x3F;
        uint16_t addr = operand_address(dst);
        r[6] = static_cast<uint16_t>(r[6] - 2);
        write_word(r[6], r[reg]);
        r[reg] = r[7];
        r[7] = addr;
        return;
    }

    if ((instr & 0xFFF8) == 0000020) { // RTS 00020R
        uint16_t reg = instr & 0x7;
        uint16_t old = r[reg];
        r[reg] = read_word(r[6]);
        r[6] = static_cast<uint16_t>(r[6] + 2);
        r[7] = old;
        return;
    }

    if ((instr & 0xFFC0) == 0005000) { // CLR 0050dd
        uint16_t dst = instr & 0x3F;
        write_operand(dst, 0);
        psw.n = false;
        psw.z = true;
        psw.v = false;
        psw.c = false;
        return;
    }

    if ((instr & 0xFFC0) == 0005200) { // INC 0052dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        uint16_t res = static_cast<uint16_t>(val + 1);
        write_operand(dst, res);
        set_nz(res);
        psw.v = (val == 0x7FFF);
        return;
    }

    if ((instr & 0xFFC0) == 0005300) { // DEC 0053dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        uint16_t res = static_cast<uint16_t>(val - 1);
        write_operand(dst, res);
        set_nz(res);
        psw.v = (val == 0x8000);
        return;
    }

    if ((instr & 0xFFC0) == 0005700) { // TST 0057dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        set_nz(val);
        psw.v = false;
        psw.c = false;
        return;
    }

    if ((instr & 0xFFC0) == 0006000) { // ROR 0060dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        uint16_t new_c = val & 0x1;
        uint16_t res = static_cast<uint16_t>((psw.c ? 0x8000 : 0) | (val >> 1));
        write_operand(dst, res);
        psw.c = new_c != 0;
        set_nz(res);
        psw.v = psw.n ^ psw.c;
        return;
    }

    if ((instr & 0xFFC0) == 0006100) { // ROL 0061dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        uint16_t new_c = (val & 0x8000) != 0;
        uint16_t res = static_cast<uint16_t>((val << 1) | (psw.c ? 1 : 0));
        write_operand(dst, res);
        psw.c = new_c != 0;
        set_nz(res);
        psw.v = psw.n ^ psw.c;
        return;
    }

    if ((instr & 0xFFC0) == 0006200) { // ASR 0062dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        uint16_t new_c = val & 0x1;
        uint16_t res = static_cast<uint16_t>((val & 0x8000) | (val >> 1));
        write_operand(dst, res);
        psw.c = new_c != 0;
        set_nz(res);
        psw.v = psw.n ^ psw.c;
        return;
    }

    if ((instr & 0xFFC0) == 0006300) { // ASL 0063dd
        uint16_t dst = instr & 0x3F;
        uint16_t val = read_operand(dst);
        uint16_t new_c = (val & 0x8000) != 0;
        uint16_t res = static_cast<uint16_t>(val << 1);
        write_operand(dst, res);
        psw.c = new_c != 0;
        set_nz(res);
        psw.v = psw.n ^ psw.c;
        return;
    }

    if ((instr & 0xFFC0) == 0105000) { // CLRB 1050dd
        uint16_t dst = instr & 0x3F;
        write_operand_byte(dst, 0, false);
        psw.n = false;
        psw.z = true;
        psw.v = false;
        psw.c = false;
        return;
    }

    if ((instr & 0xFFC0) == 0105200) { // INCB 1052dd
        uint16_t dst = instr & 0x3F;
        uint8_t val = read_operand_byte(dst);
        uint8_t res = static_cast<uint8_t>(val + 1);
        write_operand_byte(dst, res, false);
        set_nz_byte(res);
        psw.v = (val == 0x7F);
        return;
    }

    if ((instr & 0xFFC0) == 0105300) { // DECB 1053dd
        uint16_t dst = instr & 0x3F;
        uint8_t val = read_operand_byte(dst);
        uint8_t res = static_cast<uint8_t>(val - 1);
        write_operand_byte(dst, res, false);
        set_nz_byte(res);
        psw.v = (val == 0x80);
        return;
    }

    if ((instr & 0xFFC0) == 0105700) { // TSTB 1057dd
        uint16_t dst = instr & 0x3F;
        uint8_t val = read_operand_byte(dst);
        set_nz_byte(val);
        psw.v = false;
        psw.c = false;
        return;
    }

    // Branch group
    if ((instr & 0xFF00) >= 0000400 && (instr & 0xFF00) <= 0001400) {
        uint16_t op = instr & 0xFF00;
        int8_t off = static_cast<int8_t>(instr & 0xFF);
        if (op == 0000400) { // BR
            r[7] = static_cast<uint16_t>(r[7] + static_cast<int16_t>(off) * 2);
            return;
        }
        if (op == 0001400) { // BEQ
            if (psw.z) {
                r[7] = static_cast<uint16_t>(r[7] + static_cast<int16_t>(off) * 2);
            }
            return;
        }
        if (op == 0001000) { // BNE
            if (!psw.z) {
                r[7] = static_cast<uint16_t>(r[7] + static_cast<int16_t>(off) * 2);
            }
            return;
        }
    }

    // Double operand group
    if ((instr & 0xF000) == 0010000 || (instr & 0xF000) == 0060000 ||
        (instr & 0xF000) == 0020000 || (instr & 0xF000) == 0160000 ||
        (instr & 0xF000) == 0030000 || (instr & 0xF000) == 0040000 ||
        (instr & 0xF000) == 0050000) {
        uint16_t src = (instr >> 6) & 0x3F;
        uint16_t dst = instr & 0x3F;

        if ((instr & 0xF000) == 0010000) { // MOV 01SSDD
            uint16_t val = read_operand(src);
            write_operand(dst, val);
            set_nz(val);
            psw.v = false;
            return;
        }
        if ((instr & 0xF000) == 0020000) { // CMP 02SSDD (dst - src)
            uint16_t s = read_operand(src);
            uint16_t d = read_operand(dst);
            uint32_t res = static_cast<uint32_t>(d) - static_cast<uint32_t>(s);
            uint16_t r16 = static_cast<uint16_t>(res);
            set_nz(r16);
            psw.v = ((d ^ s) & (d ^ r16) & 0x8000) != 0;
            psw.c = (res & 0x10000) != 0;
            return;
        }
        if ((instr & 0xF000) == 0060000) { // ADD 06SSDD
            uint16_t s = read_operand(src);
            uint16_t d = read_operand(dst);
            uint32_t res = static_cast<uint32_t>(s) + static_cast<uint32_t>(d);
            uint16_t r16 = static_cast<uint16_t>(res);
            write_operand(dst, r16);
            set_nz(r16);
            psw.v = (~(s ^ d) & (s ^ r16) & 0x8000) != 0;
            psw.c = (res & 0x10000) != 0;
            return;
        }
        if ((instr & 0xF000) == 0160000) { // SUB 16SSDD
            uint16_t s = read_operand(src);
            uint16_t d = read_operand(dst);
            uint32_t res = static_cast<uint32_t>(d) - static_cast<uint32_t>(s);
            uint16_t r16 = static_cast<uint16_t>(res);
            write_operand(dst, r16);
            set_nz(r16);
            psw.v = ((d ^ s) & (d ^ r16) & 0x8000) != 0;
            psw.c = (res & 0x10000) != 0;
            return;
        }
        if ((instr & 0xF000) == 0030000) { // BIT 03SSDD
            uint16_t s = read_operand(src);
            uint16_t d = read_operand(dst);
            uint16_t r16 = static_cast<uint16_t>(s & d);
            set_nz(r16);
            psw.v = false;
            psw.c = false;
            return;
        }
        if ((instr & 0xF000) == 0040000) { // BIC 04SSDD
            uint16_t s = read_operand(src);
            uint16_t d = read_operand(dst);
            uint16_t r16 = static_cast<uint16_t>(d & ~s);
            write_operand(dst, r16);
            set_nz(r16);
            psw.v = false;
            psw.c = false;
            return;
        }
        if ((instr & 0xF000) == 0050000) { // BIS 05SSDD
            uint16_t s = read_operand(src);
            uint16_t d = read_operand(dst);
            uint16_t r16 = static_cast<uint16_t>(d | s);
            write_operand(dst, r16);
            set_nz(r16);
            psw.v = false;
            psw.c = false;
            return;
        }
    }

    // Byte double-operand group
    if ((instr & 0xF000) == 0110000 || (instr & 0xF000) == 0120000 ||
        (instr & 0xF000) == 0130000 || (instr & 0xF000) == 0140000 ||
        (instr & 0xF000) == 0150000) {
        uint16_t src = (instr >> 6) & 0x3F;
        uint16_t dst = instr & 0x3F;

        if ((instr & 0xF000) == 0110000) { // MOVB 11SSDD
            uint8_t val = read_operand_byte(src);
            write_operand_byte(dst, val, true);
            set_nz_byte(val);
            psw.v = false;
            return;
        }
        if ((instr & 0xF000) == 0120000) { // CMPB 12SSDD (dst - src)
            uint8_t s = read_operand_byte(src);
            uint8_t d = read_operand_byte(dst);
            uint16_t res = static_cast<uint16_t>(d) - static_cast<uint16_t>(s);
            uint8_t r8 = static_cast<uint8_t>(res & 0xFF);
            set_nz_byte(r8);
            int8_t sd = static_cast<int8_t>(d);
            int8_t ss = static_cast<int8_t>(s);
            int8_t sr = static_cast<int8_t>(r8);
            psw.v = ((sd ^ ss) & (sd ^ sr) & 0x80) != 0;
            psw.c = (res & 0x100) != 0;
            return;
        }
        if ((instr & 0xF000) == 0130000) { // BITB 13SSDD
            uint8_t s = read_operand_byte(src);
            uint8_t d = read_operand_byte(dst);
            uint8_t r8 = static_cast<uint8_t>(s & d);
            set_nz_byte(r8);
            psw.v = false;
            psw.c = false;
            return;
        }
        if ((instr & 0xF000) == 0140000) { // BICB 14SSDD
            uint8_t s = read_operand_byte(src);
            uint8_t d = read_operand_byte(dst);
            uint8_t r8 = static_cast<uint8_t>(d & static_cast<uint8_t>(~s));
            write_operand_byte(dst, r8, false);
            set_nz_byte(r8);
            psw.v = false;
            psw.c = false;
            return;
        }
        if ((instr & 0xF000) == 0150000) { // BISB 15SSDD
            uint8_t s = read_operand_byte(src);
            uint8_t d = read_operand_byte(dst);
            uint8_t r8 = static_cast<uint8_t>(d | s);
            write_operand_byte(dst, r8, false);
            set_nz_byte(r8);
            psw.v = false;
            psw.c = false;
            return;
        }
    }

    std::ostringstream oss;
    oss << "Unimplemented instruction 0x" << std::hex << instr << std::dec
        << " at PC=" << pc_before;
    throw std::runtime_error(oss.str());
}

void CPU::run(uint64_t max_steps) {
    for (uint64_t i = 0; i < max_steps && !halted; ++i) {
        if (!breakpoints.empty() && breakpoints.find(r[7]) != breakpoints.end()) {
            break_hit = true;
            break_addr = r[7];
            return;
        }
        step();
    }
}

} // namespace pdp11
