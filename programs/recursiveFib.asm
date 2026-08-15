; ================================================================================
; Program: Recursive Fibonacci (Byte-Addressed CPU with Add/Store sequence)
; Recursive Fibonacci (n = 0 to 10)
; Memory Map:
;   0x0100 (256) -> Stack Pointer Base (R7)
;   0x0200 (512) -> Output RAM Base Address (R3)
; This code only take 6455 cycles to complete 11 iterations
; Standard RISC based Fib sequence with no PUSH and POP using bytes Addressed Memo
; =================================================================================

MAIN:
    ; 1. Initialize Stack Pointer (R7 = 256) and Output Address (R3 = 512)
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
    JAL     R6, FIB           ; Call FIB; Return address saved in R6 (LR)

    ; 3. Store result to RAM: RAM[R3 + (i * 4)] = R1
    ADD     R4, R2, R2        ; R4 = 2 * i
    ADD     R4, R4, R4        ; R4 = 4 * i (Word offset)
    ADD     R5, R3, R4        ; R5 = 0x0200 + (4 * i)
    STORE   R5, R1, 0         ; RAM[R5 + 0] = R1

    ADDI    R2, R2, 1         ; i++
    BEQ     R0, R0, MAIN_LOOP ; Unconditional loop jump

DONE:
    BEQ     R0, R0, DONE      ; Infinite loop (terminates execution)


; ====================================================================
; Subroutine: FIB(n)
; Input:  R1 = n
; Output: R1 = fib(n)
; ====================================================================
FIB:
    ; Base Case 1: n == 0 -> Return 0
    BNE     R1, R0, FIB_NOT_ZERO
    ADDI    R1, R0, 0
    JALR    R0, R6, 0         ; Return to caller via R6

FIB_NOT_ZERO:
    ; Base Case 2: n == 1 -> Return 1
    ADDI    R4, R0, 1
    BNE     R1, R4, FIB_RECURSE
    ADDI    R1, R0, 1
    JALR    R0, R6, 0         ; Return to caller via R6

FIB_RECURSE:
    ; ----------------------------------------------------------------
    ; Recursive Case: fib(n) = fib(n - 1) + fib(n - 2)
    ; Stack Frame (R7) Size = 12 Bytes (3 words):
    ;   0(R7) : Saved Return Address (R6 / LR)
    ;   4(R7) : Saved Argument (n)
    ;   8(R7) : Intermediate fib(n - 1) result
    ; ----------------------------------------------------------------
    ADDI    R7, R7, -12       ; Allocate 12 bytes on stack
    STORE   R7, R6, 0         ; Push Return Address (R6)
    STORE   R7, R1, 4         ; Push argument n (R1)

    ; Step A: Compute fib(n - 1)
    ADDI    R1, R1, -1        ; R1 = n - 1
    JAL     R6, FIB           ; Recursive call
    STORE   R7, R1, 8         ; Save fib(n - 1) result

    ; Step B: Compute fib(n - 2)
    LOAD    R1, R7, 4         ; Restore original n from stack
    ADDI    R1, R1, -2        ; R1 = n - 2
    JAL     R6, FIB           ; Recursive call -> R1 holds fib(n - 2)

    ; Step C: Combine results
    LOAD    R4, R7, 8         ; Load fib(n - 1) into R4
    ADD     R1, R4, R1        ; R1 = fib(n - 1) + fib(n - 2)

    ; Epilogue: Clean up stack frame and return
    LOAD    R6, R7, 0         ; Restore original Return Address (R6)
    ADDI    R7, R7, 12        ; Deallocate stack frame
    JALR    R0, R6, 0         ; Return to caller