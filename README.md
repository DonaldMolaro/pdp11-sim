# PDP-11 Assembly Simulator

This project provides a small DEC PDP-11 simulator with a built-in assembler for a focused instruction subset, plus a lightweight test framework.

## Supported Instructions
- Word: `MOV`, `ADD`, `SUB`, `CMP`, `BIT`, `BIC`, `BIS`
- Byte: `MOVB`, `CMPB`, `BITB`, `BICB`, `BISB`
- Single operand: `CLR`, `INC`, `DEC`, `TST`, `ROR`, `ROL`, `ASR`, `ASL`
- Byte single operand: `CLRB`, `INCB`, `DECB`, `TSTB`
- Control: `JMP`, `JSR`, `RTS`, `BR`, `BEQ`, `BNE`, `HALT`
- Syscalls: `TRAP #vector` (see I/O below)

## Addressing Modes
- Register: `R0`
- Register deferred: `(R0)`
- Autoincrement: `(R0)+`
- Autodecrement: `-(R0)`
- Indexed: `X(R0)`
- Immediate: `#value`
- PC-relative: `label`
- Absolute: `@#value`

## Directives
- `.ORIG addr`
- `.WORD value`

## Numeric Literals
- Default base is decimal.
- Use `0o` for octal and `0x` for hex (e.g., `0o177777`, `0xFF`).

## Build
```sh
cmake -S . -B build
cmake --build build
```
Or use `make build`.

## Run
```sh
./build/pdp11sim examples/demo.asm
```

### Trace Mode
```sh
./build/pdp11sim examples/demo.asm --trace
```

### Memory Watch / Trace
```sh
./build/pdp11sim examples/demo.asm --watch=0x0100:16
./build/pdp11sim examples/demo.asm --trace-mem
```

### Symbol Map
```sh
./build/pdp11sim examples/demo.asm --dump-symbols
./build/pdp11sim examples/demo.asm --map out.map
```

## Demos
- `examples/demo.asm`: quick loop demo
- `examples/all_instructions.asm`: exercises all implemented instructions
- `examples/traps.asm`: exercises all TRAP I/O vectors (expects input)
- `examples/traps_extended.asm`: exercises extended TRAP text and file I/O (expects input)

Run all demos:
```sh
make demo
```
The `demo` target uses `expect` to provide input for `examples/traps.asm` and `examples/traps_extended.asm`.

## Simple I/O (TRAP)
The simulator provides a minimal TRAP interface:
- `TRAP #1`: output the low byte of `R0` as a character.
- `TRAP #2`: read one character into low byte of `R0` (sets `Z` if EOF).
- `TRAP #3`: output a null-terminated string starting at address in `R0`.
- `TRAP #4`: print signed decimal value from `R0`.
- `TRAP #5`: read a line into buffer at `R0` with max size `R1` (includes null). Returns length in `R0`.
- `TRAP #6`: print hex value from `R0` (format `0xNNNN`).
- `TRAP #7`: print unsigned decimal value from `R0`.
- `TRAP #8`: print string at `R0` followed by newline.
- `TRAP #9`: read signed decimal into `R0` (skips leading whitespace).
- `TRAP #10`: read hex into `R0` (accepts optional `0x`).
- `TRAP #20`: open file at address `R0`, mode in `R1` (0=read,1=write,2=append,3=read/write). Returns handle in `R0`, `0xFFFF` on failure.
- `TRAP #21`: read file handle in `R0` into buffer `R1`, max bytes `R2`. Returns bytes read in `R0`.
- `TRAP #22`: write file handle in `R0` from buffer `R1`, bytes `R2`. Returns bytes written in `R0`.
- `TRAP #23`: close file handle in `R0`, returns `0` on success or `0xFFFF` on failure.
- `TRAP #24`: seek file handle in `R0` by signed offset `R1`, origin `R2` (0=beg,1=cur,2=end). Returns `0` or `0xFFFF`.
- `TRAP #25`: tell file handle in `R0`. Returns position in `R0` (low 16 bits) or `0xFFFF` on failure.
- `TRAP #26`: set data memory bank (`R0` = 0..3). Instruction fetch stays in bank 0; data uses `bank << 16 | addr`.

## Banked Memory (256K)
The simulator provides 4 data banks of 64K each (total 256K). Instruction fetch is always from bank 0. Data reads/writes use the current bank selected by `TRAP #26`.

### How It Works
- **Code**: always read from bank 0.
- **Data**: physical address = `(bank << 16) | addr16`.
- **Bank select**: `TRAP #26` with `R0 = 0..3`.

### Example
```asm
.ORIG 0
; Write to bank 0
MOV #0, R0
TRAP #26
MOV #0x0100, R5
MOV #123, R1
MOV R1, (R5)

; Switch to bank 1 and read (will be 0)
MOV #1, R0
TRAP #26
MOV (R5), R2

; Write to bank 1
MOV #1, R1
MOV R1, (R5)

; Switch back to bank 0 and read original 123
MOV #0, R0
TRAP #26
MOV (R5), R3

HALT
```
Expected results:
- `R2 = 0`
- `R3 = 123`

## Tests
```sh
./build/pdp11_tests
```
Or use `make test`.
