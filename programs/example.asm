; Example code for explaining RAS


; Program's Entry Point

main:

MOVI R7, 20 ; Setup Sp

add_numbers:
MOVI R2, 5
MOVI R3, 6
JAL R6, function_Add
; word 4
; Return Point
PUSH R1

add_numbers2:
MOVI R2, 1
ADDI, R3, R0, 6
JAL R6, function_Add
;word pc -> adress 8
; Return Point

PUSH R1

BEQ R0, R0, 0
; HALT condition

function_Add: 
add R1, R2, R3
JALR R0, R6, 0  ; Return to Calee RET
