; Comprehensive instruction demo (no input required)
.ORIG 0

; Basic moves and arithmetic
MOV #0x1234, R0
MOV #10, R1
ADD R1, R0
SUB #3, R0
CMP R0, R1

; Bitwise ops
MOV #0x00FF, R2
MOV #0x0F0F, R3
BIT R2, R3
BIC R2, R3
BIS #0x0003, R3

; Single operand ops
CLR R4
INC R4
DEC R4
TST R4

; Shifts and rotates
MOV #0x4000, R5
ASL R5
MOV #0x8001, R6
ASR R6
ROR R6
ROL R6

; Byte operations on memory
MOV #buf, R0
MOVB #0x41, (R0)   ; 'A'
INCB (R0)
DECB (R0)
TSTB (R0)
MOVB (R0), R1
BITB #0x01, R1
BICB #0x01, R1
BISB #0x02, R1
CLRB (R0)

; Branches (avoid R7, which is PC)
MOV #2, R2
loop:
DEC R2
BNE loop
BEQ after_beq
BR after_beq

after_beq:
; JSR/RTS
JSR R5, sub
JMP done

sub:
INC R4
RTS R5

done:
HALT

buf:
.WORD 0
