#include "assembler.h"
#include "pdp11.h"

#include <functional>
#include <iostream>
#include <memory>
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

static CPU run_with_watch(const std::string& asm_source,
                          uint16_t watch_start,
                          uint16_t watch_end,
                          std::string* watch_out,
                          uint64_t max_steps = 100000) {
    Assembler asmblr;
    AsmResult res = asmblr.assemble(asm_source);
    CPU cpu;
    cpu.reset();
    cpu.r[7] = res.start;
    cpu.r[6] = 0xFFFE;
    cpu.load_words(res.start, res.words);
    cpu.mem_watch.enabled = true;
    cpu.mem_watch.start = watch_start;
    cpu.mem_watch.end = watch_end;

    std::ostringstream buf;
    std::streambuf* old = std::cout.rdbuf(buf.rdbuf());
    cpu.run(max_steps);
    std::cout.rdbuf(old);

    if (watch_out) {
        *watch_out = buf.str();
    }
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

TEST(SymbolTableContainsLabel) {
    Assembler asmblr;
    AsmResult res = asmblr.assemble(R"(
        .ORIG 0
        BR loop
    loop:
        HALT
    )");
    REQUIRE(res.symbols.find("LOOP") != res.symbols.end());
}

TEST(BreakpointStops) {
    Assembler asmblr;
    AsmResult res = asmblr.assemble(R"(
        .ORIG 0
        MOV #1, R0
    stop:
        INC R0
        HALT
    )");
    CPU cpu;
    cpu.reset();
    cpu.r[7] = res.start;
    cpu.r[6] = 0xFFFE;
    cpu.load_words(res.start, res.words);
    cpu.breakpoints.insert(res.symbols.at("STOP"));
    cpu.run();
    REQUIRE(cpu.break_hit);
    REQUIRE(cpu.break_addr == res.symbols.at("STOP"));
    REQUIRE(cpu.r[0] == 1);
    REQUIRE(cpu.halted == false);
}

TEST(MemWatchOutput) {
    std::string out;
    auto cpu = run_with_watch(R"(
        .ORIG 0
        MOV #0x0100, R0
        MOV #0x00AA, (R0)
        MOV (R0), R1
        HALT
    )", 0x0100, 0x0100, &out);
    REQUIRE(cpu.r[1] == 0x00AA);
    REQUIRE(out.find("MEM W") != std::string::npos);
    REQUIRE(out.find("MEM R") != std::string::npos);
    REQUIRE(out.find("addr=0x0100") != std::string::npos);
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

TEST(TrapPrintUnsigned) {
    std::string out;
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV #65535, R0
        TRAP #7
        HALT
    )",
    []() -> int { return EOF; },
    [&](uint8_t ch) { out.push_back(static_cast<char>(ch)); });
    REQUIRE(out == "65535");
    REQUIRE(cpu.halted);
}

TEST(TrapReadIntAndHex) {
    std::string input = "  -42 0x1A ";
    size_t idx = 0;
    auto cpu = run_with_io(R"(
        .ORIG 0
        TRAP #9
        MOV R0, R2
        TRAP #10
        MOV R0, R3
        HALT
    )",
    [&]() -> int {
        if (idx >= input.size()) return EOF;
        return static_cast<uint8_t>(input[idx++]);
    },
    [](uint8_t) {});
    REQUIRE(static_cast<int16_t>(cpu.r[2]) == -42);
    REQUIRE(cpu.r[3] == 0x001A);
}

TEST(TrapFileIO) {
    std::string out;
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV #path, R0
        MOV #1, R1
        TRAP #20
        MOV R0, R4
        MOV #buf, R1
        MOV #5, R2
        MOVB #72, (R1)+
        MOVB #101, (R1)+
        MOVB #108, (R1)+
        MOVB #108, (R1)+
        MOVB #111, (R1)+
        MOV #buf, R1
        TRAP #22
        MOV R4, R0
        TRAP #23
        MOV #path, R0
        MOV #0, R1
        TRAP #20
        MOV R0, R4
        MOV #buf, R1
        MOV #5, R2
        MOV R4, R0
        TRAP #21
        MOV #buf, R0
        TRAP #3
        MOV R4, R0
        TRAP #23
        HALT
    path:
        .WORD 0x2E74
        .WORD 0x7874
        .WORD 0x0074
    buf:
        .WORD 0
        .WORD 0
        .WORD 0
    )",
    []() -> int { return EOF; },
    [&](uint8_t ch) { out.push_back(static_cast<char>(ch)); });
    REQUIRE(out == "Hello");
    REQUIRE(cpu.halted);
}

