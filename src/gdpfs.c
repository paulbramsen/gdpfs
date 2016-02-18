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

static char *log_path;
static bool ro_mode = false;
static uint64_t root_fh;

static int
gdpfs_getattr(const char *path, struct stat *stbuf)
{
    EP_STAT estat;
    gdpfs_dir_entry_t ent;
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
    }
    else
    {
        ent.offset = 0;
        res = -ENOENT;
        while(res != 0)
        {
            estat = gdpfs_dir_read(root_fh, &ent, ent.offset);
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
                stbuf->st_mode = S_IFREG | (ro_mode ? 0444 : 0644);
                stbuf->st_nlink = 1;
                stbuf->st_size = gdpfs_file_size(root_fh);
                stbuf->st_uid = getuid();
                stbuf->st_gid = getgid();
                res = 0;
            }
        }
    }
    return res;
}

static int
gdpfs_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, log_path) != 0)
        return -ENOENT;
    printf("open not yet impleented\n");
    return 0;
}

static int
gdpfs_opendir(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    fi->fh = root_fh;
    printf("opendir\n");
    return 0;
}

static int
gdpfs_release(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, log_path) != 0)
        return -ENOENT;
    printf("closing\n");
    return 0;
}

static int
gdpfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, log_path) != 0)
        return -ENOENT;
    printf("closing dir\n");
    return 0;
}

static int
gdpfs_read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi)
{
    (void) fi;
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
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
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_write(fi->fh, buf, size, offset);
}

static int
gdpfs_truncate(const char *path, off_t file_size)
{
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_ftruncate(root_fh, file_size);
}

static int
gdpfs_ftruncate(const char *path, off_t file_size, struct fuse_file_info *fi)
{
    (void)fi;
    if(strcmp(path, log_path) != 0)
        return -ENOENT;
    return gdpfs_file_ftruncate(fi->fh, file_size);
}

static int
gdpfs_create(const char *file, mode_t mode, struct fuse_file_info *info)
{
    EP_STAT estat;
    printf("adding %s (trash this print)\n", file);
    estat = gdpfs_dir_add(root_fh, file + 1, NULL);
    // TODO: open the file
    if (EP_STAT_ISOK(estat))
        return 0;
    else
        return -ENOENT;
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
    .mkdir          = gdpfs_mkdir,
    .chmod          = gdpfs_chmod,
    .chown          = gdpfs_chown,
    .utimens        = gdpfs_utimens,
};

int gdpfs_run(char *root_log, bool ro, int fuse_argc, char *fuse_argv[])
{
    EP_STAT estat;
    int ret;
    gdpfs_file_mode_t mode;

    ro_mode = ro;
    if (ro)
        mode = GDPFS_FILE_MODE_RO;
    else
        mode = GDPFS_FILE_MODE_RW;
    
    // TODO: trash this
    log_path = malloc(sizeof(char) * (2 + strlen(root_log)));
    log_path[0] = '/';
    strcpy(log_path + 1, root_log);

    estat = init_gdpfs_file();
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);
    /*
    estat = init_gdpfs_dir(root_log, mode);
    if (!EP_STAT_ISOK(estat))
        exit(EX_UNAVAILABLE);
    */
    root_fh = gdpfs_file_open_init(&estat, root_log, mode, GDPFS_FILE_TYPE_DIR, false);
    if (!EP_STAT_ISOK(estat) || root_fh == (uint64_t)-1)
    {
        ep_app_error("unable to open the root");
        exit(EX_UNAVAILABLE);
    }

    // TODO: properly close resources after fuse runs.
    ret = fuse_main(fuse_argc, fuse_argv, &gdpfs_oper, NULL);
    gdpfs_stop();
    return ret;
}

void gdpfs_stop()
{
    gdpfs_file_close(root_fh);
    free(log_path);
}