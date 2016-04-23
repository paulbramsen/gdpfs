#include "gdpfs_dir.h"
#include "gdpfs_stat.h"

#include <ep/ep_app.h>
#include <libgen.h>
#include <string.h>

#define GDPFS_ROOT_DEFAULT_PERM (0551)

static gdpfs_file_gname_t root_log_gname;

struct gdpfs_dir_entry_phys
{
    /* The GNAME is the name of the log (a gdpfs_file_gname_t is really just a gdp_name_t). */
    gdpfs_file_gname_t gname;
    bool in_use;
    char name[NAME_MAX_GDPFS + 1];
};
typedef struct gdpfs_dir_entry_phys gdpfs_dir_entry_phys_t;

static EP_STAT
_open_parent_dir(uint64_t* fh, const char* filepath, char** file,
        char** tofree);
static EP_STAT
_find_insert_offset(off_t* offset_ptr, uint64_t fh, const char* name,
        gdpfs_file_gname_t gname_if_exists);
static EP_STAT
_add_at_offset(uint64_t fh, off_t offset, const char *name, gdpfs_file_gname_t gname);

EP_STAT
init_gdpfs_dir(char *root_log)
{
    EP_STAT estat;
    uint64_t fh;

    gdp_parse_name(root_log, root_log_gname);

    // initialize the root log
    fh = gdpfs_file_open_init(&estat, root_log_gname, GDPFS_FILE_TYPE_DIR,
            GDPFS_ROOT_DEFAULT_PERM, false);
    if (EP_STAT_ISOK(estat))
    {
        gdpfs_file_close(fh);
    }
    return estat;
}

void
stop_gdpfs_dir()
{
    /* Nothing to do here. */
}

uint64_t
gdpfs_dir_open_file_at_path(EP_STAT *ret_stat, const char *path,
        gdpfs_file_type_t type)
{
    EP_STAT estat;
    gdpfs_file_type_t curr_type;
    gdpfs_dir_entry_phys_t phys_ent;
    uint64_t fh;
    char name[NAME_MAX_GDPFS + 1];
    int i;
    off_t offset;
    size_t size;
    // TODO: check execute permission

    if (path[0] != '/' && path[0] != '\0')
    {
        ep_app_error("Can only resolve paths relative to /. Path:%s", path);
        if (ret_stat != NULL)
            *ret_stat = GDPFS_STAT_BADPATH;
        return -1;
    }

    fh = gdpfs_file_open_type(&estat, root_log_gname, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open root at path \"%s\".", path);
        if (ret_stat != NULL)
            *ret_stat = estat;
        return -1;
    }
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
            fh = gdpfs_file_open_type(&estat, phys_ent.gname, curr_type);
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

static EP_STAT
_open_parent_dir(uint64_t* fh, const char* filepath, char** file,
        char** tofree)
{
    EP_STAT estat;
    char *path, *path_mem;

    if (filepath[0] != '/')
    {
        return GDPFS_STAT_BADPATH;
    }

    path_mem = ep_mem_zalloc(strlen(filepath) + 1);
    if (!path_mem)
    {
        return GDPFS_STAT_OOMEM;
    }
    strncpy(path_mem, filepath, strlen(filepath) + 1);
    path = dirname(path_mem);

    *fh = gdpfs_dir_open_file_at_path(&estat, path, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat) || file == NULL)
    {
        ep_mem_free(path_mem);
        *tofree = NULL;
    }
    else
    {
        strncpy(path_mem, filepath, strlen(filepath) + 1);
        *file = basename(path_mem);
        *tofree = path_mem;
    }

    return estat;
}

EP_STAT
gdpfs_dir_create_file_at_path(uint64_t* fh, const char* filepath,
        gdpfs_file_type_t type, gdpfs_file_gname_t gname_if_exists,
        gdpfs_file_perm_t perm)
{
    EP_STAT estat;
    off_t insert_offset;
    char *file, *file_mem;
    gdpfs_file_gname_t newfile_gname;
    uint64_t dirfh;

    estat = _open_parent_dir(&dirfh, filepath, &file, &file_mem);
    if (!EP_STAT_ISOK(estat))
    {
        return estat;
    }

    estat = _find_insert_offset(&insert_offset, dirfh, file, gname_if_exists);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to find insert offset for file:\"%s\": %d", filepath, EP_STAT_DETAIL(estat));
        goto fail;
    }

    estat = gdpfs_file_create(fh, newfile_gname, type, perm);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to create file:\"%s\": %d", filepath, EP_STAT_DETAIL(estat));
        goto fail;
    }

    estat = _add_at_offset(dirfh, insert_offset, file, newfile_gname);
    gdpfs_file_close(dirfh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to add file:\"%s\": %d", filepath, EP_STAT_DETAIL(estat));
        // TODO close fh
        goto fail;
    }

    ep_mem_free(file_mem);
    return GDPFS_STAT_OK;

fail:
    ep_mem_free(file_mem);
    return estat;
}

