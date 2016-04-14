#ifndef _GDPFS_DIR_H_
#define _GDPFS_DIR_H_

#include <ep/ep.h>
#include "gdpfs_file.h"

#define NAME_MAX_GDPFS 255

struct gdpfs_dir_entry
{
    char name[NAME_MAX_GDPFS];
    off_t offset;
};
typedef struct gdpfs_dir_entry gdpfs_dir_entry_t;

/*
 * global dir subsystem intiailization
 */
EP_STAT
init_gdpfs_dir(char *root_log);

void
stop_gdpfs_dir();

/*
 * dir open
 */
uint64_t
gdpfs_dir_open_file_at_path(EP_STAT *ret_stat, const char *path,
		gdpfs_file_type_t type);

/*
 * dir read/modify
 */
EP_STAT
gdpfs_dir_open_parent_dir(uint64_t* fh, const char* filepath, char** file,
		char** tofree);

EP_STAT
gdpfs_dir_create_file_at_path(uint64_t* fh, const char* filepath,
		gdpfs_file_type_t type, gdpfs_file_gname_t gname_if_exists,
		gdpfs_file_perm_t perm);

EP_STAT
gdpfs_dir_replace_file_at_path(uint64_t fh, const char *filepath2);

EP_STAT
gdpfs_dir_remove_file_at_path(const char *path);

EP_STAT
gdpfs_dir_remove(uint64_t fh, const char *name);

EP_STAT
gdpfs_dir_read(uint64_t fh, gdpfs_dir_entry_t *ent, off_t offset);

#endif // _GDPFS_DIR_H_
