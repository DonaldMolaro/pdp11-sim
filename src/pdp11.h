#pragma once

#include <cstdint>
#include <functional>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace pdp11 {

struct Flags {
    bool n = false;
    bool z = false;
    bool v = false;
    bool c = false;
};

struct CPU {
    static constexpr uint32_t kMemSize = 262144; // bytes (4 banks of 64K)

    uint16_t r[8]{}; // R0-R7 (R7=PC, R6=SP)
    Flags psw{};
    bool halted = false;
    uint8_t mem_bank = 0; // 0-3

    std::vector<uint8_t> mem;
    std::function<int()> in_char;
    std::function<void(uint8_t)> out_char;
    std::vector<std::unique_ptr<std::fstream>> files;

    struct MemWatch {
        bool enabled = false;
        bool trace_all = false;
        uint16_t start = 0;
        uint16_t end = 0;
    } mem_watch;

    CPU();

    void reset();
    void load_words(uint16_t address, const std::vector<uint16_t>& words);
    void run(uint64_t max_steps = 1000000);
    void step();

    uint16_t read_word(uint16_t address) const;
    void write_word(uint16_t address, uint16_t value);
    uint16_t read_word_code(uint16_t address) const;
    void write_word_code(uint16_t address, uint16_t value);
    uint8_t read_byte(uint16_t address) const;
    void write_byte(uint16_t address, uint8_t value);

private:
    uint16_t fetch_word();
    void set_nz(uint16_t value);
    void set_nz_byte(uint8_t value);

    enum class Access {
        Read,
        Write,
        AddressOnly
    };

    struct EA {
        bool is_reg = false;
        uint16_t* reg = nullptr;
        uint16_t addr = 0;
        bool is_code = false;
    };

    EA resolve_ea(uint16_t spec, Access access, int size);
    uint16_t read_operand(uint16_t spec);
    void write_operand(uint16_t spec, uint16_t value);
    uint8_t read_operand_byte(uint16_t spec);
    void write_operand_byte(uint16_t spec, uint8_t value, bool sign_extend_to_reg);
    uint16_t operand_address(uint16_t spec);
};

} // namespace pdp11
