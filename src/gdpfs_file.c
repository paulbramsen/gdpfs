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
struct gdpfs_file
{
    gdpfs_log_t *log_handle;
    gdpfs_file_mode_t mode;
    char *hash_key;
    uint32_t ref_count;
};
typedef struct gdpfs_file gdpfs_file_t;

struct gdpfs_fmeta
{
    size_t file_size;
    off_t ent_offset;
    size_t ent_size;
    gdpfs_file_type_t type;
    uint64_t magic;
};
typedef struct gdpfs_fmeta gdpfs_fmeta_t;

#define MAX_FHS 256
static bitmap_t *fhs;
static gdpfs_file_t **files;
static EP_HASH *file_hash;

// Private Functions 
static size_t do_write(uint64_t fh, size_t file_size, const char *buf,
        size_t size, off_t offset, gdpfs_file_type_t type);
static gdpfs_file_type_t file_type(uint64_t fh);
static gdpfs_file_t *lookup_fh(uint64_t fh);


EP_STAT init_gdpfs_file()
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
    estat = init_gdpfs_log();
    return estat;

failoom:
    ep_mem_free(files);
    bitmap_free(fhs);
    return GDPFS_STAT_OOMEM;
}

void stop_gdpfs_file()
{
    ep_hash_free(file_hash);
    bitmap_free(fhs);
    // TODO: close the log
}

// TODO: make log_name the global name
static uint64_t open_file(EP_STAT *ret_stat, char *log_name,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type, bool init,
        bool strict_init)
{
    EP_STAT stat;
    uint64_t fh;
    gdpfs_file_t *file;
    gdpfs_log_mode_t log_mode;
    gdpfs_file_type_t current_type;

    switch (mode)
    {
    case GDPFS_FILE_MODE_RO:
        log_mode = GDPFS_LOG_MODE_RO;
        break;    
    case GDPFS_FILE_MODE_RW:
        log_mode = GDPFS_LOG_MODE_RA;
        break;
    case GDPFS_FILE_MODE_WO:
        log_mode = GDPFS_LOG_MODE_AO;
        break;
    default:
        if (ret_stat)
            *ret_stat = GDPFS_STAT_INVLDPARAM;
        goto fail1;
    }

    fh = bitmap_reserve(fhs);
    if (fh == -1)
    {
        stat = GDPFS_STAT_OOMEM;
        goto fail1;
    }

    // TODO: use the mode as part of the key
    // check if the file is already open and in the hash
    file = ep_hash_search(file_hash, strlen(log_name), log_name);

    if (!file)
    {
        // if the file is not yet in the cash, we need to create it
        file = ep_mem_zalloc(sizeof(gdpfs_file_t));
        if (!file)
        {
            stat = GDPFS_STAT_OOMEM;
            goto fail0;
        }
        ep_hash_insert(file_hash, strlen(log_name), log_name, file);
        stat = gdpfs_log_open(&file->log_handle, log_name, log_mode);
        if (!EP_STAT_ISOK(stat))
        {
            goto fail0;
        }
        file->mode = mode;
        file->hash_key = ep_mem_zalloc(strlen(log_name) + 1);
        if (!file->hash_key)
        {
            stat = GDPFS_STAT_OOMEM;
            goto fail0;
        }
        strcpy(file->hash_key, log_name);
        if (ret_stat != NULL)
            *ret_stat = stat;
    }
    file->ref_count++;
    files[fh] = file;

    // check type and initialize if necessary
    if (type != GDPFS_FILE_TYPE_UNKNOWN)
    {
        current_type = file_type(fh);
        if (init)
        {
            if (current_type == GDPFS_FILE_TYPE_NEW)
            {
                do_write(fh, 0, NULL, 0, 0, type);
            }
            else if (current_type == GDPFS_FILE_TYPE_UNKNOWN || strict_init)
            {
                *ret_stat = GDPFS_STAT_INVLDFTYPE;
                goto fail0;
            }
        }
        else if (current_type != type)
        {
            *ret_stat = GDPFS_STAT_INVLDFTYPE;
            goto fail0;
        }
    }

    return fh;

fail0:
    files[fh] = NULL;
    bitmap_release(fhs, fh);
    ep_mem_free(file->hash_key);
    ep_mem_free(file);
fail1:
    return -1;
}

uint64_t gdpfs_file_open(EP_STAT *ret_stat, char *name, gdpfs_file_mode_t mode)
{
    return open_file(ret_stat, name, mode, GDPFS_FILE_TYPE_UNKNOWN, false, true);
}

uint64_t gdpfs_file_open_type(EP_STAT *ret_stat, char *name,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type)
{
    return open_file(ret_stat, name, mode, type, false, true);
}

uint64_t gdpfs_file_open_init(EP_STAT *ret_stat, char *name,
        gdpfs_file_mode_t mode, gdpfs_file_type_t type, bool strict_init)
{
    return open_file(ret_stat, name, mode, type, true, strict_init);
}

