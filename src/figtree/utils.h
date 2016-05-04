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
