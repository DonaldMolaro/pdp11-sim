; Guess the hidden hex digit in 3 tries.
.ORIG 0

        TRAP #28
        MOV #title, R0
        TRAP #8

        TRAP #27
        BIC #0xFFF0, R0
        MOV R0, R4
        MOV #3, R3

round:
        MOV #prompt, R0
        TRAP #3
        TRAP #10
        CMP R4, R0
        BEQ win

        DEC R3
        BEQ lose
        MOV #wrong, R0
        TRAP #8
        BR round

win:
        MOV #win_msg, R0
        TRAP #3
        MOV R4, R0
        TRAP #6
        MOV #10, R0
        TRAP #1
        HALT

lose:
        MOV #lose_msg, R0
        TRAP #3
        MOV R4, R0
        TRAP #6
        MOV #10, R0
        TRAP #1
        HALT

title:
        .WORD 0x4548
        .WORD 0x2058
        .WORD 0x5548
        .WORD 0x544E
        .WORD 0x0000

prompt:
        .WORD 0x7547
        .WORD 0x7365
        .WORD 0x2073
        .WORD 0x2D30
        .WORD 0x3A46
        .WORD 0x0020

wrong:
        .WORD 0x7257
        .WORD 0x6E6F
        .WORD 0x2167
        .WORD 0x0000

win_msg:
        .WORD 0x6F59
        .WORD 0x2075
        .WORD 0x6977
        .WORD 0x216E
        .WORD 0x4820
        .WORD 0x7865
        .WORD 0x203A
        .WORD 0x0000

lose_msg:
        .WORD 0x6F4C
        .WORD 0x6573
        .WORD 0x202E
        .WORD 0x6548
        .WORD 0x2078
        .WORD 0x0000
