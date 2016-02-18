#ifndef _GDPFS_DIR_H_
#define _GDPFS_DIR_H_

#include <ep/ep.h>
#include "gdpfs_file.h"

#define NAME_MAX2 255

struct gdpfs_dir_entry
{
    char name[NAME_MAX2];
    off_t offset;
};
typedef struct gdpfs_dir_entry gdpfs_dir_entry_t;

EP_STAT init_gdpfs_dir(char *root_log, gdpfs_file_mode_t mode);
void stop_gdpfs_dir();
uint64_t gdpfs_dir_open(EP_STAT *ret_stat, char *name, gdpfs_file_mode_t mode);
EP_STAT gdpfs_dir_close(uint64_t fh);
EP_STAT gdpfs_dir_add(uint64_t fh, const char *name, const char *log);
EP_STAT gdpfs_dir_read(uint64_t fh, gdpfs_dir_entry_t *ent, off_t offset);

/*

int gdpfs_file_ftruncate(uint64_t fd, size_t file_size);
size_t gdpfs_file_size(uint64_t fd);
*/

#endif // _GDPFS_DIR_H_
