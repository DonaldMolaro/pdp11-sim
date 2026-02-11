.ORIG 0x1000
; Demo for --watch / --trace-mem
; Expected watch on 0x0100:
;   MEM W ... addr=0x0100 ...
;   MEM R ... addr=0x0100 ...

MOV #0x0100, R0
MOV #0x00AA, (R0)
MOV (R0), R1
HALT
