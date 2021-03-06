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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

/* The Log Daemon to use to create new logs. */
char* logd_xname;

static void
usage(void)
{
    fprintf(stderr,
        "Usage: %s [-hrd] [-G gdp_router] logname servername -- [fuse args]\n"
        "    logname: GDP address of filesystem root directory log\n"
        "    servername: GDP address of log daemon to create new logs on\n"
        "    -h display this usage message and exit\n"
        "    -r mount the filesys in read only mode\n"
        "    -d disable the cache\n"
        "    -G IP host to contact for GDP router\n",
        ep_app_getprogname());
    exit(EX_USAGE);
}

static void
sig_int(int sig)
{
    gdpfs_stop();
}

int
main(int argc, char *argv[])
{
    char *gclpname;
    char *gdp_router_addr = NULL;
    int opt;
    int fuseargc;
    bool read_only = false;
    bool use_cache = true;
    bool show_usage = false;
    char *argv0 = argv[0];

    // we only want to parse gdpfs args, not fuse args. We need to count them.
    for (fuseargc = argc;
         fuseargc > 0 && strcmp(argv[argc - fuseargc], "--") != 0;
         fuseargc--);
    argc -= fuseargc;

    while ((opt = getopt(argc, argv, "G:hrd::")) > 0)
    {
        switch (opt)
        {
        case 'h':
            show_usage = true;
            break;

        case 'r':
            read_only = true;
            break;

        case 'd':
            use_cache = false;
            break;

        case 'G':
            gdp_router_addr = optarg;
            break;

        default:
            show_usage = true;
            break;
        }
    }
    argc -= optind;
    argv += optind;

    gclpname = argv[0];
    logd_xname = argv[1];
    argc -= 2;
    argv += 2;

    if (show_usage || argc != 0)
        usage();

    if (fuseargc > 0)
    {
        argv++;     // consume the --
        fuseargc--; // don't coun't the --
        argc = fuseargc;
    }

    // re-add prog name at argv[0] (fuse needs it).
    argv--;
    argv[0] = argv0;
    argc++;

    signal(SIGINT, sig_int);
    return gdpfs_run(gclpname, gdp_router_addr, read_only, use_cache, argc, argv);
}
