CC=gcc
WALL=-Wall

export C_INCLUDE_PATH=lib/

DEBUG=-g -fvar-tracking -O0
FUSE_LIBS=`pkg-config fuse --cflags --libs`
CFLAGS=$(WALL) $(FUSE_LIBS) $(DEBUG)

LIBGDP=	-lgdp
LIBEP=	-lep
LFLAGS=	-Wall $(FUSE_LIBS) $(LIBGDP) $(LIBEP) $(DEBUG)

SRCDIR=src
BINDIR=bin
BUILDDIR=$(BINDIR)/build

_OBJ=block_cache.o directory.o gdpfs.o inode.o main.o
OBJ=$(patsubst %,$(BUILDDIR)/%,$(_OBJ))

_OBJOLD=old/gdpfs.o old/gdpfs_file.o old/gdpfs_log.o old/gdpfs_dir.o
OBJOLD=$(patsubst %,$(BUILDDIR)/%,$(_OBJOLD))

all: $(BINDIR)/gdpfs

$(BINDIR)/gdpfs: $(OBJ)
	mkdir -p $(@D)
	gcc -o $@ $^ $(LFLAGS)

old: $(BINDIR)/gdpfsold

$(BINDIR)/gdpfsold: $(OBJOLD)
	mkdir -p $(@D)
	gcc -o $@ $^ $(LFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY clean:
	rm -rf $(BINDIR)
	rm -rf $(BUILDDIR)