EP_STAT gdpfs_file_close(uint64_t fh)
{
    EP_STAT estat;
    gdpfs_file_t *file;

    file = lookup_fh(fh);
    if (file == NULL)
        return GDPFS_STAT_BADFH;
    bitmap_release(fhs, fh);

    if (--file->ref_count == 0)
    {
        ep_hash_delete(file_hash, strlen(file->hash_key), file->hash_key);
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

static size_t do_read(uint64_t fh, char *buf, size_t size, off_t offset)
{
    gdpfs_file_t *file;

    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    memset(buf, 0, size);
    return do_read_starting_at_rec_no(files[fh], buf, size, offset, -1);
}

size_t gdpfs_file_read(uint64_t fh, void *buf, size_t size, off_t offset)
{
    return do_read(fh, buf, size, offset);
}

static size_t do_write(uint64_t fh, size_t file_size, const char *buf,
        size_t size, off_t offset, gdpfs_file_type_t type)
{
    EP_STAT estat;
    gdpfs_file_t *file;
    size_t written;
    gdpfs_log_ent_t *log_ent;
    gdpfs_fmeta_t entry = {
        .file_size = file_size,
        .ent_offset = offset,
        .ent_size = size,
        .type = type,
        .magic = MAGIC_NUMBER,
    };

    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    // can't write if file is read only
    if (file->mode == GDPFS_FILE_MODE_RO)
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

size_t gdpfs_file_write(uint64_t fh, const void *buf, size_t size, off_t offset)
{
    EP_STAT estat;
    size_t file_size;
    size_t potential_size;
    gdpfs_file_info_t info;
    
    info = gdpfs_file_info(fh, &estat);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to read file size.");
        return 0;
    }
    potential_size = offset + size;
    file_size = max(info.size, potential_size);

    return do_write(fh, file_size, buf, size, offset, info.type);
}

int gdpfs_file_ftruncate(uint64_t fh, size_t file_size)
{
    gdpfs_file_type_t type;

    type = file_type(fh);
    return do_write(fh, file_size, NULL, 0, 0, type);
}

gdpfs_file_info_t gdpfs_file_info(uint64_t fh, EP_STAT *ret_stat)
{
    EP_STAT estat;
    gdpfs_file_t *file;
    gdpfs_file_info_t info;
    gdpfs_fmeta_t curr_entry;
    gdpfs_log_ent_t *log_ent;
    size_t read;

    memset(&info, 0, sizeof(info));
    
    file = lookup_fh(fh);
    if (file == NULL)
    {
        goto fail0;
    }

    info.mode = file->mode;
    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, -1);
    if (!EP_STAT_ISOK(estat))
    {
        if (ret_stat)
            *ret_stat = estat;
        goto fail0;
    }
    else
    {
        // TODO: check that this returns sizeof(gdpfs_fmeta_t). If not, corruption
        read = gdpfs_log_ent_read(log_ent, &curr_entry, sizeof(gdpfs_fmeta_t));
        if (read != sizeof(gdpfs_fmeta_t))
        {
            if (ret_stat)
                *ret_stat = GDPFS_STAT_CORRUPT;
            goto fail0;
        }
        info.size = curr_entry.file_size;
        info.type = curr_entry.type;
    }
    // remember to free our resources
    gdpfs_log_ent_close(log_ent);
    return info;

fail0:
    info.size = 0;
    info.type = GDPFS_FILE_TYPE_UNKNOWN;
    return info;
}

static gdpfs_file_t *lookup_fh(uint64_t fh)
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

// TODO: generalize this stuff into a log read meta style function
static gdpfs_file_type_t file_type(uint64_t fh)
{
    EP_STAT estat;
    gdpfs_file_t *file;
    gdpfs_file_type_t type;
    gdpfs_fmeta_t curr_entry;
    gdpfs_log_ent_t *log_ent;

    file = lookup_fh(fh);
    if (file == NULL)
        return 0;

    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, -1);
    if (!EP_STAT_ISOK(estat))
    {
        if (EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
        {
            // no entries yet so file type is new
            type = GDPFS_FILE_TYPE_NEW;
        }
        else
        {
            // error
            type = GDPFS_FILE_TYPE_UNKNOWN;
        }
        goto fail0;
    }
    else
    {
        // TODO: check that this returns sizeof(gdpfs_fmeta_t). If not, corruption
        gdpfs_log_ent_read(log_ent, &curr_entry, sizeof(gdpfs_fmeta_t));
        type = curr_entry.type;
    }
    // remember to free our resources
    gdpfs_log_ent_close(log_ent);
    return type;

fail0:
    return type;
}

void gdpfs_file_gname(uint64_t fh, gdpfs_file_gname_t gname)
{
    gdpfs_log_gname(files[fh]->log_handle, gname);
}
