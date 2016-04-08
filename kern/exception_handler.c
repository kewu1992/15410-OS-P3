/** @brief Exception handler 
  *
  *
  *
  */

#include <exception_handler.h>
#include <vm.h>
#include <asm_helper.h>
#include <control_block.h>
#include <loader.h>
#include <syscall_lifecycle.h>

/** @brief Put register values saved on stakc to ureg struct */
static void get_ureg(ureg_t *ureg, uint32_t *ebp, int has_error_code) {

    // According to register saving code custom in handler_wrapper.S,
    // with ebp, the offset to saved register values are constant
    /* The stack looks like the following:
       SS 
       ESP
       EFLAGS
       CS
       EIP
       error code [whether exists or not depends on has_error_code]
       EAX 
       ECX 
       DDX 
       EBX 
       ESP 
       EBP 
       ESI 
       EDI
       Ret addr
       DS
       ES
       FS
       GS
       param
       Ret addr
       old EBP <---- %EBP  
       */
    // Get segment registers
    memcpy((void *)(&ureg->ds), (void *)(ebp + 3), 4 * sizeof(uint32_t));
    // Get general purpose registers
    memcpy((void *)(&ureg->edi), (void *)(ebp + 8), 8 * sizeof(uint32_t)); 
    // Set dummy %esp
    ureg->zero = 0;
    // Get eip, cs, eflags, esp, ss 
    memcpy((void *)(&ureg->eip), (void *)(ebp + (16 + has_error_code)), 
            5 * sizeof(uint32_t)); 
    // Get error code
    ureg->error_code = has_error_code ? *(ebp + 16) : 0;

}
/** @brief Dump register values in ureg
  *
  * @param tid The thread tid
  * @param ureg The struct that contains the register values to dump
  *
  * @return void
  */
static void dump_register(int tid, ureg_t *ureg) {

    lprintf("Register dump for thread tid %d:\n "
            "cause: 0x%x, cr2: 0x%x, ds: 0x%x, es: 0x%x, fs: 0x%x, gs: 0x%x\n"
            "edi: 0x%x, esi: 0x%x, ebp: 0x%x, zero: 0x%x, ebx: 0x%x, edx: 0x%x\n"
            "ecx: 0x%x, eax: 0x%x, error code: 0x%x, eip: 0x%x, cs: 0x%x, eflags: 0x%x\n"
            "esp: 0x%x, ss: 0x%x", tid,
            (unsigned)ureg->cause, (unsigned)ureg->cr2, (unsigned)ureg->ds, 
            (unsigned)ureg->es, (unsigned)ureg->fs, (unsigned)ureg->gs, 
            (unsigned)ureg->edi, (unsigned)ureg->esi, (unsigned)ureg->ebp, 
            (unsigned)ureg->zero, (unsigned)ureg->ebx, (unsigned)ureg->edx,
            (unsigned)ureg->ecx, (unsigned)ureg->eax, 
            (unsigned)ureg->error_code, (unsigned)ureg->eip,
            (unsigned)ureg->cs, (unsigned)ureg->eflags, (unsigned)ureg->esp, 
            (unsigned)ureg->ss);
    // Should also print to console
    // TBD ******************************
}

/** @brief Generic exception handler
 *
 * Whatever exception happens, this exception handler executes first
 * after registers are pushed on the stack.
 *
 * @param exception_type The type of exception
 *
 * @return void
 */
void exception_handler(int exception_type) {

    lprintf("exception handler called");

    // Get ureg value when exception happened
    ureg_t ureg;
    memset(&ureg, 0, sizeof(ureg_t));
    ureg.cause = exception_type;
    uint32_t *ebp = (uint32_t *)asm_get_ebp();
    /* According Intel documentation, only the following kinds of exception 
       will push an error code on stack: 8, 10 to 14, 17 */
    int has_error_code = (exception_type == 8 || 
            (exception_type >= 10 && exception_type <= 14)
            || exception_type == 17) ? 1 : 0;
    // Fill in ureg struct
    get_ureg(&ureg, ebp, has_error_code);

    int pf_need_debug = 0;
    switch(exception_type) {
        case IDT_DE: // Division error
            lprintf("Division error");
            break;
        case IDT_PF: // Page fault
            lprintf("Page fault");
            // Get faulting address
            ureg.cr2 = get_cr2();
            // Check if it's caused by ZFOD and if so fix it
            if(is_page_ZFOD(ureg.cr2, ureg.error_code, 1)) {
                // Return normally
                lprintf("Kernel fixed ZFOD");
                return;
            }
            lprintf("The page fault was not caused by ZFOD");



            pf_need_debug = 1;



            // Pass it to user exception handler if there's one registered
            break;
        default:
            lprintf("Unknown exception type: %x", (unsigned)exception_type);
    }

    // Check if current thread has an exception handler installed
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        panic("tcb is NULL");
    }

    // DEBUG
    if(pf_need_debug) {
        dump_register(this_thr->tid, &ureg);
        MAGIC_BREAK;
    }
    // DEBUG

    if(this_thr->swexn_struct == NULL) {
        // Thread doesn't have a swexn handler registered
        // Print a reason to kill the thread, should print to the console
        lprintf("Thread tid %d caused a exception of type %d " 
                "and it doesn't have an exception handler installed, "
                "kernel will kill it!", this_thr->tid, exception_type);
        // Dump registers
        dump_register(this_thr->tid, &ureg);

        // Kill thread
        int is_kernel_kill = 1;
        vanish_syscall_handler(is_kernel_kill);
        lprintf("Should not reach here");
        MAGIC_BREAK;
    }

    // Current thread has an exception handler installed
    // Give it a chance to fix the exception
    lprintf("thread tid %d has an exception handler installed",
                this_thr->tid);

    // Deregister swexn handler first
    void *esp3 = this_thr->swexn_struct->esp3;
    swexn_handler_t eip = this_thr->swexn_struct->eip;
    void *arg = this_thr->swexn_struct->arg;
    free(this_thr->swexn_struct);
    this_thr->swexn_struct = NULL;

    // Set up user's expcetion stack
    /* The user's exception stack should look like the following:
        actual ureg struct
        ureg_t *ureg
        swexn->arg
        ret addr   Deadbeef? ESP should also point to here
    */

    // Position of ureg struct on user's exception stack
    uint32_t actual_ureg_pos = (uint32_t)esp3 - 
        sizeof(ureg_t);
    // Copy ureg struct from kernel space to user's exception stack
    memcpy((void *)actual_ureg_pos, (void *)&ureg, sizeof(ureg_t));
    // Push swexn handler's second argument
    *((uint32_t *)(actual_ureg_pos - sizeof(uint32_t))) = actual_ureg_pos;
    // Push swexn handler's first argument
    *((uint32_t *)(actual_ureg_pos - 2 * sizeof(uint32_t))) = 
        (uint32_t)arg;
    // Push return address
    *((uint32_t *)(actual_ureg_pos - 3 * sizeof(uint32_t))) = 0xdeadbeef;

    lprintf("init_elflags: %x", (unsigned)get_init_eflags());
    
    // Set up kernel exception handler's stack before returning to user space
    // to run swexn hanlder
    // asm_ret_swexn_handler(eip, cs, eflags, esp, ss);
    lprintf("About to run user space swexn handler");
    asm_ret_swexn_handler(eip, SEGSEL_USER_CS, 
            get_init_eflags(), actual_ureg_pos - 3 * sizeof(uint32_t), 
            SEGSEL_USER_DS);

    panic("Why did user space swexn handler return to kernel space "
            "exception handler?!");

}

