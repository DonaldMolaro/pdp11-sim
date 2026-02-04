#include "assembler.h"
#include "pdp11.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace pdp11;

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

static std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

#define TEST(name) \
    void test_##name(); \
    static TestRegistrar registrar_##name(#name, test_##name); \
    void test_##name()

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream oss; \
            oss << "Requirement failed: " << #cond << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

static CPU run(const std::string& asm_source, uint64_t max_steps = 100000) {
    Assembler asmblr;
    AsmResult res = asmblr.assemble(asm_source);
    CPU cpu;
    cpu.reset();
    cpu.r[7] = res.start;
    cpu.r[6] = 0xFFFE;
    cpu.load_words(res.start, res.words);
    cpu.run(max_steps);
    return cpu;
}

static CPU run_with_io(const std::string& asm_source,
                       const std::function<int()>& in_cb,
                       const std::function<void(uint8_t)>& out_cb,
                       uint64_t max_steps = 100000) {
    Assembler asmblr;
    AsmResult res = asmblr.assemble(asm_source);
    CPU cpu;
    cpu.reset();
    cpu.in_char = in_cb;
    cpu.out_char = out_cb;
    cpu.r[7] = res.start;
    cpu.r[6] = 0xFFFE;
    cpu.load_words(res.start, res.words);
    cpu.run(max_steps);
    return cpu;
}

TEST(MovImmediate) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #1234, R0
        HALT
    )");
    REQUIRE(cpu.r[0] == 1234);
    REQUIRE(cpu.halted);
}

TEST(AddSub) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #10, R0
        MOV #3, R1
        ADD R0, R1
        SUB #2, R1
        HALT
    )");
    REQUIRE(cpu.r[1] == 11);
}

TEST(MemoryIndirect) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #100, R0
        MOV #777, (R0)
        MOV (R0), R1
        HALT
    )");
    REQUIRE(cpu.r[1] == 777);
}

TEST(IndexedAddressing) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #200, R0
        MOV #55, 2(R0)
        MOV 2(R0), R1
        HALT
    )");
    REQUIRE(cpu.r[1] == 55);
}

TEST(AutoincAutodec) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #300, R0
        MOV #1, (R0)+
        MOV #2, (R0)+
        MOV -(R0), R1
        MOV -(R0), R2
        HALT
    )");
    REQUIRE(cpu.r[1] == 2);
    REQUIRE(cpu.r[2] == 1);
}

TEST(BranchLoop) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #5, R0
    loop:
        DEC R0
        BNE loop
        HALT
    )");
    REQUIRE(cpu.r[0] == 0);
    REQUIRE(cpu.psw.z);
}

TEST(JSRRTS) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #0, R0
        JSR R5, sub
        HALT
    sub:
        INC R0
        RTS R5
    )");
    REQUIRE(cpu.r[0] == 1);
    REQUIRE(cpu.halted);
}

TEST(PCRelativeLabels) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #0, R0
        MOV label, R1
        HALT
    label:
        .WORD 123
    )");
    REQUIRE(cpu.r[1] == 123);
}

TEST(FlagsFromCMP) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #5, R0
        MOV #5, R1
        CMP R0, R1
        BEQ equal
        MOV #1, R2
    equal:
        HALT
    )");
    REQUIRE(cpu.psw.z);
}

TEST(BitBicBis) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #0x00F0, R0
        MOV #0x0F0F, R1
        BIT R0, R1
        BIC R0, R1
        BIS #0x0003, R1
        HALT
    )");
    REQUIRE(cpu.psw.z == false);
    REQUIRE(cpu.r[1] == 0x0F0F);
}

TEST(ByteOpsAndSignExtend) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #0x1234, R0
        BISB #0x80, R0
        MOVB #0x80, R1
        CLRB R0
        HALT
    )");
    REQUIRE(cpu.r[0] == 0x1200);
    REQUIRE(cpu.r[1] == 0xFF80);
}

TEST(ShiftsAndRotate) {
    auto cpu = run(R"(
        .ORIG 0
        MOV #0x4000, R0
        ASL R0
        MOV #0x8001, R1
        ASR R1
        MOV #0xFFFF, R2
        ADD #1, R2
        ROL R2
        HALT
    )");
    REQUIRE(cpu.r[0] == 0x8000);
    REQUIRE(cpu.r[1] == 0xC000);
    REQUIRE(cpu.r[2] == 0x0001);
}

TEST(TrapOutputString) {
    std::string out;
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV #msg, R0
        TRAP #3
        HALT
    msg:
        .WORD 0x6948
        .WORD 0x0000
    )",
    []() -> int { return EOF; },
    [&](uint8_t ch) { out.push_back(static_cast<char>(ch)); });
    REQUIRE(out == "Hi");
    REQUIRE(cpu.halted);
}

TEST(TrapGetChar) {
    int provided = 'Z';
    auto cpu = run_with_io(R"(
        .ORIG 0
        TRAP #2
        HALT
    )",
    [&]() -> int {
        if (provided < 0) return EOF;
        int v = provided;
        provided = -1;
        return v;
    },
    [](uint8_t) {});
    REQUIRE((cpu.r[0] & 0xFF) == 'Z');
}

TEST(TrapPrintIntAndHex) {
    std::string out;
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV # -123, R0
        TRAP #4
        MOV #0x2A, R0
        TRAP #6
        HALT
    )",
    []() -> int { return EOF; },
    [&](uint8_t ch) { out.push_back(static_cast<char>(ch)); });
    REQUIRE(out == "-1230x2a");
    REQUIRE(cpu.halted);
}

TEST(TrapReadLine) {
    std::string out;
    std::string input = "hello\n";
    size_t idx = 0;
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV #buf, R0
        MOV #6, R1
        TRAP #5
        MOV R0, R2
        MOV #buf, R0
        TRAP #3
        HALT
    buf:
        .WORD 0
        .WORD 0
        .WORD 0
    )",
    [&]() -> int {
        if (idx >= input.size()) return EOF;
        return static_cast<uint8_t>(input[idx++]);
    },
    [&](uint8_t ch) { out.push_back(static_cast<char>(ch)); });
    REQUIRE(cpu.r[2] == 5);
    REQUIRE(out == "hello");
}

int main() {
    int passed = 0;
    int failed = 0;

    for (const auto& test : registry()) {
        try {
            std::cout << "[RUN ] " << test.name << "\n";
            test.fn();
            ++passed;
            std::cout << "[ OK ] " << test.name << "\n";
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
        }
    }

    std::cout << "Total: " << (passed + failed)
              << " Passed: " << passed
              << " Failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
