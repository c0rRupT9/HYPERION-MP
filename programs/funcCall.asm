
; ===========================================================
;   R6 -> Link Address Register -> jax 
;   Link tree R6 push before overwriting
;   return using JALR R0, R6 POP on return 
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
    JALR R0, R6, 0

funcy:
    PUSH R6
    JAL R6, funca
    POP R6
    JALR R0, R6, 0

funcz:
    PUSH R6
    JAL R6, funca
    POP R6
    JALR R0, R6, 0


funca:
    ADD R1, R1, R1
    JALR R0, R6, 0 ; Return to calee



 