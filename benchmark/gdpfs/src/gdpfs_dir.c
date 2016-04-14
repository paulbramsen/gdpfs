#include "gdpfs_dir.h"
#include "gdpfs_stat.h"

#include <ep/ep_app.h>
#include <libgen.h>
#include <string.h>

static gdpfs_file_gname_t root_log_gname;

struct gdpfs_dir_entry_phys
{
    /* The GNAME is the name of the log (a gdpfs_file_gname_t is really just a gdp_name_t). */
    gdpfs_file_gname_t gname;
    bool in_use;
    char name[NAME_MAX2 + 1];
};
typedef struct gdpfs_dir_entry_phys gdpfs_dir_entry_phys_t;

EP_STAT init_gdpfs_dir(char *root_log, gdpfs_file_mode_t mode)
{
    EP_STAT estat;
    uint64_t fh;

    gdp_parse_name(root_log, root_log_gname);

    // initialize the root log
    fh = gdpfs_file_open_init(&estat, root_log_gname, mode, GDPFS_FILE_TYPE_DIR, false);
    if (EP_STAT_ISOK(estat))
    {
        gdpfs_file_close(fh);
    }
    return estat;
}

void stop_gdpfs_dir()
{
    /* Nothing to do here. */
}

uint64_t gdpfs_dir_open_file_at_path(EP_STAT *ret_stat, const char *path,
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
            fh = gdpfs_file_open_type(&estat, phys_ent.gname, mode, curr_type);
            if (!EP_STAT_ISOK(estat))
            {
                if (ret_stat)
                    *ret_stat = estat;
                return -1;
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

EP_STAT gdpfs_dir_add_file_at_path(gdpfs_file_gname_t gname,
        const char *filepath, gdpfs_file_mode_t mode)
{
    EP_STAT estat;
    uint64_t fh;
    char *path, *file;
    char *path_mem, *file_mem;

    if (filepath[0] != '/')
    {
        return GDPFS_STAT_BADPATH;
    }

    path_mem = ep_mem_zalloc(strlen(filepath) + 1);
    file_mem = ep_mem_zalloc(strlen(filepath) + 1);
    if (!path_mem || !file_mem)
    {
        estat = GDPFS_STAT_OOMEM;
        goto fail0;
    }
    strncpy(path_mem, filepath, strlen(filepath) + 1);
    strncpy(file_mem, filepath, strlen(filepath) + 1);
    path = dirname(path_mem);
    file = basename(file_mem);

    printf("Adding file %s\n", filepath);
    fh = gdpfs_dir_open_file_at_path(&estat, path, mode, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open dir at path:\"%s\"", path);
        goto fail0;
    }
    estat = gdpfs_dir_add(fh, file, gname);
    gdpfs_file_close(fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to add file:\"%s\": %d", filepath, EP_STAT_DETAIL(estat));
        goto fail0;
    }

    ep_mem_free(path_mem);
    ep_mem_free(file_mem);
    return GDPFS_STAT_OK;

fail0:
    ep_mem_free(path_mem);
    ep_mem_free(file_mem);
    return estat;
}

// TODO: need to combine this with gdpfs_dir_create_file_at_path!
EP_STAT gdpfs_dir_remove_file_at_path(const char *filepath, gdpfs_file_mode_t mode)
{
    EP_STAT estat;
    uint64_t fh;
    char *path, *file;
    char *path_mem, *file_mem;

    if (filepath[0] != '/')
    {
        return GDPFS_STAT_BADPATH;
    }

    path_mem = ep_mem_zalloc(strlen(filepath) + 1);
    file_mem = ep_mem_zalloc(strlen(filepath) + 1);
    if (!path_mem || !file_mem)
    {
        estat = GDPFS_STAT_OOMEM;
        goto end;
    }
    strncpy(path_mem, filepath, strlen(filepath) + 1);
    strncpy(file_mem, filepath, strlen(filepath) + 1);
    path = dirname(path_mem);
    file = basename(file_mem);

    printf("removing %s (trash this print)\n", filepath);
    fh = gdpfs_dir_open_file_at_path(&estat, path, mode, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open dir at path:\"%s\"", path);
        goto end;
    }
    estat = gdpfs_dir_remove(fh, file);
    gdpfs_file_close(fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to add file:\"%s\"", filepath);
        goto end;
    }

end:
    ep_mem_free(path_mem);
    ep_mem_free(file_mem);
    return estat;
}

EP_STAT gdpfs_dir_add(uint64_t fh, const char *name, gdpfs_file_gname_t gname)
{
    size_t size;
    off_t offset;
    gdpfs_dir_entry_phys_t phys_ent;

    off_t insert_offset;
    bool insert_offset_set = false;
    
    offset = 0;
    while (true) {
        size = gdpfs_file_read(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
        if (size == 0)
        {
            break;
        }
        if (size != sizeof(gdpfs_dir_entry_phys_t))
            return GDPFS_STAT_CORRUPT;
        else if (phys_ent.in_use)
        {
            if (strcmp(phys_ent.name, name) == 0)
                return GDPFS_STAT_FILE_EXISTS;
        }
        else if (!insert_offset_set)
        {
            insert_offset = offset;
            insert_offset_set = true;
        }
        offset += size;
    }
    
    if (insert_offset_set) {
        offset = insert_offset;
    }
    
    memcpy(phys_ent.gname, gname, sizeof(gdpfs_file_gname_t));
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

EP_STAT gdpfs_dir_remove(uint64_t fh, const char *name)
{
    EP_STAT estat;
    size_t size;
    off_t offset;
    gdpfs_dir_entry_phys_t phys_ent;

    offset = 0;
    estat = GDPFS_STAT_NOTFOUND;
    do {
        size = gdpfs_file_read(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
        if (size != 0 && size != sizeof(gdpfs_dir_entry_phys_t))
        {
            estat = GDPFS_STAT_CORRUPT;
            break;
        }
        if (strcmp(name, phys_ent.name) == 0)
        {
            estat = GDPFS_STAT_OK;
            break;
        }
        offset += size;
    } while(size != 0);
    if (!EP_STAT_ISOK(estat))
    {
        return estat;
    }
    phys_ent.in_use = false;
    size = gdpfs_file_write(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
    if (size == 0)
    {
        estat = GDPFS_STAT_RW_FAILED;
    }
    else if (size != sizeof(gdpfs_dir_entry_phys_t))
    {
        estat = GDPFS_STAT_CORRUPT;
    }
    return estat;
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
