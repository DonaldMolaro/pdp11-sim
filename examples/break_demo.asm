.ORIG 0x1000
; Demo for --break=label
; Run: ./build/pdp11sim examples/break_demo.asm --break=loop
; Expect: BREAK at 0x....

MOV #3, R0
loop:
DEC R0
BNE loop
HALT
