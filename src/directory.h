#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

#define NAME_MAX 20

typedef struct
{
    char name[NAME_MAX + 1];
    int inode_rec;
    bool in_use;
} gdpfs_dir_ent;

#endif // _DIRECTORY_H_
