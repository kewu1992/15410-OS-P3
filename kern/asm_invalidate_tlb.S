/** @file asm_invalidate_tlb.S
 *
 *  @brief This file contains implementation of an assembly function that
 *  invalidates the TLB entry for a page
 *
 *  @author Jian Wang (jianwan3)
 *
 *  @bug No known bugs.
 */


.globl asm_invalidate_tlb

asm_invalidate_tlb:
movl 4(%esp), %eax  # Put the page addr to invalidate in %eax
invlpg (%eax)       # invalidate tlb for specified page addr
ret

