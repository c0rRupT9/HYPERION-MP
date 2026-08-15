 MOVI R7, 10       ; Setup SP
 PUSH R4, 1        ; Store R4 to stack
 POP  R4, 1        ; Load R4 back (Result available in MEM/WB!)
 BEQ  R4, R0, 5    ; IMPLICIT COLLISION: DEC needs R4 immediately after POP
 MOVI R1, 99   ; Should NOT execute if R4 != 0