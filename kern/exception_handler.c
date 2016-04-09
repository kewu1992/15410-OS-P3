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

    // Should also print to console, TBD ******************************
    lprintf("Register dump for thread tid %d:", tid);
    lprintf("cause: 0x%x, cr2: 0x%x, ds: 0x%x", 
            (unsigned)ureg->cause, (unsigned)ureg->cr2, (unsigned)ureg->ds);
    lprintf("es: 0x%x, fs: 0x%x, gs: 0x%x", 
            (unsigned)ureg->es, (unsigned)ureg->fs, (unsigned)ureg->gs);
    lprintf("edi: 0x%x, esi: 0x%x, ebp: 0x%x",
            (unsigned)ureg->edi, (unsigned)ureg->esi, (unsigned)ureg->ebp);
    lprintf("zero: 0x%x, ebx: 0x%x, edx: 0x%x", 
            (unsigned)ureg->zero, (unsigned)ureg->ebx, (unsigned)ureg->edx);
    lprintf("ecx: 0x%x, eax: 0x%x, error code: 0x%x",
            (unsigned)ureg->ecx, (unsigned)ureg->eax, 
            (unsigned)ureg->error_code);
    lprintf("eip: 0x%x, cs: 0x%x, eflags: 0x%x",
            (unsigned)ureg->eip, (unsigned)ureg->cs, (unsigned)ureg->eflags);
    lprintf("esp: 0x%x, ss: 0x%x",
            (unsigned)ureg->esp, (unsigned)ureg->ss);

}



/** @brief Print reasons of exception
 *  
 *  @param exception_type The type of exception
 *  @param fault_va The faulting virtual address (used by page fault)
 *  @param error_code The error code (used by page fault)
 *
 *  @return Void
 */
static void exception_interpret(int exception_type, uint32_t fault_va, 
        uint32_t error_code) {

    // Should also print to console, TBD*************************
    switch(exception_type) {
        case IDT_DE: 
            lprintf("Division Error");
            break;
        case IDT_DB: 
            lprintf("Debug Exception");
            break;
        case IDT_NMI: 
            lprintf("Non-Maskable Interrupt");
            break;
        case IDT_BP: 
            lprintf("Breakpoint");
            break;
        case IDT_OF: 
            lprintf("Overflow");
            break;
        case IDT_BR:
            lprintf("BOUND Range exceeded");
            break;
        case IDT_UD: 
            lprintf("UnDefined Opcode");
            break;
        case IDT_NM: 
            lprintf("No Math coprocessor");
            break;
        case IDT_DF: 
            lprintf("Double Fault");
            break;
        case IDT_CSO: 
            lprintf("Coprocessor Segment Overrun");
            break;
        case IDT_TS: 
            lprintf("Invalid Task Segment Selector");
            break;
        case IDT_NP: 
            lprintf("Segment Not Present");
            break;
        case IDT_SS: 
            lprintf("Stack Segment Fault");
            break;
        case IDT_GP: 
            lprintf("General Protection Fault");
            break;
        case IDT_PF: 
            lprintf("Page fault: a %s in %s mode to a %s page at address 0x%x",
                    IS_SET(error_code, PG_RW) ? "write" : "read",
                    IS_SET(error_code, PG_US) ? "user" : "kernel",
                    IS_SET(error_code, PG_P) ? "protected" : "non-present",
                    (unsigned)fault_va);
            break;
        case IDT_MF: 
            lprintf("X87 Math Fault");
            break;
        case IDT_AC: 
            lprintf("Alignment Check");
            break;
        case IDT_MC: 
            lprintf("Machine Check");
            break;
        case IDT_XF: 
            lprintf("SSE Floating Point Exception");
            break;
        default: 
            lprintf("Unknown exception type: %d", exception_type);
            break;
    }

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

    //lprintf("exception handler called");

    // Get ureg value when exception happened
    ureg_t ureg;
    memset(&ureg, 0, sizeof(ureg_t));
    ureg.cause = exception_type;
    uint32_t *ebp = (uint32_t *)asm_get_ebp();

    // According Intel documentation, only the following kinds of exception 
    // will push an error code on stack
    int has_error_code = (exception_type == IDT_DF || 
            (exception_type >= IDT_TS && exception_type <= IDT_PF)
            || exception_type == IDT_AC) ? 1 : 0;
    // Fill in ureg struct from values on stack
    get_ureg(&ureg, ebp, has_error_code);

    // DEBUG
    // int pf_need_debug = 0;
    // Precheck if exception is page fault and caused by ZFOD
    if(exception_type == IDT_PF) {
        // Get faulting address
        ureg.cr2 = get_cr2();
        // Check if it's caused by ZFOD and if so fix it
        if(is_page_ZFOD(ureg.cr2, ureg.error_code, 1)) {
            // Return normally
            lprintf("Kernel fixed ZFOD");
            return;
        }
    }

    // DEBUG
    //      pf_need_debug = 1;


    // Check if current thread has an exception handler installed
    tcb_t *this_thr = tcb_get_entry((void*)asm_get_esp());
    if(this_thr == NULL) {
        lprintf("tcb is NULL");
        panic("tcb is NULL");
    }

    // DEBUG
    /*
       if(pf_need_debug) {
       dump_register(this_thr->tid, &ureg);
       MAGIC_BREAK;
       }
       */
    // DEBUG

    if(this_thr->swexn_struct == NULL) {
        // Thread doesn't have a swexn handler registered
        // Print a reason to kill the thread, should print to the console

        // Interpret a reason to kill the thread
        exception_interpret(exception_type, ureg.cr2, ureg.error_code);

        // Dump registers
        dump_register(this_thr->tid, &ureg);

        // Kill thread
        int is_kernel_kill = 1;
        vanish_syscall_handler(is_kernel_kill);
        panic("Should not reach here");
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
       ret addr(0xdeadbeef) <-%esp should point to here before iret


       0xdeadbeed is an invalid address that will cause page fault
       if user returns directly from swexn handler.
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

