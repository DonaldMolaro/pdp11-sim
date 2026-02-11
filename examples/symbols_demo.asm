.ORIG 0x1000
; Demo for --dump-symbols / --map

start:
    MOV #1, R0
    BR done

middle:
    MOV #2, R0

done:
    HALT
