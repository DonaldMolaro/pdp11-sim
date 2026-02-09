; Extended TRAP demo: text I/O + file I/O
.ORIG 0

; Print unsigned + hex
MOV #65535, R0
TRAP #7
MOV #0x002A, R0
TRAP #6

; Read signed int, then hex
TRAP #9
MOV R0, R2
TRAP #10
MOV R0, R3

; Print results
MOV #msg_int, R0
TRAP #3
MOV R2, R0
TRAP #4
MOV #msg_hex, R0
TRAP #3
MOV R3, R0
TRAP #6
TRAP #1

; File I/O: write "Hello" to ./tmp.txt, then read & print
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

msg_int:
.WORD 0x6E49
.WORD 0x2074
.WORD 0x0073

msg_hex:
.WORD 0x6548
.WORD 0x2078
.WORD 0x0073

path:
.WORD 0x2E74
.WORD 0x706D
.WORD 0x742E
.WORD 0x0074

buf:
.WORD 0
.WORD 0
.WORD 0
