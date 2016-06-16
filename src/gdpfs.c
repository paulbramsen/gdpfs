/*
**  ----- BEGIN LICENSE BLOCK -----
**  GDPFS: Global Data Plane File System
**  From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**  Copyright (c) 2016, Regents of the University of California.
**  Copyright (c) 2016, Paul Bramsen, Sam Kumar, and Andrew Chen
**  All rights reserved.
**
**  Permission is hereby granted, without written agreement and without
**  license or royalty fees, to use, copy, modify, and distribute this
**  software and its documentation for any purpose, provided that the above
**  copyright notice and the following two paragraphs appear in all copies
**  of this software.
**
**  IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**  SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**  PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**  EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**  REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**  FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**  IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**  OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**  OR MODIFICATIONS.
**  ----- END LICENSE BLOCK -----
*/

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

static gdpfs_file_mode_t fs_mode;

static inline gdpfs_file_perm_t gdpfs_extract_perm(mode_t mode)
{
    return mode & (S_IRWXU | S_IRWXG | S_IRWXO);
}

static int
gdpfs_getattr(const char *path, struct stat *stbuf)
{
    EP_STAT estat;
    gdpfs_file_info_t* info;
    uint64_t fh;

    fh = gdpfs_dir_open_file_at_path(&estat, path, GDPFS_FILE_TYPE_UNKNOWN);
    if (!EP_STAT_ISOK(estat))
        return -ENOENT;
    // TODO: if fh is -1 deal with it.
    estat = gdpfs_file_get_info(&info, fh);
    if (!EP_STAT_ISOK(estat))
        goto fail0;

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_nlink = 1; // TODO: figure out a sane value for this
    stbuf->st_size = info->file_size;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    // set mode
    stbuf->st_mode = 0;
    // handle type
    switch(info->file_type)
    {
        case GDPFS_FILE_TYPE_REGULAR:
            stbuf->st_mode |= S_IFREG;
            break;
        case GDPFS_FILE_TYPE_DIR:
            stbuf->st_mode |= S_IFDIR;
            break;
        default:
            ep_app_error("Invalid file type.");
            goto fail0;
    }

    // handle permissions
    stbuf->st_mode |= info->file_perm;
    switch (fs_mode)
    {
        case GDPFS_FILE_MODE_RO:
            // turn off all write bits
            stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
            break;
        case GDPFS_FILE_MODE_RW:
            // anything goes so just use files stored permissions
            break;
        case GDPFS_FILE_MODE_WO:
            // turn off all read bits
            stbuf->st_mode &= ~(S_IRUSR | S_IRGRP | S_IROTH);
            break;
        default:
            ep_app_error("Bad file mode");
            goto fail0;
    }

    gdpfs_file_close(fh);
    return 0;

fail0:
    gdpfs_file_close(fh);
    return -ENOENT;
}

static bool
_check_open_flags(uint64_t fh, int flags)
{
    EP_STAT estat;
    gdpfs_file_info_t* info;
    bool success = true;

    estat = gdpfs_file_get_info(&info, fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to get file info.");
        return false;
    }

    // TODO: need to check group/user
    flags &= (O_RDONLY | O_WRONLY | O_RDWR);
    if (O_RDONLY == flags)
        success = success && info->file_perm & S_IRUSR;
    else if (O_WRONLY == flags)
        success = success && info->file_perm & S_IWUSR;
    else if (O_RDWR == flags)
    {
        success = success && info->file_perm & S_IRUSR;
        success = success && info->file_perm & S_IWUSR;
    }
    else
    {
        ep_app_error("Opening with invalid access flags.");
        success = false;
    }

    return success;
}

static int
_open(const char *path, uint64_t *fh, gdpfs_file_type_t type)
{
    EP_STAT estat;

    *fh = gdpfs_dir_open_file_at_path(&estat, path, type);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("_open Failed to open file at path:\"%s\"", path);
        return -ENOENT;
    }
    return 0;
}

static int
_release(const char *path, uint64_t fh)
{
    (void)path;
    gdpfs_file_close(fh);
    return 0;
}

static int
gdpfs_open(const char *path, struct fuse_file_info *fi)
{
    if (_open(path, &fi->fh, GDPFS_FILE_TYPE_REGULAR) != 0)
        return -ENOENT;

    if (!_check_open_flags(fi->fh, fi->flags))
    {
        _release(path, fi->fh);
        return -EACCES;
    }
    return 0;
}

static int
gdpfs_opendir(const char *path, struct fuse_file_info *fi)
{
    if (_open(path, &fi->fh, GDPFS_FILE_TYPE_DIR) != 0)
        return -ENOENT;

    if (!_check_open_flags(fi->fh, fi->flags))
    {
        _release(path, fi->fh);
        return -EACCES;
    }

    return 0;
}

static int
gdpfs_release(const char *path, struct fuse_file_info *fi)
{
    return _release(path, fi->fh);
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
    (void)path;
    return gdpfs_file_write(fi->fh, buf, size, offset);
}

static int
gdpfs_truncate(const char *file, off_t file_size)
{
    int res;
    uint64_t fh;

    if ((res = _open(file, &fh, GDPFS_FILE_TYPE_REGULAR)) != 0)
        return res;
    res = gdpfs_file_ftruncate(fh, file_size);
    res = _release(file, fh) || res;
    return res;
}

static int
gdpfs_ftruncate(const char *file, off_t file_size, struct fuse_file_info *fi)
{
    (void)file;
    return gdpfs_file_ftruncate(fi->fh, file_size);
}

