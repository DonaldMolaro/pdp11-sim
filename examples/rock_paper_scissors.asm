; One-round rock-paper-scissors against the simulator.
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
pick_throw:
        TRAP #27
        BIC #0xFFFC, R0
        INC R0
        CMP #4, R0
        BEQ pick_throw
        MOV R0, R2

        MOV #cpu_pick, R0
        TRAP #3
        MOV R2, R0
        JSR R5, print_choice
        MOV #10, R0
        TRAP #1

        CMP R1, R2
        BEQ draw

        MOV R1, R3
        SUB R2, R3
        CMP #1, R3
        BEQ player_win
        CMP #-2, R3
        BEQ player_win
        BR cpu_win

draw:
        MOV #draw_msg, R0
        TRAP #8
        HALT

player_win:
        MOV #win_msg, R0
        TRAP #8
        HALT

cpu_win:
        MOV #lose_msg, R0
        TRAP #8
        HALT

print_choice:
        CMP #1, R0
        BNE choice_two
        MOV #rock_msg, R0
        TRAP #3
        RTS R5

choice_two:
        CMP #2, R0
        BNE choice_three
        MOV #paper_msg, R0
        TRAP #3
        RTS R5

choice_three:
        MOV #scissors_msg, R0
        TRAP #3
        RTS R5

title:
        .WORD 0x4F52
        .WORD 0x4B43
        .WORD 0x5020
        .WORD 0x5041
        .WORD 0x5245
        .WORD 0x5320
        .WORD 0x4943
        .WORD 0x5353
        .WORD 0x524F
        .WORD 0x0053

prompt:
        .WORD 0x3120
        .WORD 0x7220
        .WORD 0x636F
        .WORD 0x206B
        .WORD 0x3220
        .WORD 0x7020
        .WORD 0x7061
        .WORD 0x7265
        .WORD 0x2020
        .WORD 0x3320
        .WORD 0x7320
        .WORD 0x6963
        .WORD 0x7373
        .WORD 0x726F
        .WORD 0x3A73
        .WORD 0x0000

bad_input:
        .WORD 0x7355
        .WORD 0x2065
        .WORD 0x2C31
        .WORD 0x3220
        .WORD 0x726F
        .WORD 0x3320
        .WORD 0x0000

cpu_pick:
        .WORD 0x5043
        .WORD 0x2055
        .WORD 0x6964
        .WORD 0x3A64
        .WORD 0x0020

rock_msg:
        .WORD 0x6F52
        .WORD 0x6B63
        .WORD 0x0000

paper_msg:
        .WORD 0x6150
        .WORD 0x6570
        .WORD 0x0072

scissors_msg:
        .WORD 0x6353
        .WORD 0x7369
        .WORD 0x6F73
        .WORD 0x7372
        .WORD 0x0000

draw_msg:
        .WORD 0x7244
        .WORD 0x7761
        .WORD 0x0021

win_msg:
        .WORD 0x6F59
        .WORD 0x2075
        .WORD 0x6977
        .WORD 0x216E
        .WORD 0x0000

lose_msg:
        .WORD 0x6F59
        .WORD 0x2075
        .WORD 0x6F6C
        .WORD 0x6573
        .WORD 0x0021
