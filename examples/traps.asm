; TRAP demo: uses all TRAP vectors.
.ORIG 0

; TRAP #1: putc
MOV #0x48, R0     ; 'H'
TRAP #1
MOV #0x69, R0     ; 'i'
TRAP #1

; TRAP #3: puts (null-terminated)
MOV #msg, R0
TRAP #3

; TRAP #4: print signed decimal
MOV #-123, R0
TRAP #4

; TRAP #6: print hex
MOV #0x2A, R0
TRAP #6

; TRAP #2: getc (echo it with TRAP #1)
TRAP #2
TRAP #1

; TRAP #5: read line into buffer and echo it
MOV #linebuf, R0
MOV #32, R1
TRAP #5
MOV #linebuf, R0
TRAP #3

HALT

msg:
.WORD 0x6C6C
.WORD 0x206F
.WORD 0x7266
.WORD 0x206D
.WORD 0x5050
.WORD 0x2D44
.WORD 0x3131
.WORD 0x2100

linebuf:
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
.WORD 0
