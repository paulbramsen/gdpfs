#include "gdpfs_file.h"
#include "gdpfs_stat.h"
#include "gdpfs.h"

#include <ep/ep_app.h>
#include <ep/ep_hash.h>
#include <string.h>

#include "bitmap.h"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define MAGIC_NUMBER 0xb531479b64f64e0d

// TODO: file should store a copy of the meta data. This makes writes easier.
typedef struct
{
    gdpfs_log_t *log_handle;
    char *hash_key;
    uint32_t ref_count;
} gdpfs_file_t;

typedef struct
{
    size_t file_size;
    uint16_t file_perm;
    gdpfs_file_type_t file_type;
    off_t ent_offset;
    size_t ent_size;
    uint64_t magic;
} gdpfs_fmeta_t;

#define MAX_FHS 256
static bitmap_t *fhs;
static gdpfs_file_t **files;
static EP_HASH *file_hash;

// Private Functions
static size_t do_write(uint64_t fh, const char *buf,
        size_t size, off_t offset, const gdpfs_file_info_t *info);
static gdpfs_file_t *lookup_fh(uint64_t fh);


EP_STAT
init_gdpfs_file(gdpfs_file_mode_t fs_mode)
{
    EP_STAT estat;

    files = ep_mem_zalloc(sizeof(gdpfs_file_t *) * MAX_FHS);
    if (files == NULL)
        goto failoom;
    fhs = bitmap_create(MAX_FHS);
    if (fhs == NULL)
        goto failoom;
    file_hash = ep_hash_new("file_hash", NULL, MAX_FHS);
    if (file_hash == NULL)
        goto failoom;
    estat = init_gdpfs_log(fs_mode);
    return estat;

failoom:
    ep_mem_free(files);
    bitmap_free(fhs);
    return GDPFS_STAT_OOMEM;
}

void
stop_gdpfs_file()
{
    ep_hash_free(file_hash);
    bitmap_free(fhs);
    // TODO: close the logs
}

EP_STAT
gdpfs_file_create(uint64_t* fhp, gdpfs_file_gname_t log_iname,
        gdpfs_file_type_t type, gdpfs_file_perm_t perm)
{
    EP_STAT estat;
    uint64_t fh;

    estat = gdpfs_log_create(log_iname);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to create file");
        return estat;
    }

    fh = gdpfs_file_open_init(&estat, log_iname, type, perm, true);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to initialize file");
        return estat;
    }

    if (fhp)
        *fhp = fh;
    return GDPFS_STAT_OK;
}

static uint64_t
open_file(EP_STAT *ret_stat, gdpfs_file_gname_t log_name, gdpfs_file_type_t type,
        gdpfs_file_mode_t perm, bool init, bool strict_init)
{
    EP_STAT estat;
    uint64_t fh;
    gdpfs_file_t *file;
    gdpfs_file_info_t current_info;

    fh = bitmap_reserve(fhs);
    if (fh == -1)
    {
        estat = GDPFS_STAT_OOMEM;
        goto fail1;
    }

    // check if the file is already open and in the hash
    file = ep_hash_search(file_hash, sizeof(gdpfs_file_gname_t), log_name);

    // if there file isn't currently open, create and open it
    if (!file)
    {
        file = ep_mem_zalloc(sizeof(gdpfs_file_t));
        if (!file)
        {
            if (ret_stat)
                *ret_stat = GDPFS_STAT_OOMEM;
            goto fail0;
        }
        estat = gdpfs_log_open(&file->log_handle, log_name);
        if (!EP_STAT_ISOK(estat))
        {
            if (ret_stat)
                *ret_stat = estat;
            goto fail0;
        }
        file->hash_key = ep_mem_zalloc(sizeof(gdpfs_file_gname_t));
        if (!file->hash_key)
        {
            if (ret_stat)
                *ret_stat = GDPFS_STAT_OOMEM;
            goto fail0;
        }
        memcpy(file->hash_key, log_name, sizeof(gdpfs_file_gname_t));
        // add to hash table at very end to make handling failure cases easier
        ep_hash_insert(file_hash, sizeof(gdpfs_file_gname_t), log_name, file);
    }
    file->ref_count++;
    files[fh] = file;

    // check type and initialize if necessary
    if (type != GDPFS_FILE_TYPE_UNKNOWN)
    {
        estat = gdpfs_file_info(fh, &current_info);
        if (!EP_STAT_ISOK(estat))
        {
            ep_app_error("Failed to read current file info.");
            *ret_stat = estat;
            goto fail2;
        }
        if (init)
        {
            if (current_info.file_type == GDPFS_FILE_TYPE_NEW)
            {
                current_info.file_size = 0;
                current_info.file_type = type;
                current_info.file_perm = perm;
                do_write(fh, NULL, 0, 0, &current_info);
            }
            else if (current_info.file_type == GDPFS_FILE_TYPE_UNKNOWN || strict_init)
            {
                if (ret_stat)
                    *ret_stat = GDPFS_STAT_INVLDFTYPE;
                goto fail2;
            }
        }
        else if (current_info.file_type != type)
        {
            if (ret_stat)
                *ret_stat = GDPFS_STAT_INVLDFTYPE;
            goto fail2;
        }
    }

    // success
    if (ret_stat)
        *ret_stat = GDPFS_STAT_OK;

    return fh;

fail0:
    bitmap_release(fhs, fh);
    if (file)
    {
        ep_mem_free(file->hash_key);
        ep_mem_free(file);
    }
fail1:
    return -1;
fail2:
    gdpfs_file_close(fh);
    return -1;
}