TEST(TrapSeekTell) {
    std::string out;
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV #path, R0
        MOV #1, R1
        TRAP #20
        MOV R0, R4
        MOV #buf, R1
        MOV #6, R2
        MOVB #65, (R1)+
        MOVB #66, (R1)+
        MOVB #67, (R1)+
        MOVB #68, (R1)+
        MOVB #69, (R1)+
        MOVB #70, (R1)+
        MOV #buf, R1
        TRAP #22
        MOV R4, R0
        TRAP #23
        MOV #path, R0
        MOV #0, R1
        TRAP #20
        MOV R0, R4
        MOV R4, R0
        MOV #2, R1
        MOV #0, R2
        TRAP #24
        MOV R4, R0
        MOV #buf, R1
        MOV #2, R2
        TRAP #21
        MOV #buf, R1
        MOVB #0, 2(R1)
        MOV #buf, R0
        TRAP #3
        MOV R4, R0
        TRAP #25
        MOV R0, R2
        MOV R4, R0
        TRAP #23
        HALT
    path:
        .WORD 0x742f
        .WORD 0x706d
        .WORD 0x702f
        .WORD 0x3170
        .WORD 0x5f31
        .WORD 0x6573
        .WORD 0x2e6b
        .WORD 0x7874
        .WORD 0x0074
    buf:
        .WORD 0
        .WORD 0
        .WORD 0
    )",
    []() -> int { return EOF; },
    [&](uint8_t ch) { out.push_back(static_cast<char>(ch)); });
    REQUIRE(out == "CD");
    REQUIRE(cpu.r[2] == 4);
    REQUIRE(cpu.halted);
}

TEST(TrapMemoryBank) {
    auto cpu = run_with_io(R"(
        .ORIG 0
        MOV #0, R0
        TRAP #26
        MOV #0x0100, R5
        MOV #123, R1
        MOV R1, (R5)
        MOV #1, R0
        TRAP #26
        MOV #0, R2
        MOV (R5), R2
        MOV #1, R1
        MOV R1, (R5)
        MOV #0, R0
        TRAP #26
        MOV (R5), R3
        HALT
    )",
    []() -> int { return EOF; },
    [](uint8_t) {});
    REQUIRE(cpu.r[2] == 0);
    REQUIRE(cpu.r[3] == 123);
}

TEST(TrapImmediateUsesCodeBank) {
    auto cpu = run_with_io(R"(
        .ORIG 0x1000
        MOV #1, R0
        TRAP #26
        MOV #0x1234, R3
        MOV #0x0100, R1
        MOV R3, (R1)
        MOV (R1), R0
        HALT
    )",
    []() -> int { return EOF; },
    [](uint8_t) {});
    REQUIRE(cpu.r[0] == 0x1234);
}

TEST(TrapPCRelativeLiteralUsesCodeBank) {
    auto cpu = run_with_io(R"(
        .ORIG 0x2000
        MOV #2, R0
        TRAP #26
        MOV literal, R1
        MOV #0x0100, R2
        MOV R1, (R2)
        MOV (R2), R0
        HALT
    literal:
        .WORD 0xBEEF
    )",
    []() -> int { return EOF; },
    [](uint8_t) {});
    REQUIRE(cpu.r[0] == 0xBEEF);
}

TEST(MemWatchLogs) {
    auto old_buf = std::cout.rdbuf();
    std::ostringstream capture;
    std::cout.rdbuf(capture.rdbuf());
    {
        Assembler asmblr;
        AsmResult res = asmblr.assemble(R"(
            .ORIG 0
            MOV #0x0100, R1
            MOV #0x1234, (R1)
            MOV (R1), R0
            HALT
        )");
        CPU cpu;
        cpu.reset();
        cpu.r[7] = res.start;
        cpu.r[6] = 0xFFFE;
        cpu.load_words(res.start, res.words);
        cpu.mem_watch.enabled = true;
        cpu.mem_watch.trace_all = false;
        cpu.mem_watch.start = 0x0100;
        cpu.mem_watch.end = 0x0100;
        cpu.run(1000);
    }
    std::cout.rdbuf(old_buf);
    std::string out = capture.str();
    REQUIRE(out.find("MEM W") != std::string::npos);
    REQUIRE(out.find("addr=0x0100") != std::string::npos);
}

TEST(BreakpointsStopRun) {
    Assembler asmblr;
    AsmResult res = asmblr.assemble(R"(
        .ORIG 0
        MOV #1, R0
        MOV #2, R1
        HALT
    )");
    CPU cpu;
    cpu.reset();
    cpu.r[7] = res.start;
    cpu.r[6] = 0xFFFE;
    cpu.load_words(res.start, res.words);
    cpu.breakpoints.insert(0);
    cpu.run(1000);
    REQUIRE(cpu.break_hit);
    REQUIRE(cpu.break_addr == 0);
    REQUIRE(!cpu.halted);
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
