CC=gcc
WALL=-Wall

export C_INCLUDE_PATH=lib/

DEBUG=-g -fvar-tracking -O0 -lprofiler
FUSE_LIBS=`pkg-config fuse --cflags --libs`
CFLAGS=$(WALL) $(FUSE_LIBS) $(DEBUG)

LIBGDP=	-lgdp
LIBEP=	-lep
LFLAGS=	-Wall $(FUSE_LIBS) $(LIBGDP) $(LIBEP) $(DEBUG)

SRCDIR=src
BINDIR=bin
BINOUT=gdpfs
BUILDDIR=$(BINDIR)/build

_OBJ=gdpfs.o gdpfs_file.o gdpfs_log.o gdpfs_dir.o main.o bitmap.o bitmap_file.o
OBJ=$(patsubst %,$(BUILDDIR)/%,$(_OBJ))

all: $(BINDIR)/$(BINOUT)

$(BINDIR)/$(BINOUT): $(OBJ)
	mkdir -p $(@D)
	gcc -o $@ $^ $(LFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY clean:
	rm -rf $(BINDIR)
	rm -rf $(BUILDDIR)
