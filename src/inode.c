#include "inode.h"

#include <gdp/gdp.h>
#include <stddef.h>

typedef enum
{
    GDPFS_FILE = 0,
    GDPFS_DIR,
    // GDPFS_SYMLINK,
} file_type_t;

typedef struct
{
    gdpfs_inumber inumber;
    size_t file_size;
    uint32_t ref_count;
    file_type_t type;
    gdp_recno_t recs[12];
} gdpfs_inode_log_t;

typedef struct
{
    uint32_t open_count;
} gdpfs_inode_t;

gdpfs_inode_t *
inode_open(gdp_recno_t rec)
{
    return NULL;
}