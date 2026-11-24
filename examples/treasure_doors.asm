; Pick one of three doors. One hides treasure.
.ORIG 0

start:
        TRAP #28
        MOV #title, R0
        TRAP #8
        MOV #prompt, R0
        TRAP #3
        TRAP #9
        MOV R0, R1

        CMP #1, R1
        BEQ valid
        CMP #2, R1
        BEQ valid
        CMP #3, R1
        BEQ valid
        MOV #bad_input, R0
        TRAP #8
        HALT

valid:
pick_door:
        TRAP #27
        BIC #0xFFFC, R0
        INC R0
        CMP #4, R0
        BEQ pick_door
        MOV R0, R4

        CMP R4, R1
        BEQ win

        MOV #lose_msg, R0
        TRAP #3
        MOV R4, R0
        TRAP #4
        MOV #10, R0
        TRAP #1
        HALT

win:
        MOV #win_msg, R0
        TRAP #8
        HALT

title:
        .WORD 0x5254
        .WORD 0x4145
        .WORD 0x5553
        .WORD 0x4552
        .WORD 0x4420
        .WORD 0x4F4F
        .WORD 0x5352
        .WORD 0x0000

prompt:
        .WORD 0x6950
        .WORD 0x6B63
        .WORD 0x3120
        .WORD 0x3220
        .WORD 0x3320
        .WORD 0x003A
        .WORD 0x0000

bad_input:
        .WORD 0x7355
        .WORD 0x2065
        .WORD 0x2C31
        .WORD 0x3220
        .WORD 0x726F
        .WORD 0x3320
        .WORD 0x0000

lose_msg:
        .WORD 0x6F4E
        .WORD 0x6570
        .WORD 0x202E
        .WORD 0x6854
        .WORD 0x2065
        .WORD 0x7274
        .WORD 0x6165
        .WORD 0x7573
        .WORD 0x6572
        .WORD 0x7720
        .WORD 0x7361
        .WORD 0x6420
        .WORD 0x6F6F
        .WORD 0x2072
        .WORD 0x0000

win_msg:
        .WORD 0x6F59
        .WORD 0x2075
        .WORD 0x6F66
        .WORD 0x6E75
        .WORD 0x2064
        .WORD 0x6874
        .WORD 0x2065
        .WORD 0x7274
        .WORD 0x6165
        .WORD 0x7573
        .WORD 0x2165
        .WORD 0x0000
