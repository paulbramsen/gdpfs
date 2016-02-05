#include "gdpfs.h"

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <sysexits.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

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
sig_int(int sig)
{
    gdpfs_stop();
}

int
main(int argc, char *argv[])
{
    char *gclpname;
    int opt;
    int fuseargc;
    bool read_only = false;
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

    gclpname = argv[0];
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

    // re-add prog name at argv[0] (fuse needs it).
    argv--;
    argv[0] = argv0;
    argc++;

    signal(SIGINT, sig_int);
    return gdpfs_run(gclpname, read_only, argc, argv);
}