static int
gdpfs_create(const char *filepath, mode_t mode, struct fuse_file_info *fi)
{
    EP_STAT estat;
    uint64_t fh;
    gdpfs_log_gname_t existing_logname;

    if (strlen(filepath) == 0)
        return -ENOENT;
    if (filepath[strlen(filepath) - 1] == '/')
        return -EISDIR;

    estat = gdpfs_dir_create_file_at_path(&fh, filepath, GDPFS_FILE_TYPE_REGULAR,
            existing_logname, gdpfs_extract_perm(mode));
    if (EP_STAT_IS_SAME(estat, GDPFS_STAT_FILE_EXISTS))
    {
        printf("Opening existing file\n");
        fh = gdpfs_file_open_type(&estat, existing_logname, GDPFS_FILE_TYPE_REGULAR);
    }

    if (!EP_STAT_ISOK(estat))
        return -ENOENT;
    fi->fh = fh;
    return 0;
}

static int
gdpfs_unlink(const char *filepath)
{
    EP_STAT estat;

    if (strlen(filepath) == 0)
        return -ENOENT;

    estat = gdpfs_dir_remove_file_at_path(filepath, GDPFS_FILE_TYPE_REGULAR);
    if (!EP_STAT_ISOK(estat))
    {
        return -ENOENT;
    }

    return 0;
}

static int
gdpfs_mkdir(const char *filepath, mode_t mode)
{
    EP_STAT estat;
    uint64_t fh;

    if (strlen(filepath) == 0)
        return -ENOENT;

    estat = gdpfs_dir_create_file_at_path(&fh, filepath, GDPFS_FILE_TYPE_DIR, NULL, gdpfs_extract_perm(mode));

    if (!EP_STAT_ISOK(estat))
        return -ENOENT;
    return 0;
}

static int
gdpfs_rmdir(const char *filepath)
{
    EP_STAT estat;

    if (strlen(filepath) == 0)
        return -ENOENT;

    estat = gdpfs_dir_remove_file_at_path(filepath, GDPFS_FILE_TYPE_DIR);
    if (EP_STAT_IS_SAME(estat, GDPFS_STAT_DIR_NOT_EMPTY))
    {
        return -ENOTEMPTY;
    }
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
    int rv = _open(filepath1, &fh, GDPFS_FILE_TYPE_UNKNOWN);
    if (rv)
    {
        return -ENOENT;
    }

    estat = gdpfs_dir_replace_file_at_path(fh, filepath2);
    gdpfs_file_close(fh);

    if (EP_STAT_IS_SAME(estat, GDPFS_STAT_FILE_EXISTS))
        return -ENOTDIR;
    else if (EP_STAT_IS_SAME(estat, GDPFS_STAT_DIR_EXISTS))
        return -EISDIR;
    else if (EP_STAT_IS_SAME(estat, GDPFS_STAT_DIR_NOT_EMPTY))
        return -ENOTEMPTY;
    else if (!EP_STAT_ISOK(estat))
        return -ENOENT;

    estat = gdpfs_dir_remove_file_at_path(filepath1, GDPFS_FILE_TYPE_UNKNOWN);

    if (!EP_STAT_ISOK(estat))
        return -ENOENT;

    return 0;
}

static int
gdpfs_access(const char *filepath, int mode)
{
    EP_STAT estat;
    uint64_t fh;
    gdpfs_file_info_t* info;
    bool success = true;

    // _open should fail if file doesn't exist
    if (_open(filepath, &fh, GDPFS_FILE_TYPE_UNKNOWN) != 0)
        return -1;
    estat = gdpfs_file_get_info(&info, fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to get file info.");
        success = false;
        goto fail0;
    }
    // TODO: need to check group/user
    if (R_OK & mode)
        success = success && info->file_perm & S_IRUSR;
    if (W_OK & mode)
        success = success && info->file_perm & S_IWUSR;
    if (X_OK & mode)
        success = success && info->file_perm & S_IXUSR;
fail0:
    _release(filepath, fh);
    if (success)
        return 0;
    return -1;
}

static int
gdpfs_chmod (const char *filepath, mode_t mode)
{
    EP_STAT estat;
    uint64_t fh;

    if (_open(filepath, &fh, GDPFS_FILE_TYPE_UNKNOWN) != 0)
        return -ENOENT;
    estat = gdpfs_file_set_perm(fh, gdpfs_extract_perm(mode));
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to set permissions");
    }
    _release(filepath, fh);
    return 0;
}

static int
gdpfs_chown (const char *filepath, uid_t uid, gid_t gid)
{
    ep_app_warn("Cannont change owner of file \"%s\" in mounted GDPFS.\n"
                "All files are owned by the mounter.", filepath);
    return 0;
}

static int
gdpfs_utimens(const char* filepath, const struct timespec ts[2])
{
    //ep_app_warn("utimens not implemented. File:\"%s\"", filepath);
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
    .access         = gdpfs_access,
    .utimens        = gdpfs_utimens,
};

int gdpfs_run(char *root_log, char *gdp_router_addr, bool ro, bool use_cache, int fuse_argc, char *fuse_argv[])
{
    EP_STAT estat;
    int ret;

    if (ro)
        fs_mode = GDPFS_FILE_MODE_RO;
    else
        fs_mode = GDPFS_FILE_MODE_RW;

    // need to init file before dir
    estat = init_gdpfs_file(fs_mode, use_cache, gdp_router_addr);
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);
    estat = init_gdpfs_dir(root_log);
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
