#ifndef _UTILS_H_
#define _UTILS_H_

#include <ep/ep_assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t byte_index_t;
typedef long int figtree_value_t;

#define FT_ORDER 2
#define FT_SPLITLIMIT (1 + (FT_ORDER << 1))

struct subtree_ptr {
    struct ft_node* st;
};

#define BYTE_INDEX_MIN 0
#define BYTE_INDEX_MAX UINT32_MAX

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define ASSERT(x, desc) EP_ASSERT(x)
void my_assert(bool x, char* desc);
void* mem_alloc(size_t s);
void mem_free(void* ptr);
void subtree_set(struct subtree_ptr* sptr, struct ft_node* st);
struct ft_node* subtree_get(struct subtree_ptr* sptr);
void subtree_free(struct subtree_ptr* sptr);

#endif
