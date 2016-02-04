#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <sysexits.h>

#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include "gdpfs_log.h"
#include "gdpfs_stat.h"

#define MAGIC_NUMBER 0xb531479b64f64e0d

struct gdpfs_entry
{
    size_t file_size;
    off_t ent_offset;
    size_t ent_size;
    bool is_dir;
    uint64_t magic;
};

typedef struct gdpfs_entry gdpfs_entry_t;

static char *log_path;
static bool read_only = false;

static gdpfs_log_t *log_handle;

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

static size_t
current_file_size()
{
    EP_STAT estat;
    size_t size;
    gdpfs_entry_t curr_entry;
    gdpfs_log_ent_t *log_ent;

    estat = gdpfs_log_ent_open(log_handle, &log_ent, -1);
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

fail0:
    return size;
}

static int
gdpfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, log_path) == 0) {
        stbuf->st_mode = S_IFREG | (read_only ? 0444 : 0644);
        stbuf->st_nlink = 1;
        stbuf->st_size = current_file_size();
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
    } else
        res = -ENOENT;
    return res;
}

static int
gdpfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
              off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, log_path + 1, NULL, 0);
    return 0;
}

/*
static int
gdpfs_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, log_path) != 0)
        return -ENOENT;
    return 0;
}
*/

static int
do_read_starting_at_rec_no(char *buf, size_t size, off_t offset, int rec_no)
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

    estat = gdpfs_log_ent_open(log_handle, &log_ent, rec_no);
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
        do_read_starting_at_rec_no(buf, left_read_size, offset, rec_no - 1);
        // assume read of size size succeeds (if not then they're just filled
        // with zeros) and do right read.
        read_so_far = left_read_size + read_size;
        do_read_starting_at_rec_no(buf + read_so_far, size - read_so_far,
                                   offset + read_so_far, rec_no - 1);
    }
    else
    {
        gdpfs_log_ent_close(log_ent);
        do_read_starting_at_rec_no(buf, size, offset, rec_no - 1);
    }
    return size;

fail0:
    gdpfs_log_ent_close(log_ent);
    return 0;
}

static int
do_read(char *buf, size_t size, off_t offset)
{
    memset(buf, 0, size);
    return do_read_starting_at_rec_no(buf, size, offset, -1);
}

static int
gdpfs_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi)
{
    (void) fi;
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return do_read(buf, size, offset);
}

static int
do_write(const char *path, size_t file_size, const char *buf,
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

    if(strcmp(path, log_path) != 0)
        return -ENOENT;

    if (read_only)
        return -EPERM;

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

    estat = gdpfs_log_append(log_handle, log_ent);
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

static int
gdpfs_write(const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    size_t file_size;
    size_t current_size;
    size_t potential_size;

    current_size = current_file_size();
    potential_size = offset + size;
    file_size = max(current_size, potential_size);

    return do_write(path, file_size, buf, size, offset);
}

static int
gdpfs_truncate(const char *path, off_t size)
{
    return do_write(path, size, NULL, 0, 0);
}

static int
gdpfs_create(const char *file, mode_t mode, struct fuse_file_info *info)
{
    printf("Create not implemented. File:\"%s\"\n", file);
    return 0;
}

static int
gdpfs_mkdir(const char * file, mode_t mode)
{
    printf("Mkdir not implemented. File:\"%s\"\n", file);
    return 0;
}

static int
gdpfs_chmod (const char *file, mode_t mode)
{
    printf("Chmod not implemented. File:\"%s\"\n", file);
    return 0;
}

static int
gdpfs_chown (const char *file, uid_t uid, gid_t gid)
{
    printf("Chown not implemented. File:\"%s\"\n", file);
    return 0;
}

static struct fuse_operations gdpfs_oper = {
    .getattr        = gdpfs_getattr,
    .readdir        = gdpfs_readdir,
    //.open           = gdpfs_open,
    .read           = gdpfs_read,
    .write          = gdpfs_write,
    .truncate       = gdpfs_truncate,
    .create         = gdpfs_create,
    .mkdir          = gdpfs_mkdir,
    .chmod          = gdpfs_chmod,
    .chown          = gdpfs_chown,
};

static void
usage(void)
{
    fprintf(stderr,
        "Usage: %s [-hr] logname -- [fuse args]\n"
        "    -h display this usage message and exit\n"
        "    -r mount the filesys in read only mode\n",
        ep_app_getprogname());
    exit(EX_USAGE);
}

static void
close_resources()
{
    EP_STAT estat;
    int status = 0;
    estat = gdpfs_log_close(log_handle);
    if (!EP_STAT_ISOK(estat))
    {
        char sbuf[100];

        ep_app_error("Cannot close GCL:\n    %s",
            ep_stat_tostr(estat, sbuf, sizeof sbuf));
        status = -EP_STAT_DETAIL(estat);
    }
    free(log_path);
    exit(status);
}

static void
sig_int(int sig)
{
    close_resources();
}

int
main(int argc, char *argv[])
{
    EP_STAT estat;
    //gdpfs_log_t *log_handle;
    char *log_name;
    int opt;
    int fuseargc;
    int ret;
    bool show_usage = false;
    char *argv0 = argv[0];

    // we only want to parse gdpfs args, not fuse args. We need to count them.
    for (fuseargc = argc;
         fuseargc > 0 && strcmp(argv[argc - fuseargc], "--") != 0;
         fuseargc--);
    argc -= fuseargc;

    while ((opt = getopt(argc, argv, "hr::")) > 0)
    {
        switch (opt)
        {
        case 'h':
            show_usage = true;
            break;

        case 'r':
            read_only = true;
            break;

        default:
            show_usage = true;
            break;
        }
    }
    argc -= optind;
    argv += optind;

    log_name = argv[0];
    argc--;
    argv++;

    if (show_usage || argc != 0)
        usage();

    if (fuseargc > 0)
    {
        argv++;     // consume the --
        fuseargc--; // don't coun't the --
        argc = fuseargc;
    }

    // re-add argv[0] (fuse needs it).
    argv--;
    argv[0] = argv0;
    argc++;

    log_path = malloc(sizeof(char) * (2 + strlen(log_name)));
    log_path[0] = '/';
    strcpy(log_path + 1, log_name);

    estat = init_gdpfs_log();
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);
    
    estat = gdpfs_log_open(&log_handle, log_name, read_only);
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);

    signal(SIGINT, sig_int);
    ret = fuse_main(argc, argv, &gdpfs_oper, NULL);
    close_resources();
    return ret;
}