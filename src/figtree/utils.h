/*
**  ----- BEGIN LICENSE BLOCK -----
**  GDPFS: Global Data Plane File System
**  From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**  Copyright (c) 2016, Regents of the University of California.
**  Copyright (c) 2016, Paul Bramsen, Sam Kumar, and Andrew Chen
**  All rights reserved.
**
**  Permission is hereby granted, without written agreement and without
**  license or royalty fees, to use, copy, modify, and distribute this
**  software and its documentation for any purpose, provided that the above
**  copyright notice and the following two paragraphs appear in all copies
**  of this software.
**
**  IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**  SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**  PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**  EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**  REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**  FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**  IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**  OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**  OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
*/

#ifndef _UTILS_H_
#define _UTILS_H_

#include <ep/ep_assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "../gdpfs_log.h"

typedef uint32_t byte_index_t;
typedef long int figtree_value_t;

struct figtree;

#define FT_ORDER 2
#define FT_SPLITLIMIT (1 + (FT_ORDER << 1))

struct subtree_ptr {
    gdpfs_recno_t recno;
    off_t offset;
    struct ft_node* st;
    bool inmemory;
};

#define BYTE_INDEX_MIN 0
#define BYTE_INDEX_MAX UINT32_MAX

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define ASSERT(x, desc) EP_ASSERT(x)
void my_assert(bool x, char* desc);
void* mem_alloc(size_t s);
void mem_free(void* ptr);
void subtree_clear(struct subtree_ptr* sptr, int height);
void subtree_set(struct subtree_ptr* sptr, struct ft_node* st);
struct ft_node* subtree_get(struct subtree_ptr* sptr, gdpfs_log_t* log);
void subtree_free(struct subtree_ptr* sptr);

void get_dirty(struct ft_node** dirty, int* dirty_len, struct figtree* ft, gdpfs_recno_t chkpt_recno);

#endif
