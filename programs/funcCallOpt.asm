; ===========================================================
;  OPTIMIZED ASSEMBLY: Eliminates 2-cycle POP -> JALR Stalls
;  Strategy: Insert 2 independent instructions after POP R6
;  Only immitates optimizations using dummy instructions
; ===========================================================
MAIN: 
    MOVI R7, 100
    MOVI R1, 2
    JAL R6, funcx
    JAL R6, funcx
    BEQ R0, R0, 0

funcx:
    PUSH R6
    JAL R6, funcy
    JAL R6, funcz
    POP R6
    ADD R7, R7, R0       
    MOVI R5, 0           
    JALR R0, R6, 0       

funcy:
    PUSH R6
    JAL R6, funca
    POP R6
    ADD R1, R1, R0        
    MOVI R5, 0          
    JALR R0, R6, 0     

funcz:
    PUSH R6
    JAL R6, funca
    POP R6 ; -> PC + 1
    ADD R1, R1, R0
    MOVI R5, 0           
    JALR R0, R6, 0      

funca:
    ADD R1, R1, R1
    JALR R0, R6, 0