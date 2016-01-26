#ifndef _GDPFS_PRIV_H_
#define _GDPFS_PRIV_H_

struct gdpfs_ent
{
    size_t file_size;
    off_t ent_offset;
    size_t ent_size;
};

typedef struct gdpfs_ent gdpfs_ent_t;

#endif // _GDPFS_PRIV_H_