uint64_t
gdpfs_file_open(EP_STAT *ret_stat, gdpfs_file_gname_t name)
{
    return open_file(ret_stat, name, GDPFS_FILE_TYPE_UNKNOWN, 0, false, true);
}

uint64_t
gdpfs_file_open_type(EP_STAT *ret_stat, gdpfs_file_gname_t name,
        gdpfs_file_type_t type)
{
    return open_file(ret_stat, name, type, 0, false, true);
}

uint64_t
gdpfs_file_open_init(EP_STAT *ret_stat, gdpfs_file_gname_t name,
        gdpfs_file_type_t type, gdpfs_file_perm_t perm, bool strict_init)
{
    return open_file(ret_stat, name, type, perm, true, strict_init);
}

EP_STAT
gdpfs_file_close(uint64_t fh)
{
    EP_STAT estat;
    gdpfs_file_t *file;

    file = lookup_fh(fh);
    if (file == NULL)
        return GDPFS_STAT_BADFH;
    bitmap_release(fhs, fh);

    if (--file->ref_count == 0)
    {
        ep_hash_delete(file_hash, sizeof(gdpfs_file_gname_t), file->hash_key);
        estat = gdpfs_log_close(file->log_handle);
        ep_mem_free(file->hash_key);
        ep_mem_free(file);
    }
    return estat;
}

static size_t do_read_starting_at_rec_no(gdpfs_file_t *file, char *buf, size_t size,
        off_t offset, gdpfs_recno_t rec_no)
{
    EP_STAT estat;
    size_t data_size;
    off_t read_start;
    size_t read_size;
    size_t left_read_size;
    size_t read_so_far;
    gdpfs_fmeta_t entry;
    gdpfs_log_ent_t *log_ent;

    if (size == 0)
    {
        return 0;
    }

    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, rec_no);
    if (!EP_STAT_ISOK(estat))
    {
        if (!EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
        {
            char sbuf[100];
            ep_app_error("Issue while reading GCL:\n    %s",
                ep_stat_tostr(estat, sbuf, sizeof sbuf));
        }
        goto fail0;
    }
    data_size = gdpfs_log_ent_length(log_ent);
    if (gdpfs_log_ent_read(log_ent, &entry, sizeof(gdpfs_fmeta_t)) != sizeof(gdpfs_fmeta_t)
        || data_size != sizeof(gdpfs_fmeta_t) + entry.ent_size)
    {
        ep_app_error("Corrupt log entry.");
        goto fail0;
    }

    // limit read size to file size. Even though we may encounter a larger file
    // size deeper down, we don't want to read its data since that means that
    // the log was truncated at some point and everything beyond the truncation
    // point should be 0s.
    size = max(min((int64_t)entry.file_size - (int64_t)offset, (int64_t)size), 0);
    read_start = max(offset, entry.ent_offset);
    read_size = max(min((int64_t)offset + (int64_t)size - (int64_t)read_start,
                        (int64_t)entry.ent_offset + (int64_t)entry.ent_size - (int64_t)read_start), 0);
    if (read_size > 0)
    {

        left_read_size = read_start - offset;

        // this log entry has data we want
        // TODO: error checking
        gdpfs_log_ent_read(log_ent, NULL, read_start - entry.ent_offset);
        gdpfs_log_ent_read(log_ent, buf + left_read_size, read_size);
        gdpfs_log_ent_close(log_ent);

        // do left read
        do_read_starting_at_rec_no(file, buf, left_read_size, offset, rec_no - 1);
        // assume read of size size succeeds (if not then they're just filled
        // with zeros) and do right read.
        read_so_far = left_read_size + read_size;
        do_read_starting_at_rec_no(file, buf + read_so_far, size - read_so_far,
                                   offset + read_so_far, rec_no - 1);
    }
    else
    {
        gdpfs_log_ent_close(log_ent);
        do_read_starting_at_rec_no(file, buf, size, offset, rec_no - 1);
    }
    return size;

fail0:
    gdpfs_log_ent_close(log_ent);
    return 0;
}

static size_t
do_read(uint64_t fh, char *buf, size_t size, off_t offset)
{
    gdpfs_file_t *file;

    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    memset(buf, 0, size);
    return do_read_starting_at_rec_no(files[fh], buf, size, offset, -1);
}

