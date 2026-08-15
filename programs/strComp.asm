; =========================================================================
; HYPERION String Comparison Test Program
; =========================================================================

MAIN:
    ; ---------------------------------------------------------------------
    ; 1. Store String 1 ("HI\0") into RAM starting at Word Address 10
    ; ---------------------------------------------------------------------
    MOVI  R1, 10              ; Base address of Str1 = Word 10
    
    MOVI  R4, 72              ; ASCII 'H' (0x48)
    STORE R1, R4, 0           ; MEM[10 + 0] = 'H'
    MOVI  R4, 73              ; ASCII 'I' (0x49)
    STORE R1, R4, 1           ; MEM[10 + 1] = 'I'
    MOVI  R4, 0               ; ASCII '\0'
    STORE R1, R4, 2           ; MEM[10 + 2] = '\0'

    ; ---------------------------------------------------------------------
    ; 2. Store String 2 ("HI\0") into RAM starting at Word Address 14
    ; ---------------------------------------------------------------------
    MOVI  R2, 14              ; Base address of Str2 = Word 14
    
    MOVI  R4, 72              ; ASCII 'H'
    STORE R2, R4, 0           ; MEM[14 + 0] = 'H'
    MOVI  R4, 73              ; ASCII 'I'
    STORE R2, R4, 1           ; MEM[14 + 1] = 'I'
    MOVI  R4, 0               ; ASCII '\0'
    STORE R2, R4, 2           ; MEM[14 + 2] = '\0'

    ; ---------------------------------------------------------------------
    ; 3. Call strcmp(R1=10, R2=14)
    ; ---------------------------------------------------------------------
    JAL   R6, strcmp          ; Jump to strcmp, save return PC in R6
    
HALT:
    BEQ   R0, R0, HALT        ; Infinite loop when done (R3 holds result: 1=equal, 0=not equal)


; =========================================================================
; strcmp Routine
; Inputs:  R1 = Pointer to String 1 (word address)
;          R2 = Pointer to String 2 (word address)
;          R6 = Return address
; Output:  R3 = 1 if equal, 0 if not equal
; Temp:    R4 = char1, R5 = char2
; =========================================================================
strcmp:
loop:
    LOAD  R4, R1, 0           ; Load 16-bit word from Str1
    LOAD  R5, R2, 0           ; Load 16-bit word from Str2

    BNE   R4, R5, not_equal   ; If char1 != char2 -> Mismatch!
    BEQ   R4, R0, equal       ; If char1 == '\0' (and matched char2) -> Strings match!

    ADDI  R1, R1, 1           ; Move to next word address
    ADDI  R2, R2, 1           ; Move to next word address
    BEQ   R0, R0, loop        ; Unconditional loop back

not_equal:
    MOVI  R3, 0               ; Return 0
    JALR  R0, R6, 0           ; Return to caller

equal:
    MOVI  R3, 1               ; Return 1
    JALR  R0, R6, 0           ; Return to caller