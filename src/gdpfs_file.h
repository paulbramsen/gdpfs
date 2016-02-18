#ifndef _GDPFS_FILE_H_
#define _GDPFS_FILE_H_

#include <ep/ep.h>
#include <stdbool.h>

#include "gdpfs_log.h"

enum gdpfs_file_type
{
    GDPFS_FILE_TYPE_REGULAR = 0,
    GDPFS_FILE_TYPE_DIR,
    GDPFS_FILE_TYPE_NEW,
};
typedef enum gdpfs_file_type gdpfs_file_type_t;

enum gdpfs_file_mode
{
    GDPFS_FILE_MODE_RO = 0,
    GDPFS_FILE_MODE_RW,
    GDPFS_FILE_MODE_WO,
};
typedef enum gdpfs_file_mode gdpfs_file_mode_t;
typedef gdpfs_log_gname_t gdpfs_file_gname_t;

EP_STAT init_gdpfs_file();
void stop_gdpfs_file();
uint64_t gdpfs_file_open(EP_STAT *ret_stat, char *name, gdpfs_file_mode_t mode);
uint64_t gdpfs_file_open_type(EP_STAT *ret_stat, char *name,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type);
// strict_init causes failure if file is not TYPE_NEW
uint64_t gdpfs_file_open_init(EP_STAT *ret_stat, char *name,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type, bool strict_init);
EP_STAT gdpfs_file_close(uint64_t fh);
size_t gdpfs_file_read(uint64_t fh, void *buf, size_t size, off_t offset);
size_t gdpfs_file_write(uint64_t fh, const void *buf, size_t size, off_t offset);
int gdpfs_file_ftruncate(uint64_t fh, size_t file_size);
size_t gdpfs_file_size(uint64_t fh);
void gdpfs_file_gname(uint64_t fh, gdpfs_file_gname_t gname);

#endif // _GDPFS_FILE_H_
