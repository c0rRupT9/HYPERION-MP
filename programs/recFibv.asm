; =======================================================================
; Program: Recursive Fibonacci (Word-Addressed CPU with Native PUSH/POP)
; Memory Map:
;   0x0100 (256) -> Stack Pointer Base (R7)
;   0x0200 (512) -> Output RAM Base Address (R3)
; Uses CPU native ISA PUSH and POP based Rec Fib reducing payload by 1000 
; cycles used = 5538 diff after ras 220
; =======================================================================

; ========================================================================
;  R0 -> Hardwired Zero (R0 = 0)
;  R1 -> Return Value / Accumulator
;  R2 -> Input Parameter 'n' (for Fib(n))
;  R3 -> Output Base Address (Increments for memory writes)
;  R4 -> Saved Temporary (Preserves Fib(n-1) across second call)
;  R5 -> Scratch / Temporary Register (Base case comparisons)
;  R6 -> Link Address Register (RA / Return Address)
;  R7 -> Stack Pointer (SP initialized to 256)
; =========================================================================


MAIN:
    ; 1. Initialize Stack Pointer (R7 = 256) and Output Base (R3 = 512)
    MOVI    R7, 64            ; R7 = 64
    ADD     R7, R7, R7        ; R7 = 128
    ADD     R7, R7, R7        ; R7 = 256 (0x0100)
    ADD     R3, R7, R7        ; R3 = 512 (0x0200)

    ADDI    R2, R0, 0         ; R2 (i) = 0

MAIN_LOOP:
    ; Exit check: if i == 11, jump to DONE
    MOVI    R4, 21        ; Use MOVI for depth > 15 
    BEQ     R2, R4, DONE      ; Exit loop after processing i = 10

    ; 2. Call FIB(i)
    ADD     R1, R2, R0        ; Argument R1 = i
    JAL     R6, FIB           ; Call FIB; Return address saved in R6


; Return 1
    ; 3. Store result to RAM: RAM[512 + i] = R1 (Word Addressed!)
    ADD     R5, R3, R2        ; R5 = 512 + i
    STORE   R5, R1, 0         ; RAM[R5] = R1

    ADDI    R2, R2, 1         ; i++
    BEQ     R0, R0, MAIN_LOOP ; Loop back

DONE:
    BEQ     R0, R0, DONE      ; Infinite loop (terminates execution)


; ====================================================================
; Subroutine: FIB(n)
; Input:  R1 = n
; Output: R1 = fib(n)
; ====================================================================
; ====================================================================
; Subroutine: FIB(n) - Fully Scheduled for 0 Load-Use Stalls
; ====================================================================
FIB:
    ; Base Case 1: n == 0 -> Return 0
    BNE     R1, R0, FIB_NOT_ZERO
    JALR    R0, R6, 0           ; Return 0

FIB_NOT_ZERO:
    ; Base Case 2: n == 1 -> Return 1
    ADDI    R4, R0, 1
    BNE     R1, R4, FIB_RECURSE
    JALR    R0, R6, 0           ; Return 1

FIB_RECURSE:
    ; 1. Save frame state to stack
    PUSH    R6                  ; Push Return Address
    PUSH    R1                  ; Push original n

    ; 2. Compute FIB(n - 1)
    ADDI    R1, R1, -1          ; R1 = n - 1
    JAL     R6, FIB             ; Call FIB(n - 1) -> R1 holds fib(n - 1)

    ; ----------------------------------------------------------------
    ; 3. Prepare for FIB(n - 2) [SCHEDULED: 0 STALLS]
    ; ----------------------------------------------------------------
    ADD     R4, R1, R0          ; R4 = fib(n - 1)
    POP     R1                  ; Trigger load of original 'n' into R1
    PUSH    R4                  ; <--- Fills POP R1 delay slot! (Push fib(n-1) to stack)
    ADDI    R1, R1, -2          ; R1 is ready! R1 = original_n - 2
    JAL     R6, FIB             ; Call FIB(n - 2) -> R1 holds fib(n - 2)

    ; ----------------------------------------------------------------
    ; 4. Combine Results & Return [SCHEDULED: 0 STALLS]
    ; ----------------------------------------------------------------
    POP     R4                  ; Trigger load of fib(n - 1) into R4
    POP     R6                  ; <--- Fills POP R4 delay slot! Trigger load of R6
    ADD     R1, R1, R4          ; <--- Fills POP R6 delay slot! R1 = fib(n-2) + fib(n-1)
    JALR    R0, R6, 0           ; Return to caller (R6 is fully ready!)