#include "gdpfs_file.h"

#include "gdpfs_log.h"
#include "gdpfs_stat.h"

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <string.h>

#include "gdpfs.h"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// TODO: internal file structure. Rename this
struct gdpfs_entry
{
    size_t file_size;
    off_t ent_offset;
    size_t ent_size;
    bool is_dir;
    uint64_t magic;
};

typedef struct gdpfs_entry gdpfs_entry_t;

EP_STAT gdpfs_file_open(gdpfs_file_t **file, char *name, bool ro_mode)
{
    EP_STAT estat;

    *file = ep_mem_zalloc(sizeof(gdpfs_file_t));
    if (!*file)
    {
        estat = GDPFS_STAT_OOMEM;
        goto fail0;
    }
    estat = gdpfs_log_open(&(*file)->log_handle, name, ro_mode);
    if (!EP_STAT_ISOK(estat))
    {
        goto fail0;
    }
    (*file)->ro_mode = ro_mode;
    return estat;

fail0:
    ep_mem_free(file);
    return estat;
}

EP_STAT gdpfs_file_close(gdpfs_file_t *file)
{
    EP_STAT estat;

    estat = gdpfs_log_close(file->log_handle);
    ep_mem_free(file);

    return estat;
}

static size_t do_read_starting_at_rec_no(gdpfs_file_t *file, char *buf,
    size_t size, off_t offset, gdpfs_recno_t rec_no)
{
    EP_STAT estat;
    size_t data_size;
    off_t read_start;
    size_t read_size;
    size_t left_read_size;
    size_t read_so_far;
    gdpfs_entry_t entry;
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
    if (gdpfs_log_ent_read(log_ent, &entry, sizeof(gdpfs_entry_t)) != sizeof(gdpfs_entry_t)
        || data_size != sizeof(gdpfs_entry_t) + entry.ent_size)
    {
        ep_app_error("Corrupt log entry.");
        goto fail0;
    }

    // limit read size to file size. Even though we may encounter a larger file
    // size deeper down, we don't want to read its data since that means that
    // the log was truncated at some point and everything beyond the truncation
    // point should be 0s.
    size = max(min(entry.file_size - offset, size), 0);
    read_start = max(offset, entry.ent_offset);
    read_size = max(min(offset + size - read_start,
                        entry.ent_offset + entry.ent_size - read_start), 0);
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

size_t gdpfs_file_read(gdpfs_file_t *file, char *buf, size_t size,
        off_t offset)
{
    memset(buf, 0, size);
    return do_read_starting_at_rec_no(file, buf, size, offset, -1);
}

static size_t do_write(gdpfs_file_t *file, size_t file_size, const char *buf,
        size_t size, off_t offset)
{
    EP_STAT estat;
    size_t written;
    gdpfs_log_ent_t *log_ent;
    gdpfs_entry_t entry = {
        .file_size = file_size,
        .ent_offset = offset,
        .ent_size = size,
    };

    // can't write if file is read only
    if (file->ro_mode)
        return 0;

    log_ent = gdpfs_log_ent_new();
    if (!log_ent)
    {
        written = 0;
        goto fail0;
    }
    if (gdpfs_log_ent_write(log_ent, &entry, sizeof(gdpfs_entry_t)) != 0)
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

size_t gdpfs_file_write(gdpfs_file_t *file, const char *buf, size_t size,
        off_t offset)
{
    size_t file_size;
    size_t current_size;
    size_t potential_size;

    current_size = gdpfs_file_size(file);
    potential_size = offset + size;
    file_size = max(current_size, potential_size);

    return do_write(file, file_size, buf, size, offset);
}

int gdpfs_file_truncate(gdpfs_file_t *file, size_t file_size)
{
    return do_write(file, file_size, NULL, 0, 0);
}

size_t gdpfs_file_size(gdpfs_file_t *file)
{
    EP_STAT estat;
    size_t size;
    gdpfs_entry_t curr_entry;
    gdpfs_log_ent_t *log_ent;

    estat = gdpfs_log_ent_open(file->log_handle, &log_ent, -1);
    if (!EP_STAT_ISOK(estat))
    {
        if (EP_STAT_IS_SAME(estat, GDPFS_STAT_NOTFOUND))
        {
            // no entries yet so file size is 0
            size = 0;
        }
        else
        {
            // error
            size = 0;
        }
        goto fail0;
    }
    else
    {
        // TODO: check that this returns sizeof(gdpfs_entry_t). If not, corruption
        gdpfs_log_ent_read(log_ent, &curr_entry, sizeof(gdpfs_entry_t));
        size = curr_entry.file_size;
    }
    // remember to free our resources
    gdpfs_log_ent_close(log_ent);
    return size;

fail0:
    return size;
}

