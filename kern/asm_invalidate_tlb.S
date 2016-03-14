.globl asm_invalidate_tlb

asm_invalidate_tlb:
movl 4(%esp), %eax  # Put the page addr to invalidate in %eax
invlpg (%eax)       # invalidate tlb for specified page addr
ret

