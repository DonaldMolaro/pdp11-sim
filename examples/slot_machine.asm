; Tiny slot machine demo using TRAP #27, #28, and #29.
.ORIG 0

start:
        TRAP #28
        MOV #title, R0
        TRAP #8
        MOV #prompt, R0
        TRAP #3
        MOV #linebuf, R0
        MOV #8, R1
        TRAP #5

        MOV #linebuf, R1
        MOVB (R1), R0
        CMPB #113, R0
        BEQ quit
        CMPB #81, R0
        BEQ quit

        MOV #spinning, R0
        TRAP #8

        MOV #450, R0
        TRAP #29
        TRAP #27
        BIC #0xFFFC, R0
        MOV R0, R1

        MOV #450, R0
        TRAP #29
        TRAP #27
        BIC #0xFFFC, R0
        MOV R0, R2

        MOV #450, R0
        TRAP #29
        TRAP #27
        BIC #0xFFFC, R0
        MOV R0, R3

        MOV #result, R0
        TRAP #3

        MOV #91, R0
        TRAP #1
        MOV R1, R0
        JSR R5, print_symbol
        MOV #124, R0
        TRAP #1
        MOV R2, R0
        JSR R5, print_symbol
        MOV #124, R0
        TRAP #1
        MOV R3, R0
        JSR R5, print_symbol
        MOV #93, R0
        TRAP #1
        MOV #10, R0
        TRAP #1

        CMP R1, R2
        BNE lose
        CMP R2, R3
        BNE lose
        MOV #jackpot, R0
        TRAP #8
        BR pause_result

lose:
        MOV #again, R0
        TRAP #8
pause_result:
        MOV #900, R0
        TRAP #29
        BR start

quit:
        MOV #bye, R0
        TRAP #8
        HALT

print_symbol:
        CMP #0, R0
        BNE sym1
        MOV #79, R0
        TRAP #1
        RTS R5
sym1:
        CMP #1, R0
        BNE sym2
        MOV #88, R0
        TRAP #1
        RTS R5
sym2:
        CMP #2, R0
        BNE sym3
        MOV #42, R0
        TRAP #1
        RTS R5
sym3:
        MOV #36, R0
        TRAP #1
        RTS R5

title:
        .WORD 0x4552
        .WORD 0x5254
        .WORD 0x204F
        .WORD 0x4C53
        .WORD 0x544F
        .WORD 0x0053

prompt:
        .WORD 0x6E45
        .WORD 0x6574
        .WORD 0x3D72
        .WORD 0x7073
        .WORD 0x6E69
        .WORD 0x7120
        .WORD 0x713D
        .WORD 0x6975
        .WORD 0x3A74
        .WORD 0x0020
        .WORD 0x0000

spinning:
        .WORD 0x7053
        .WORD 0x6E69
        .WORD 0x696E
        .WORD 0x676E
        .WORD 0x2E2E
        .WORD 0x002E

result:
        .WORD 0x6552
        .WORD 0x7573
        .WORD 0x746C
        .WORD 0x203A
        .WORD 0x0000

jackpot:
        .WORD 0x414A
        .WORD 0x4B43
        .WORD 0x4F50
        .WORD 0x2154
        .WORD 0x0000

again:
        .WORD 0x7254
        .WORD 0x2079
        .WORD 0x6761
        .WORD 0x6961
        .WORD 0x216E
        .WORD 0x0000

bye:
        .WORD 0x7942
        .WORD 0x2165
        .WORD 0x0000

linebuf:
        .WORD 0
        .WORD 0
        .WORD 0
        .WORD 0
