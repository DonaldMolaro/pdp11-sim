#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace pdp11 {

struct AsmResult {
    uint16_t start = 0;
    std::vector<uint16_t> words;
    std::unordered_map<std::string, uint16_t> symbols;
};

class Assembler {
public:
    AsmResult assemble(const std::string& source);
    AsmResult assemble_file(const std::string& path);

private:
    struct Line {
        int line_no = 0;
        std::string label;
        std::string opcode;
        std::vector<std::string> operands;
        std::string raw;
    };

    std::vector<Line> parse_lines(const std::string& source);
    static std::string trim(const std::string& s);
    static std::string upper(const std::string& s);

    static bool is_register(const std::string& token, uint16_t& reg);
    static bool is_number(const std::string& token, int32_t& value);
    static int32_t parse_number(const std::string& token);

    struct OperandEnc {
        uint16_t spec = 0;
        bool has_extra = false;
        int32_t extra = 0;
    };

    OperandEnc encode_operand(const std::string& token, uint16_t pc,
                              const std::unordered_map<std::string, uint16_t>& symbols,
                              bool allow_undefined);

    uint16_t encode_double_op(const std::string& opcode);
    uint16_t encode_single_op(const std::string& opcode);
};

} // namespace pdp11
