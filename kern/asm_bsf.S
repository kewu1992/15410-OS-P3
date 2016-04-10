# int asm_bsf(uint32_t value);
.global asm_bsf

asm_bsf:
    bsf 4(%esp), %eax
    ret
