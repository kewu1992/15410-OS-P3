/** @file hashtable.h
 *  @brief Function prototypes of a generic hash table and declaraion of 
 *         hash table node and hash table data structure. 
 *
 *  @author Ke Wu (kewu)
 *
 *  @bug No known bugs.
 */

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <mutex.h>

/** @brief The node strcuture of linked list of hash table
 * Separate chaining is used to resolve collision. 
 */
typedef struct hashnode_s{
    /** @brief Pointer to next node */
    struct hashnode_s *next;
    /** @brief Key */
    void *key;
    /** @brief Value */
    void *value;
} hashnode_t;

/** @brief The hash table data structure */
typedef struct {
    /** @brief Acutal data of the hashtable */
    hashnode_t *array;
    /** @brief Size of the hashtable */
    int size;
    /** @brief The hash function used by this hashtable */
    int (*func)(void *);
    /** A mutex to ensure thread-safety of this hashtable */
    mutex_t lock;
} hashtable_t;

int hashtable_init(hashtable_t *table);

void hashtable_put(hashtable_t *table, void* key, void* vaule);

void* hashtable_get(hashtable_t *table, void* key, int *is_find);

void* hashtable_remove(hashtable_t *table, void* key, int *is_find);

void hashtable_destroy(hashtable_t *table);

#endif
