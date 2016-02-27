.globl asm_pf_handler

/*
Which mode is the program in before hits in this wrapper?
Do I need to save registers?
*/

asm_pf_handler:
pusha                   # Save general purpose registers
call    pf_handler   
popa                    # Restore general purpose registers
iret                    # Return from interrupt handler


