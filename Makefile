CC=gcc
WALL=-Wall

DEBUG=-g -fvar-tracking
FUSE_LIBS=`pkg-config fuse --cflags --libs`
CFLAGS=$(WALL) $(FUSE_LIBS) $(DEBUG)

LIBGDP=	-lgdp
LIBEP=	-lep
LFLAGS=	-Wall $(FUSE_LIBS) $(LIBGDP) $(LIBEP) $(DEBUG)

BINDIR=bin
BUILDDIR=$(BINDIR)/build

_OBJ=gdpfs.o
OBJ=$(patsubst %,$(BUILDDIR)/%,$(_OBJ))

all: $(BINDIR)/gdpfs

$(BUILDDIR)/%.o: %.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/gdpfs: $(OBJ)
	mkdir -p $(BINDIR)
	gcc -o $@ $^ $(LFLAGS)

.PHONY clean:
	rm -rf $(BINDIR)
	rm -rf $(BUILDDIR)
