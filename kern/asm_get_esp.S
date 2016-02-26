/** @file asm_get_esp.S
 *
 *  @brief Get current %esp value
 *  
 *  @author Jian Wang (jianwan3)
 *  @author Ke Wu (kewu)
 *  @bug No known bugs
 */
# uint32_t asm_get_esp();

.globl asm_get_esp

asm_get_esp:
movl    %esp, %eax      # Get current %esp value
ret                     # Return current %esp value

