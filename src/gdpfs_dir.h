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

#ifndef _GDPFS_DIR_H_
#define _GDPFS_DIR_H_

#include <ep/ep.h>
#include "gdpfs_file.h"

#define NAME_MAX_GDPFS 255

struct gdpfs_dir_entry
{
    char name[NAME_MAX_GDPFS];
    off_t offset;
};
typedef struct gdpfs_dir_entry gdpfs_dir_entry_t;

/*
 * global dir subsystem intiailization
 */
EP_STAT
init_gdpfs_dir(char *root_log);

void
stop_gdpfs_dir();

/*
 * dir open
 */
uint64_t
gdpfs_dir_open_file_at_path(EP_STAT *ret_stat, const char *path,
		gdpfs_file_type_t type);

/*
 * dir read/modify
 */
EP_STAT
gdpfs_dir_create_file_at_path(uint64_t* fh, const char* filepath,
		gdpfs_file_type_t type, gdpfs_file_gname_t gname_if_exists,
		gdpfs_file_perm_t perm);

EP_STAT
gdpfs_dir_replace_file_at_path(uint64_t fh, const char *filepath2);

EP_STAT
gdpfs_dir_remove_file_at_path(const char *path, gdpfs_file_type_t type);

EP_STAT
gdpfs_dir_remove(uint64_t fh, const char *name, gdpfs_file_type_t type);

EP_STAT
gdpfs_dir_read(uint64_t fh, gdpfs_dir_entry_t *ent, off_t offset);

EP_STAT
gdpfs_dir_isempty(bool* isempty, uint64_t fh);

#endif // _GDPFS_DIR_H_
