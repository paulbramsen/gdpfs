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

static bool ro_mode = false;
static gdpfs_file_mode_t mode;

static int
gdpfs_getattr(const char *path, struct stat *stbuf)
{
    EP_STAT estat;
    gdpfs_dir_entry_t ent;
    int res;
    uint64_t fh;
    uint64_t dir_fh;

    res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0)
    {
        //stbuf->st_size = gdpfs_file_size(root_fh_);
        stbuf->st_mode = S_IFDIR | 0555;
        //stbuf->st_nlink = 2;
    }
    else
    {
        ent.offset = 0;
        res = -ENOENT;
        // TODO: fix this. Use actual directory.
        dir_fh = gdpfs_dir_open_path(&estat, "/", mode, GDPFS_FILE_TYPE_REGULAR);
        if (!EP_STAT_ISOK(estat))
        {
            ep_app_error("failed to open dir");
        }
        
        while(res != 0)
        {
            estat = gdpfs_dir_read(dir_fh, &ent, ent.offset);
        
            if (EP_STAT_IS_SAME(estat, GDPFS_STAT_EOF))
            {
                break;
            }
            if (!EP_STAT_ISOK(estat))
            {
                ep_app_error("couldn't read root dir");
                break;
            }
            if (strcmp(path + 1, ent.name) == 0)
            {
                fh = gdpfs_dir_open_path(&estat, path, mode, GDPFS_FILE_TYPE_REGULAR);
                if (!EP_STAT_ISOK(estat))
                {
                    ep_app_error("couldn't open %s", path);
                    continue;
                }
                stbuf->st_mode = S_IFREG | (ro_mode ? 0444 : 0644);
                stbuf->st_nlink = 1;
                stbuf->st_size = 12;
                stbuf->st_size = gdpfs_file_size(fh);
                stbuf->st_uid = getuid();
                stbuf->st_gid = getgid();
                res = 0;
                gdpfs_dir_close(fh);
            }
        }
        gdpfs_dir_close(dir_fh);
    }
    return res;
}

static int
open_(const char *path, uint64_t *fh)
{
    EP_STAT estat;

    *fh = gdpfs_dir_open_path(&estat, path, mode, GDPFS_FILE_TYPE_REGULAR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open file at path:\"%s\"", path);
        return -ENOENT;
    }
    printf("open not yet impleented %lu\n", *fh);
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

    fi->fh = gdpfs_dir_open_path(&estat, path, mode, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to opendir at path:\"%s\"", path);
        return -ENOENT;
    }
    printf("opendir %lu\n", fi->fh);
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
    printf("closing dir\n");
    gdpfs_dir_close(fi->fh);
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
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    
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
    printf("buf:%s fh:%lu size:%lu offset:%lu\n", buf, fi->fh, size, offset);
    return gdpfs_file_write(fi->fh, buf, size, offset);
}

static int
gdpfs_truncate(const char *file, off_t file_size)
{
    /*
    int res;
    uint64_t fh;

    if ((res = open_(file, &fh)) != 0)
        return res;
    res = gdpfs_file_ftruncate(fh, file_size);
    res = release_(file, fh) || res;
    return res;
    */
    printf("Truncate not yet implemented\n");
    return 0;
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
    uint64_t fh;
    char *path;
    char *file;
    int i;

    if (filepath[0] != '/')
        return -ENOENT;

    path = ep_mem_zalloc(strlen(filepath) + 1);
    if (!path)
    {
        goto fail0;
    }
    strncpy(path, filepath, strlen(filepath) + 1);
    for (i = strlen(path); i >= 0; i--)
    {
        if (path[i] == '/')
            break;
    }
    file = path + i + 1;
    path[i] = '\0';

    printf("adding %s (trash this print)\n", filepath);
    fh = gdpfs_dir_open_path(&estat, path, mode, GDPFS_FILE_TYPE_DIR);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to open dir at path:\"%s\"", path);
        goto fail0;
    }
    estat = gdpfs_dir_add(fh, file, NULL);
    gdpfs_dir_close(fh);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to add file:\"%s\"", filepath);
        goto fail0;
    }
    fi->fh = gdpfs_file_open_init(&estat, file, mode, GDPFS_FILE_TYPE_REGULAR, true);
    if (!EP_STAT_ISOK(estat))
    {
        ep_app_error("Failed to initialize file:\"%s\"", filepath);
        goto fail0;
    }

    ep_mem_free(path);
    return 0;

fail0:
    ep_mem_free(path);
    return -ENOENT;
}

static int
gdpfs_unlink(const char *file)
{
    printf("Unlink not implemented. File:\"%s\"\n", file);
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
    .chmod          = gdpfs_chmod,
    .chown          = gdpfs_chown,
    .utimens        = gdpfs_utimens,
};

int gdpfs_run(char *root_log, bool ro, int fuse_argc, char *fuse_argv[])
{
    EP_STAT estat;
    int ret;
    

    ro_mode = ro;
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

    // TODO: properly close resources after fuse runs.
    ret = fuse_main(fuse_argc, fuse_argv, &gdpfs_oper, NULL);
    gdpfs_stop();
    return ret;
}

void gdpfs_stop()
{
    stop_gdpfs_dir();
    stop_gdpfs_file();
}