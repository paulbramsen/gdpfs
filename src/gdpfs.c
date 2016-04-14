#include "gdpfs.h"

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
#include "gdpfs_file.h"
#include "gdpfs_dir.h"
#include "gdpfs_stat.h"

struct gdpfs_entry
{
    size_t file_size;
    off_t ent_offset;
    size_t ent_size;
    bool is_dir;
    uint64_t magic;
};

typedef struct gdpfs_entry gdpfs_entry_t;

static gdpfs_file_mode_t mode;

void debug() {};

static int
gdpfs_getattr(const char *path, struct stat *stbuf)
{
    EP_STAT estat;
    gdpfs_file_info_t info;
    uint64_t fh;

    fh = gdpfs_dir_open_file_at_path(&estat, path, mode, GDPFS_FILE_TYPE_UNKNOWN);
    if (!EP_STAT_ISOK(estat))
        return -ENOENT;
    estat = gdpfs_file_info(fh, &info);
    if (!EP_STAT_ISOK(estat))
        goto fail0;

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_nlink = 1; // TODO: figure out a sane value for this
    stbuf->st_size = info.size;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    if (info.type == GDPFS_FILE_TYPE_REGULAR)
    {
        stbuf->st_mode = S_IFREG;
        switch (mode)
        {
        case GDPFS_FILE_MODE_RO:
            stbuf->st_mode |= 0544;
            break;
        case GDPFS_FILE_MODE_RW:
            stbuf->st_mode |= 0744;
            break;
        case GDPFS_FILE_MODE_WO:
            stbuf->st_mode |= 0200;
            break;
        default:
            goto fail0;
        }
    }
    else if (info.type == GDPFS_FILE_TYPE_DIR)
    {
        stbuf->st_mode = S_IFDIR;
        switch (mode)
        {
        case GDPFS_FILE_MODE_RO:
            stbuf->st_mode |= 0555;
            break;
        case GDPFS_FILE_MODE_RW:
            stbuf->st_mode |= 0755;
            break;
        case GDPFS_FILE_MODE_WO:
            stbuf->st_mode |= 0200;
            break;
        default:
            goto fail0;
        }
    }
    else
    {
        goto fail0;
    }

    gdpfs_file_close(fh);
    return 0;

fail0:
    gdpfs_file_close(fh);
    return -ENOENT;
}

static int
open_(const char *path, uint64_t *fh)
{
    EP_STAT estat;

    *fh = gdpfs_dir_open_file_at_path(&estat, path, mode, GDPFS_FILE_TYPE_REGULAR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open file at path:\"%s\"", path);
        return -ENOENT;
    }
    return 0;
}

static int
gdpfs_open(const char *path, struct fuse_file_info *fi)
{
    return open_(path, &fi->fh);
}

static int
gdpfs_opendir(const char *path, struct fuse_file_info *fi)
{
    EP_STAT estat;

    fi->fh = gdpfs_dir_open_file_at_path(&estat, path, mode, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to opendir at path:\"%s\"", path);
        return -ENOENT;
    }
    return 0;
}

static int
release_(const char *path, uint64_t fh)
{
    (void)path;
    gdpfs_file_close(fh);
    return 0;
}

static int
gdpfs_release(const char *path, struct fuse_file_info *fi)
{
    return release_(path, fi->fh);
}

static int
gdpfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    gdpfs_file_close(fi->fh);
    return 0;
}

static int
gdpfs_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi)
{
    if (strcmp(path, "/gdpfs/bin/build/gdpfs.o") == 0) {
        debug();
    }
    (void)path;
    return gdpfs_file_read(fi->fh, buf, size, offset);
}

static int
gdpfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
              off_t offset, struct fuse_file_info *fi)
{
    EP_STAT estat;
    int fill_result;
    gdpfs_dir_entry_t ent;

    (void) fi;

    if (offset == 0)
        offset = 1;

    do {
        if (offset == 1)
        {
            offset++;
            fill_result = filler(buf, ".", NULL, offset);
        }
        else if (offset == 2)
        {
            offset++;
            fill_result = filler(buf, "..", NULL, offset);
        }
        else
        {
            estat = gdpfs_dir_read(fi->fh, &ent, offset - 3);
            if (EP_STAT_IS_SAME(estat, GDPFS_STAT_EOF))
            {
                break;
            }
            else if (!EP_STAT_ISOK(estat))
            {
                ep_app_error("Unable to read directory at path %s", path);
            }
            offset = ent.offset + 3;
            fill_result = filler(buf, ent.name, NULL, offset);
        }
    } while (fill_result != 1);
    return 0;
}