size_t
gdpfs_file_read(uint64_t fh, void *buf, size_t size, off_t offset)
{
    return do_read(fh, buf, size, offset);
}

// TODO: do_write should probably return an EP_STAT so we can error check
static size_t
do_write(uint64_t fh, const char *buf, size_t size, off_t offset,
    const gdpfs_file_info_t *info)
{
    EP_STAT estat;
    gdpfs_file_t *file;
    size_t written;
    gdpfs_log_ent_t *log_ent;
    gdpfs_fmeta_t entry = {
        .file_size  = info->file_size,
        .file_type  = info->file_type,
        .file_perm  = info->file_perm,
        .ent_offset = offset,
        .ent_size   = size,
        .magic      = MAGIC_NUMBER,
    };

    // TODO: where are perms checked?

    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    log_ent = gdpfs_log_ent_new();
    if (!log_ent)
    {
        written = 0;
        goto fail0;
    }
    if (gdpfs_log_ent_write(log_ent, &entry, sizeof(gdpfs_fmeta_t)) != 0)
        goto fail0;
    if (gdpfs_log_ent_write(log_ent, buf, size) != 0)
        goto fail0;

    estat = gdpfs_log_append(file->log_handle, log_ent);
    if (!EP_STAT_ISOK(estat))
    {
        written = 0;
    }
    else
    {
        written = size;
    }

    // remember to free our resources
    gdpfs_log_ent_close(log_ent);
    return written;

fail0:
    return written;
}

size_t
gdpfs_file_write(uint64_t fh, const void *buf, size_t size, off_t offset)
{
    EP_STAT estat;
    size_t potential_size;
    gdpfs_file_info_t info;

    estat = gdpfs_file_info(fh, &info);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to read file size.");
        return 0;
    }
    potential_size = offset + size;
    info.file_size = max(info.file_size, potential_size);

    return do_write(fh, buf, size, offset, &info);
}

// TODO return an EP_STAT
int
gdpfs_file_ftruncate(uint64_t fh, size_t file_size)
{
    EP_STAT estat;
    gdpfs_file_info_t info;

    estat = gdpfs_file_info(fh, &info);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("failed to get file info");
        return 0;
    }
    info.file_size = 0;
    return do_write(fh, NULL, 0, 0, &info);
}

EP_STAT
gdpfs_file_set_perm(uint64_t fh, gdpfs_file_perm_t perm)
{
    EP_STAT estat;
    gdpfs_file_info_t info;

    estat = gdpfs_file_info(fh, &info);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("failed to get file info");
        return estat;
    }
    info.file_perm = perm;
    do_write(fh, NULL, 0, 0, &info);
    return GDPFS_STAT_OK;
}

EP_STAT
gdpfs_file_set_info(uint64_t fh, gdpfs_file_info_t info)
{
    do_write(fh, NULL, 0, 0, &info);
    return GDPFS_STAT_OK;
}

EP_STAT
gdpfs_file_info(uint64_t fh, gdpfs_file_info_t* info)
{
    EP_STAT estat;
    gdpfs_file_t *file;
    gdpfs_fmeta_t curr_entry;
    gdpfs_log_ent_t *log_ent;
    size_t read;

    memset(info, 0, sizeof(gdpfs_file_info_t));

    file = lookup_fh(fh);
    if (file == NULL)
    {
        estat = GDPFS_STAT_BADFH;
        goto fail0;
    }

    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, -1);
    if (EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
    {
        // no entries yet so file type is new
        info->file_size = 0;
        info->file_type = GDPFS_FILE_TYPE_NEW;
        info->file_perm = 0;
    }
    else if (!EP_STAT_ISOK(estat))
    {
        goto fail0;
    }
    else
    {
        read = gdpfs_log_ent_read(log_ent, &curr_entry, sizeof(gdpfs_fmeta_t));
        if (read != sizeof(gdpfs_fmeta_t))
        {
            estat = GDPFS_STAT_CORRUPT;
            goto fail0;
        }
        info->file_size = curr_entry.file_size;
        info->file_type = curr_entry.file_type;
        info->file_perm = curr_entry.file_perm;
        // remember to free our resources
        gdpfs_log_ent_close(log_ent);

    }
    return GDPFS_STAT_OK;

fail0:
    info->file_size = 0;
    info->file_type = GDPFS_FILE_TYPE_UNKNOWN;
    info->file_perm = 0;

    return estat;
}

static gdpfs_file_t *
lookup_fh(uint64_t fh)
{
    int set;

    set = bitmap_is_set(fhs, fh);
    if (set < 0)
    {
        ep_app_error("recieved bad file descriptor:%Lu", fh);
        return NULL;
    }
    return files[fh];
}

void
gdpfs_file_gname(uint64_t fh, gdpfs_file_gname_t gname)
{
    memcpy(gname, files[fh]->hash_key, sizeof(gdpfs_file_gname_t));
}
