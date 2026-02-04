#pragma once

#include <cstdint>
#include <functional>
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
    static constexpr uint32_t kMemSize = 65536; // bytes

    uint16_t r[8]{}; // R0-R7 (R7=PC, R6=SP)
    Flags psw{};
    bool halted = false;

    std::vector<uint8_t> mem;
    std::function<int()> in_char;
    std::function<void(uint8_t)> out_char;

    CPU();

    void reset();
    void load_words(uint16_t address, const std::vector<uint16_t>& words);
    void run(uint64_t max_steps = 1000000);
    void step();

    uint16_t read_word(uint16_t address) const;
    void write_word(uint16_t address, uint16_t value);
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
    };

    EA resolve_ea(uint16_t spec, Access access, int size);
    uint16_t read_operand(uint16_t spec);
    void write_operand(uint16_t spec, uint16_t value);
    uint8_t read_operand_byte(uint16_t spec);
    void write_operand_byte(uint16_t spec, uint8_t value, bool sign_extend_to_reg);
    uint16_t operand_address(uint16_t spec);
};

} // namespace pdp11
