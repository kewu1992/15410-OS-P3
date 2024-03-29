/* 
 * Copyright (c) 1996-1995 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 */
/*
 * Alternate version of malloc
 * that expects the caller to keep track of the size of the chunk.
 * Allocated chunks must be freed with sfree() rather than free().
 */

#include <stddef.h>
#include "malloc_internal.h"

void *_smalloc(size_t size)
{
	void *chunk;

	if (!(chunk = lmm_alloc(&core_malloc_lmm[smp_get_cpu()], size, 0)))
        return NULL;

	return chunk;
}

