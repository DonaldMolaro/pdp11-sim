; Quick demo: small loop and HALT
.ORIG 0
MOV #5, R0
MOV #1, R1
loop:
ADD R0, R1
DEC R0
BNE loop
HALT
