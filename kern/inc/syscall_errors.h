/** @file syscall_errors.h
 *
 *  @brief This file contains the macro definitions for syscall errors similar
 *  to those in Linux
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */
#ifndef _SYSCALL_ERRORS_H_
#define _SYSCALL_ERRORS_H_

/** @brief No such file or directory */
#define ENOENT          (-2)          
/** @brief Arg list too long */
#define E2BIG           (-7) 
/** @brief Exec format error */         
#define ENOEXEC         (-8)          
/** @brief No child processes */
#define ECHILD          (-10)         
/** @brief Out of memory */
#define ENOMEM          (-12)         
/** @brief Bad address (invalid memory address) */
#define EFAULT          (-14)         
/** @brief Invalid argument */
#define EINVAL          (-22) 
/** @brief File name too long */        
#define ENAMETOOLONG    (-36)         
/** @brief More than one thread */ 
#define EMORETHR        (-256) 
/** @brief No such thread */       
#define ETHREAD         (-257)        
/** @brief Memory address already allocated */
#define EALLOCATED      (-258)        

#endif