EP_STAT
gdpfs_dir_replace_file_at_path(uint64_t fh, const char *filepath2)
{
    EP_STAT estat;
    uint64_t fh2;
    gdpfs_file_info_t finfo;
    gdpfs_file_type_t f1type;
    gdpfs_file_type_t f2type;
    gdpfs_file_gname_t gname;
    gdpfs_dir_entry_t dirent;
    char *file, *file_mem;
    off_t insert_offset;
    gdpfs_file_gname_t existing_logname;

    gdpfs_file_gname(fh, gname);
    estat = gdpfs_file_info(fh, &finfo);
    if (!EP_STAT_ISOK(estat))
        return estat;

    f1type = finfo.file_type;

    estat = _open_parent_dir(&fh, filepath2, &file, &file_mem);
    if (!EP_STAT_ISOK(estat))
        return estat;

    estat = _find_insert_offset(&insert_offset, fh, file, existing_logname);
    if (EP_STAT_DETAIL(estat) == EP_STAT_DETAIL(GDPFS_STAT_FILE_EXISTS))
    {
        fh2 = gdpfs_file_open(&estat, existing_logname);
        if (!EP_STAT_ISOK(estat))
        {
            ep_app_error("Could not open parent directory of \"%s\": %d", filepath2, EP_STAT_DETAIL(estat));
            goto failandfree;
        }
        estat = gdpfs_file_info(fh2, &finfo);
        if (!EP_STAT_ISOK(estat))
        {
            ep_app_error("Could not get info of parent directory of \"%s\": %d", filepath2, EP_STAT_DETAIL(estat));
            goto failcloseandfree;
        }
        f2type = finfo.file_type;
        if (f1type != f2type)
        {
            if (f2type == GDPFS_FILE_TYPE_REGULAR)
                estat = GDPFS_STAT_FILE_EXISTS;
            else
                estat = GDPFS_STAT_DIR_EXISTS;
            goto failcloseandfree;
        }
        if (f2type == GDPFS_FILE_TYPE_DIR)
        {
            // Check to make sure that the directory is empty
            estat = gdpfs_dir_read(fh2, &dirent, 0);
            if (!EP_STAT_ISOK(estat))
            {
                ep_app_error("Could not read parent directory of \"%s\": %d", filepath2, EP_STAT_DETAIL(estat));
                goto failcloseandfree;
            }
            else if (EP_STAT_DETAIL(estat) != EP_STAT_DETAIL(GDPFS_STAT_EOF))
            {
                estat = GDPFS_STAT_DIR_NOT_EMPTY;
                goto failcloseandfree;
            }
        }
        gdpfs_file_close(fh2);
    }
    else if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Parent directory is corrupt:\"%s\": %d", filepath2, EP_STAT_DETAIL(estat));
        goto failandfree;
    }

    estat = _add_at_offset(fh, insert_offset, file, gname);
    ep_mem_free(file_mem);
    if (!EP_STAT_ISOK(estat))
        return estat;

    return GDPFS_STAT_OK;

failcloseandfree:
    gdpfs_file_close(fh2);
failandfree:
    ep_mem_free(file_mem);
    return estat;
}

EP_STAT
gdpfs_dir_remove_file_at_path(const char *filepath)
{
    EP_STAT estat;
    uint64_t fh;
    char *file, *file_mem;

    estat = _open_parent_dir(&fh, filepath, &file, &file_mem);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open parent dir of \"%s\"", filepath);
        return estat;
    }

    printf("removing %s (trash this print)\n", filepath);
    estat = gdpfs_dir_remove(fh, file);
    gdpfs_file_close(fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to add file:\"%s\"", filepath);
        goto end;
    }

end:
    ep_mem_free(file_mem);
    return estat;
}

static EP_STAT
_find_insert_offset(off_t* offset_ptr, uint64_t fh, const char* name,
        gdpfs_file_gname_t gname_if_exists)
{
    size_t size;
    off_t offset;
    gdpfs_dir_entry_phys_t phys_ent;

    off_t insert_offset;
    bool insert_offset_set = false;

    offset = 0;
    while (true)
    {
        size = gdpfs_file_read(fh, &phys_ent, sizeof(gdpfs_dir_entry_phys_t), offset);
        if (size == 0)
            break;
        else if (size != sizeof(gdpfs_dir_entry_phys_t))
            return GDPFS_STAT_CORRUPT;
        else if (phys_ent.in_use)
        {
            if (strcmp(phys_ent.name, name) == 0)
            {
                if (gname_if_exists)
                    memcpy(gname_if_exists, phys_ent.gname, sizeof(gdpfs_file_gname_t));
                *offset_ptr = offset;
                return GDPFS_STAT_FILE_EXISTS;
            }
        }
        else if (!insert_offset_set)
        {
            insert_offset = offset;
            insert_offset_set = true;
        }
        offset += size;
    }

    if (insert_offset_set)
        *offset_ptr = insert_offset;
    else
        *offset_ptr = offset;

    return GDPFS_STAT_OK;
}

static EP_STAT
_add_at_offset(uint64_t fh, off_t offset, const char *name, gdpfs_file_gname_t gname)
{
    size_t size;
    gdpfs_dir_entry_phys_t phys_ent;

    memcpy(phys_ent.gname, gname, sizeof(gdpfs_file_gname_t));
    phys_ent.in_use = true;
    strncpy(phys_ent.name, name, NAME_MAX_GDPFS);
    phys_ent.name[NAME_MAX_GDPFS] = '\0';
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

EP_STAT
gdpfs_dir_remove(uint64_t fh, const char *name)
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

EP_STAT
gdpfs_dir_read(uint64_t fh, gdpfs_dir_entry_t *ent, off_t offset)
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
    strncpy(ent->name, phys_ent.name, NAME_MAX_GDPFS + 1);
    ent->offset = offset;
    return GDPFS_STAT_OK;
}
