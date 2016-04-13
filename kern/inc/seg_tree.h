/** @file seg_tree.h
 *  @brief This file contains function interfaces of seg_tree.c
 *  @author Ke Wu (kewu)
 *  @bug No known bug
 */

#ifndef _SEG_TREE_H_
#define _SEG_TREE_H_

/** @brief This is a special value to indicate there is no free frame */
#define NAN -1

int init_seg_tree();

uint32_t get_next();

void put_back(uint32_t index);

#endif
