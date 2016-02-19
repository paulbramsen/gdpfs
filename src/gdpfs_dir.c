#include "gdpfs_dir.h"
#include "gdpfs_stat.h"

#include <ep/ep_app.h>
#include <string.h>

static char *root_log_gname;

struct gdpfs_dir_entry_phys
{
    gdpfs_file_gname_t gname;
    bool in_use;
    char name[NAME_MAX2 + 1];
};
typedef struct gdpfs_dir_entry_phys gdpfs_dir_entry_phys_t;

EP_STAT init_gdpfs_dir(char *root_log, gdpfs_file_mode_t mode)
{
    EP_STAT estat;
    uint64_t fh;

    root_log_gname = ep_mem_zalloc(strlen(root_log) + 1);
    if (!root_log_gname)
    {
        return GDPFS_STAT_OOMEM;
    }
    strcpy(root_log_gname, root_log);
    // innitialize the root log
    fh = gdpfs_file_open_init(&estat, root_log, mode, GDPFS_FILE_TYPE_DIR, false);
    if (EP_STAT_ISOK(estat))
    {
        gdpfs_file_close(fh);
    }
    return estat;
}

void stop_gdpfs_dir()
{
    ep_mem_free(root_log_gname);
}

uint64_t gdpfs_dir_open_path(EP_STAT *ret_stat, const char *path,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type)
{
    EP_STAT estat;
    gdpfs_file_type_t curr_type;
    gdpfs_dir_entry_phys_t phys_ent;
    uint64_t fh;
    char name[NAME_MAX2 + 1];
    int i;
    off_t offset;
    size_t size;

    if (path[0] != '/' && path[0] != '\0')
    {
        ep_app_error("Can only resolve paths relative to /. Path:%s", path);
        if (ret_stat != NULL)
            *ret_stat = GDPFS_STAT_BADPATH;
        return -1;
    }

    fh = gdpfs_file_open_type(&estat, root_log_gname, mode, GDPFS_FILE_TYPE_DIR);
    curr_type = GDPFS_FILE_TYPE_DIR;
    while (path[0] != '\0')
    {
        if (path[0] == '/')
        {
            path++;
        }
        else
        {
            for (i = 0; path[i] != '\0' && path[i] != '/'; i++)
            {
                name[i] = path[i];
            }
            name[i] = '\0';
            path += i;
            offset = 0;
            do {
                size = gdpfs_file_read(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
                if (size == 0)
                {
                    // failed to find file
                    if (ret_stat)
                        *ret_stat = GDPFS_STAT_BADPATH;
                    goto fail0;
                }
                else if (size != sizeof(gdpfs_dir_entry_phys_t))
                {
                    if (ret_stat)
                        *ret_stat = GDPFS_STAT_CORRUPT;
                    goto fail0;
                }
                offset += size;
            } while(!phys_ent.in_use || strcmp(phys_ent.name, name) != 0);
            if (path[0] == '\0')
                curr_type = type;
            gdpfs_file_close(fh);
            // TODO: needs to open gname, not name
            fh = gdpfs_file_open_type(&estat, phys_ent.name, mode, curr_type);
            if (!EP_STAT_ISOK(estat))
            {
                if (ret_stat)
                    *ret_stat = estat;
                goto fail0;
            }
        }
    }
    if (ret_stat)
        *ret_stat = EP_STAT_OK;
    return fh;
fail0:
    gdpfs_file_close(fh);
    return -1;
}

EP_STAT gdpfs_dir_close(uint64_t fd)
{
    return gdpfs_file_close(fd);
}

EP_STAT gdpfs_dir_add(uint64_t fh, const char *name, const char *log)
{
    size_t size;
    off_t offset;
    gdpfs_dir_entry_phys_t phys_ent;

    offset = 0;
    // TODO: make sure name doesn't exist
    do {
        size = gdpfs_file_read(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
        if (size == 0)
        {
            break;
        }
        else if (size != sizeof(gdpfs_dir_entry_phys_t))
        {
            return GDPFS_STAT_CORRUPT;
        }
        offset += size;
    } while(phys_ent.in_use);
    //phys_ent.gname = "asdf"; // TODO: convert log to gname
    phys_ent.in_use = true;
    strncpy(phys_ent.name, name, NAME_MAX2);
    phys_ent.name[NAME_MAX2] = '\0';
    size = gdpfs_file_write(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
    if (size == 0)
    {
        return GDPFS_STAT_RW_FAILED;
    }
    else if (size != sizeof(gdpfs_dir_entry_phys_t))
    {
        return GDPFS_STAT_CORRUPT;
    }
    return GDPFS_STAT_OK;
}

EP_STAT gdpfs_dir_read(uint64_t fh, gdpfs_dir_entry_t *ent, off_t offset)
{
    size_t size;

    gdpfs_dir_entry_phys_t phys_ent;
    do {
        size = gdpfs_file_read(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
        if (size == 0)
        {
            return GDPFS_STAT_EOF;
        }
        else if (size != sizeof(gdpfs_dir_entry_phys_t))
        {
            return GDPFS_STAT_CORRUPT;
        }
        offset += size;
    } while(!phys_ent.in_use);
    strncpy(ent->name, phys_ent.name, NAME_MAX2 + 1);
    ent->offset = offset;
    return GDPFS_STAT_OK;
}
