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
uint64_t gdpfs_dir_open_file_at_path(EP_STAT *ret_stat, const char *path,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type);
EP_STAT gdpfs_dir_open_parent_dir(uint64_t* fh, const char* filepath, gdpfs_file_mode_t mode, char** file, char** tofree);
//EP_STAT gdpfs_dir_add_file_at_path(gdpfs_file_gname_t gname,
//        const char *path, gdpfs_file_mode_t mode);
EP_STAT gdpfs_find_insert_offset(off_t* offset_ptr, uint64_t fh, const char* name, gdpfs_file_gname_t gname_if_exists);
EP_STAT gdpfs_dir_remove_file_at_path(const char *path, gdpfs_file_mode_t mode);
EP_STAT gdpfs_dir_add_at_offset(uint64_t fh, off_t offset, const char *name, gdpfs_file_gname_t log_name);
EP_STAT gdpfs_dir_remove(uint64_t fh, const char *name);
EP_STAT gdpfs_dir_read(uint64_t fh, gdpfs_dir_entry_t *ent, off_t offset);

#endif // _GDPFS_DIR_H_
