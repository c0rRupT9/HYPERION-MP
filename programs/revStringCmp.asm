; =====================================================================
; revStringCmp.asm (Rescheduled for Zero Hazard Stalls)
; =====================================================================

MAIN:
    MOVI R1, 10
    MOVI R4, 97
    STORE R1, R4, 0
    MOVI R4, 100
    STORE R1, R4, 1
    MOVI R4, 97
    STORE R1, R4, 2
    MOVI R4, 0
    STORE R1, R4, 3

    MOVI R2, 14
    MOVI R4, 97
    STORE R2, R4, 0
    MOVI R4, 100
    STORE R2, R4, 1
    MOVI R4, 97
    STORE R2, R4, 2
    MOVI R4, 0
    STORE R2, R4, 3

    JAL R6, rev_strcmp

HALT:
    BEQ R0, R0, 0

; =====================================================================
; rev_strcmp: Compare string at R1 (reversed) with string at R2 (forward)
; Output: R3 = 1 if match, R3 = 0 if not match
; =====================================================================
rev_strcmp:
    ADD  R4, R1, R0       ; R4 = str1 base pointer
    LOAD R5, R4, 0        ; Prime R5 with str1[0] before entering loop

find_end_loop:
    ADDI R4, R4, 1        ; Advance str1 pointer
    ADD  R7, R5, R0       ; Copy current character into R7
    LOAD R5, R4, 0        ; Pre-load NEXT character into R5
    BNE  R7, R0, find_end_loop ; Branch on R7 (ADD was 2 instructions ago -> ZERO STALL)

found_end:
    ADDI R4, R4, -2       ; Point R4 to last valid char of str1

compare_loop:
    LOAD R5, R4, 0        ; R5 = str1[i] (reverse)
    LOAD R3, R2, 0        ; R3 = str2[j] (forward)
    ADDI R4, R4, -1       ; Decrement str1 index
    SLT  R7, R4, R1       ; Compute bounds check R7 = (R4 < R1)
    ADDI R2, R2, 1        ; Increment str2 index (acts as delay slot after SLT)
    BNE  R5, R3, not_equal; Branch if chars mismatch (LOADs were 3+ cycles ago -> ZERO STALL)
    BEQ  R7, R0, compare_loop ; Loop if not done (SLT was 2 cycles ago -> ZERO STALL)

str1_done:
    LOAD R5, R2, 0        ; Load remaining str2 character
    MOVI R3, 1            ; Pre-set return value R3 = 1 (equal)
    ADD  R0, R0, R0       ; Delay slot instruction to fill load-branch gap
    BEQ  R5, R0, equal_done; If str2 is also at null terminator, return R3=1

not_equal:
    MOVI R3, 0            ; Return R3 = 0 (not equal)

equal_done:
    JALR R0, R6, 0        ; Return to caller