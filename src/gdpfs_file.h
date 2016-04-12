#ifndef _GDPFS_FILE_H_
#define _GDPFS_FILE_H_

#include <ep/ep.h>
#include <stdbool.h>

#include "gdpfs_log.h"

typedef gdpfs_log_gname_t gdpfs_file_gname_t;
typedef uint16_t gdpfs_file_perm_t;

typedef enum gdpfs_file_type
{
    GDPFS_FILE_TYPE_REGULAR = 0,
    GDPFS_FILE_TYPE_DIR,
    GDPFS_FILE_TYPE_NEW,
    GDPFS_FILE_TYPE_UNKNOWN = -1,
} gdpfs_file_type_t;

typedef enum gdpfs_file_mode
{
    GDPFS_FILE_MODE_RO = 0,
    GDPFS_FILE_MODE_RW,
    GDPFS_FILE_MODE_WO,
} gdpfs_file_mode_t;

typedef struct gdpfs_file_info
{
    size_t file_size;
    gdpfs_file_type_t file_type;
    gdpfs_file_perm_t file_perm;
} gdpfs_file_info_t;


/*
 * global file subsystem intiailization
 */
EP_STAT
init_gdpfs_file(gdpfs_file_mode_t fs_mode);

void
stop_gdpfs_file();

/*
 * file creation
 */
EP_STAT
gdpfs_file_create(uint64_t *fhp, gdpfs_file_gname_t log_iname,
        gdpfs_file_type_t type, gdpfs_file_perm_t);

/*
 * file open/close
 */
uint64_t
gdpfs_file_open(EP_STAT *ret_stat, gdpfs_file_gname_t name);

uint64_t
gdpfs_file_open_type(EP_STAT *ret_stat, gdpfs_file_gname_t name,
        gdpfs_file_type_t type);

// opens a file and initializes if necessary.
// strict_init: fail if file is not TYPE_NEW
// perm: if file is initialized, these permissions will be used
uint64_t
gdpfs_file_open_init(EP_STAT *ret_stat, gdpfs_file_gname_t name,
        gdpfs_file_type_t type, gdpfs_file_perm_t perm, bool strict_init);

EP_STAT
gdpfs_file_close(uint64_t fh);

void
gdpfs_file_gname(uint64_t fh, gdpfs_file_gname_t gname);

/*
 * file read/modify
 */
size_t
gdpfs_file_read(uint64_t fh, void *buf, size_t size, off_t offset);

size_t
gdpfs_file_write(uint64_t fh, const void *buf, size_t size, off_t offset);

int
gdpfs_file_ftruncate(uint64_t fh, size_t file_size);

EP_STAT
gdpfs_file_set_perm(uint64_t fh, gdpfs_file_perm_t perm);

EP_STAT
gdpfs_file_set_info(uint64_t fh, gdpfs_file_info_t info);

EP_STAT
gdpfs_file_info(uint64_t fh, gdpfs_file_info_t *ret_stat);

#endif // _GDPFS_FILE_H_