static int
gdpfs_write(const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    if (strcmp(path, "/gdpfs/bin/build/gdpfs.o") == 0) {
        debug();
    }
    (void)path;
    return gdpfs_file_write(fi->fh, buf, size, offset);
}

static int
gdpfs_truncate(const char *file, off_t file_size)
{
    int res;
    uint64_t fh;

    if ((res = open_(file, &fh)) != 0)
        return res;
    res = gdpfs_file_ftruncate(fh, file_size);
    res = release_(file, fh) || res;
    return res;
}

static int
gdpfs_ftruncate(const char *file, off_t file_size, struct fuse_file_info *fi)
{
    (void)file;
    return gdpfs_file_ftruncate(fi->fh, file_size);
}

static int
gdpfs_create(const char *filepath, mode_t mode_, struct fuse_file_info *fi)
{
    EP_STAT estat;
    gdpfs_file_gname_t newfile_gname;
    uint64_t fh;

    (void)mode_;

    if (strlen(filepath) == 0)
        return -ENOENT;
    if (filepath[strlen(filepath) - 1] == '/')
        return -EISDIR;
    if (strcmp(filepath, "/gdpfs/bin/build/gdpfs.o") == 0) {
        debug();
    }

    estat = gdpfs_file_create(&fh, newfile_gname, mode, GDPFS_FILE_TYPE_REGULAR);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }

    estat = gdpfs_dir_add_file_at_path(newfile_gname, filepath, mode);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }

    fi->fh = fh;

    return 0;
}

static int
gdpfs_unlink(const char *filepath)
{
    EP_STAT estat;

    if (strlen(filepath) == 0)
        return -ENOENT;

    estat = gdpfs_dir_remove_file_at_path(filepath, mode);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }

    return 0;
}

static int
gdpfs_mkdir(const char *filepath, mode_t mode_)
{
    EP_STAT estat;
    gdpfs_file_gname_t newfile_gname;
    uint64_t fh;

    (void)mode_;

    if (strlen(filepath) == 0)
        return -ENOENT;

    estat = gdpfs_file_create(&fh, newfile_gname, mode, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }
    gdpfs_file_close(fh);

    estat = gdpfs_dir_add_file_at_path(newfile_gname, filepath, mode);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }

    return 0;
}

static int
gdpfs_rmdir(const char *filepath)
{
    EP_STAT estat;

    if (strlen(filepath) == 0)
        return -ENOENT;

    estat = gdpfs_dir_remove_file_at_path(filepath, mode);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }

    return 0;
}

static int
gdpfs_rename(const char *filepath1, const char *filepath2)
{
    EP_STAT estat;
    uint64_t fh;
    gdpfs_file_gname_t gname;
    int rv = open_(filepath1, &fh);
    if (rv)
    {
        return -ENOENT;
    }
    gdpfs_file_gname(fh, gname);
    gdpfs_file_close(fh);
    estat = gdpfs_dir_add_file_at_path(gname, filepath2, mode);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }
    estat = gdpfs_dir_remove_file_at_path(filepath1, mode);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }
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

static int
gdpfs_utimens(const char* file, const struct timespec ts[2])
{
    printf("utimens not implemented. File:\"%s\"\n", file);
    return 0;
}

static struct fuse_operations gdpfs_oper = {
    .getattr        = gdpfs_getattr,
    .readdir        = gdpfs_readdir,
    .open           = gdpfs_open,
    .opendir        = gdpfs_opendir,
    .release        = gdpfs_release,
    .releasedir     = gdpfs_releasedir,
    .read           = gdpfs_read,
    .write          = gdpfs_write,
    .truncate       = gdpfs_truncate,
    .ftruncate      = gdpfs_ftruncate,
    .create         = gdpfs_create,
    .unlink         = gdpfs_unlink,
    .mkdir          = gdpfs_mkdir,
    .rmdir          = gdpfs_rmdir,
    .rename         = gdpfs_rename,
    .chmod          = gdpfs_chmod,
    .chown          = gdpfs_chown,
    .utimens        = gdpfs_utimens,
};

int gdpfs_run(char *root_log, bool ro, int fuse_argc, char *fuse_argv[])
{
    EP_STAT estat;
    int ret;

    if (ro)
        mode = GDPFS_FILE_MODE_RO;
    else
        mode = GDPFS_FILE_MODE_RW;

    // need to init file before dir
    estat = init_gdpfs_file();
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);
    estat = init_gdpfs_dir(root_log, mode);
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);

    // TODO: properly close resources after FUSE runs.
    ret = fuse_main(fuse_argc, fuse_argv, &gdpfs_oper, NULL);
    gdpfs_stop();
    return ret;
}

void gdpfs_stop()
{
    stop_gdpfs_dir();
    stop_gdpfs_file();
}
