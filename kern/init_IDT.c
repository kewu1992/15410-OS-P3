/** @file init_IDT.c
 *  @brief Install interrupt handlers and initialize device drivers
 *
 *  This file contains function to install interrupt handler, i.e. fill the 
 *  corresponding entires in IDT. It also contians some helper functions and
 *  some macros to help filling IDT entries.
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <asm.h>
#include <seg.h>
#include <string.h>
//#include <keyhelp.h>
//#include <timer_defines.h>
#include "handler_wrapper.h"
#include <console_driver.h>
//#include "keyboard_driver.h"
//#include "timer_driver.h"
#include <syscall_int.h>

#include <simics.h>

/** @brief Size of an IDT entry */
#define IDT_ENTRY_SIZE 8

/** @brief Position of the first part of offset field */
#define GATE_OFFSET_BEGIN 0
/** @brief Position of the second part of offset field */
#define GATE_OFFSET_MID 6
/** @brief Size of the first part of offset field */
#define GATE_OFFSET_SIZE1 2
/** @brief Size of the second part of offset field */
#define GATE_OFFSET_SIZE2 2

/** @brief Position of segment selector field */
#define GATE_SEGSEL_BEGIN 2
/** @brief Size of segment selector field */
#define GATE_SEGSEL_SIZE 2

/** @brief Position of option (DPL, P, D) field */
#define GATE_OPTION_BEGIN 4
/** @brief Size of option (DPL, P, D) field */
#define GATE_OPTION_SIZE 2
/** @brief Indicate trap gate */
#define GATE_TRAP_OPTION 0x700
/** @brief Indicate interrupt gate */
#define GATE_INTERRUPT_OPTION 0x600
/** @brief Set P to 1 */
#define GATE_OPTION_P 0x8000
/** @brief Set DPL to 0 so that privilege level 0 is required to call handler */
#define GATE_OPTION_DPL0 0
/** @brief Set DPL to 3 so that handler can be called in user mode */
#define GATE_OPTION_DPL3 0x6000
/** @brief Set D to 1, indicate size of the gate is 32 bits */
#define GATE_OPTION_D 0x800

/** @brief Fill interrupt handler address to an IDT entry
 *
 *  The address of handler should be divided to two parts and filled to offset 
 *  filed of IDT entry seperatrly. 
 *
 *  @param base The start address of the IDT entry
 *  @param handler The address of interrupt handler function
 *  @return Void.
 */
void fill_handler(void* base, void* handler) {
    char* cbase = (char*)base;
    uint32_t handler_addr = (uint32_t)handler;

    memcpy(cbase+GATE_OFFSET_BEGIN, &handler_addr, GATE_OFFSET_SIZE1);
    memcpy(cbase+GATE_OFFSET_MID, ((char*)&handler_addr)+GATE_OFFSET_SIZE1, 
                                                            GATE_OFFSET_SIZE2);
}

/** @brief Fill segment selector to an IDT entry
 *
 *  @param base The start address of the IDT entry
 *  @param segsel Indicate which segment selector to be filled in to the IDT,
 *                entry, should be either SEGSEL_KERNEL_CS or SEGSEL_USER_CS
 *  @return Void.
 */
void fill_segsel(void* base, uint16_t segsel) {
    memcpy((char*)base+GATE_SEGSEL_BEGIN, &segsel, GATE_SEGSEL_SIZE);
}

/** @brief Fill options to an IDT entry
 *
 *  Options include DPL, P, D and the type of gate. All these values are defined
 *  at the begining of this file. They will be filled to the IDT entry according
 *  to the format of IDT entry.
 *
 *  @param base The start address of the IDT entry
 *  @param DPL Indicate DPL flag that will be filled in IDT entry, should either
 *             be 0 (kernel mode) or 3 (user mode)
 *  @param gate_type Indicate which gate type will be filled in the IDT entry,
 *                   should either be 0 (trap gate) or 1 (interrupt gate)
 *
 *  @return Void.
 */
void fill_option(void* base, int DPL, int gate_type) {
    uint16_t gate_type_option, dpl_option;

    if (gate_type == 0)
        gate_type_option = GATE_TRAP_OPTION;
    else
        gate_type_option = GATE_INTERRUPT_OPTION;

    if (DPL == 0)
        dpl_option = GATE_OPTION_DPL0;
    else
        dpl_option = GATE_OPTION_DPL3;


    uint16_t option = 
            gate_type_option | GATE_OPTION_D | GATE_OPTION_P | dpl_option;
    memcpy((char*)base+GATE_OPTION_BEGIN, &option, GATE_OPTION_SIZE);
}

/** @brief Install an IDT entry
 *
 *
 *  @param index The index in the IDT entry
 *  @param handler The address of handler function 
 *  @param segsel Indicate which segment selector to be filled in to the IDT,
 *                entry, should be either SEGSEL_KERNEL_CS or SEGSEL_USER_CS
 *  @param DPL Indicate DPL flag that will be filled in IDT entry, should either
 *             be 0 (kernel mode) or 3 (user mode)
 *  @param gate_type Indicate which gate type will be filled in the IDT entry,
 *                   should either be 0 (trap gate) or 1 (interrupt gate)
 *
 *  @return Void.
 */
void install_IDT_entry(int index, void *hanlder, uint16_t segsel, int DPL, int gate_type) {
    void* idt_entry = 
                (void*)((char*)idt_base() + index * IDT_ENTRY_SIZE);
    fill_handler(idt_entry, hanlder);
    fill_segsel(idt_entry, segsel);
    fill_option(idt_entry, DPL, gate_type);
}

/** @brief The driver-library initialization function
 *
 *  Install interrupt handlers (keyboard and timer) in IDT (interrupt descriptor
 *  table), i.e. fill the corresponding entires in IDT. Note that the two 
 *  interrupt handlers are using trap gates.  
 *
 *  @param tickback Pointer to clock-tick callback function
 *   
 *  @return A negative error code on error, or 0 on success
 **/
int init_IDT(void (*tickback)(unsigned int)) {
    /*
    // install keyboard interrupt handler
    void* keyboard_idt = 
                    (void*)((char*)idt_base() + KEY_IDT_ENTRY * IDT_ENTRY_SIZE);
    fill_handler(keyboard_idt, keyboard_wrapper);
    fill_segsel(keyboard_idt);
    fill_option(keyboard_idt);
    
    // instll timer interrupt handler
    void* timer_idt = 
                (void*)((char*)idt_base() + TIMER_IDT_ENTRY * IDT_ENTRY_SIZE);
    fill_handler(timer_idt, timer_wrapper);
    fill_segsel(timer_idt);
    fill_option(timer_idt);
    */


    // install gettid() syscall handler
    install_IDT_entry(GETTID_INT, gettid_wrapper, SEGSEL_KERNEL_CS, 3, 0);

    // initialize device drivers
    init_console_driver();
    //init_keyboard_driver();
    //init_timer_driver(tickback);

    return 0;
}