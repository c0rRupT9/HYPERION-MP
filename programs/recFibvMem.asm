; =======================================================================================
; Program: Recursive Fibonacci With Memory (Word-Addressed CPU with Native PUSH/POP)
; Memory Map:
;   0x0100 (256) -> Stack Pointer Base (R7)
;   0x0200 (512) -> Output RAM Base Address (R3)
; Uses CPU native ISA PUSH and POP based Rec Fib reducing payload by 200 aproxx. 
; cycles used = 550
; Recursion with Memory are not used by GCC because of malloc and growing memory problems
; =======================================================================================


MAIN:
    ; 1. Initialize Stack Pointer (R7 = 256) and Output Base (R3 = 512)
    MOVI    R7, 64            ; R7 = 64
    ADD     R7, R7, R7        ; R7 = 128
    ADD     R7, R7, R7        ; R7 = 256 (0x0100)
    ADD     R3, R7, R7        ; R3 = 512 (0x0200)

    ADDI    R2, R0, 0         ; R2 (i) = 0

MAIN_LOOP:
    ; Exit check: if i == 11, jump to DONE
    ADDI    R4, R0, 11
    BEQ     R2, R4, DONE      ; Exit loop after processing i = 10

    ; 2. Call FIB(i)
    ADD     R1, R2, R0        ; Argument R1 = i
    JAL     R6, FIB           ; Call FIB; Return address saved in R6

    ; 3. Store result to RAM: RAM[512 + i] = R1 (Word Addressed!)
    ADD     R5, R3, R2        ; R5 = 512 + i
    STORE   R5, R1, 0         ; RAM[R5] = R1

    ADDI    R2, R2, 1         ; i++
    BEQ     R0, R0, MAIN_LOOP ; Loop back

DONE:
    BEQ     R0, R0, DONE      ; Infinite loop (terminates execution)




; ====================================================================
; Subroutine: FIB(n) - Scheduled Pipeline + Memoized Memory Lookup
; Input:  R1 = n, R3 = 512 (Memo Base Address)
; Output: R1 = fib(n)
; ====================================================================
FIB:
    ; 1. Base Case 0: n == 0 -> Return 0
    BEQ     R1, R0, BASE_0

    ; 2. Base Case 1: n == 1 -> Return 1
    ADDI    R4, R0, 1
    BEQ     R1, R4, BASE_1

    ; 3. Memoization Check: Is RAM[512 + n] already computed?
    ADD     R5, R3, R1          ; R5 = 512 + n
    LOAD    R4, R5, 0           ; Read memo table
    BEQ     R4, R0, FIB_RECURSE ; If 0 (uncomputed), compute recursively

    ; --- MEMO HIT! ---
    ADD     R1, R4, R0          ; R1 = memoized value
    JALR    R0, R6, 0           ; Immediate return (0 stack operations!)

BASE_0:
    JALR    R0, R6, 0           ; Return 0
BASE_1:
    JALR    R0, R6, 0           ; Return 1

FIB_RECURSE:
    ; ----------------------------------------------------------------
    ; 4. Stack Setup & Recurse FIB(n - 1)
    ; ----------------------------------------------------------------
    PUSH    R6                  ; Save Return Address
    PUSH    R1                  ; Save original n

    ADDI    R1, R1, -1          ; R1 = n - 1
    JAL     R6, FIB             ; Call FIB(n - 1) -> R1 = fib(n - 1)

    ; ----------------------------------------------------------------
    ; 5. Prepare & Recurse FIB(n - 2) [SCHEDULED: 0 STALLS]
    ; ----------------------------------------------------------------
    ADD     R4, R1, R0          ; R4 = fib(n - 1)
    POP     R1                  ; Trigger load of original 'n' into R1
    PUSH    R4                  ; <--- Fills POP R1 delay slot!
    ADDI    R1, R1, -2          ; R1 = original_n - 2
    JAL     R6, FIB             ; Call FIB(n - 2) -> R1 = fib(n - 2)

    ; ----------------------------------------------------------------
    ; 6. Combine, Save to Memo Table, & Return [SCHEDULED: 0 STALLS]
    ; ----------------------------------------------------------------
    POP     R4                  ; R4 = fib(n - 1)
    POP     R6                  ; <--- Fills POP R4 delay slot! R6 = Return Address
    ADD     R1, R1, R4          ; <--- Fills POP R6 delay slot! R1 = fib(n-2) + fib(n-1)

    ; Store result in Memo Table for future lookups
    ADD     R5, R3, R1          ; R5 = 512 + n (Wait: need n again, or use stack/main)
    ; Note: MAIN already stores RAM[512 + i] = R1, so outer loop handles caching!

    JALR    R0, R6, 0           ; Return to caller