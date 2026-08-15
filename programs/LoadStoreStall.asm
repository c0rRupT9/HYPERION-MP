; Load store hazard case 
MOVI R1, 3       ; Move 3 to R1 
STORE R0, R1, 0 ; Store R1 at mem[0]
LOAD R2, R0, 0  ; Load mem[0] to R2  R2 -> 3
STORE R2, R1, 0 ; Store 3 at mem[3] -> Will invoke a loadUseStall