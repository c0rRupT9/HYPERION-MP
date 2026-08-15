; --- SETUP ---
MOVI RA, 5       ; Main Counter (N)
MOVI RB, 1       ; Accumulator (The Result)

loop_main:
    MOV RC, RA      ; Use RC to count how many additions to do
    MOV RD, RB      ; RD holds the "base" to be added
    MOVI RB, 0      ; Clear RB to start adding into it

multiply_loop:
    ADD RB, RD      ; Add base to total
    MOVI RD, 1      ; We need to decrement RC
    SUB RC, RD      ; RC = RC - 1
    MOV RD, RB     ; (Put RB back in RD for next addition)
    JZ check_done   ; If RC is 0, we finished multiplying
    JMP multiply_loop

check_done:
    MOVI RD, 1      ; Use RD to decrement our main counter
    SUB RA, RD      ; RA = RA - 1
    JZ display      ; If RA is 0, we are totally finished!
    JMP loop_main

display:
    MOVI RD, 254    ; Target DRAM Address 0xFF
    STRD RD, RB     ; Save result to the very last byte of DRAM
    OUT RB          ; Output 0x78
    HLT