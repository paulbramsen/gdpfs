#include "gdpfs_dir.h"

#include <gdp/gdp.h>

#define NAME_MAX2 255

struct gdpfs_dir_entry
{
    gdp_name_t gclname;
    bool in_use;
    char name[NAME_MAX2];
};

typedef struct gdpfs_dir_entry gdpfs_dir_entry_t;


