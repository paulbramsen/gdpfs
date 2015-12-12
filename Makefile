CC=gcc
DEBUG=
FUSE_LIBS=`pkg-config fuse --cflags --libs`
CFLAGS=-c -Wall $(FUSE_LIBS)
LFLAGS=-Wall $(FUSE_LIBS)

BINDIR=bin
BUILDDIR=$(BINDIR)/build

_OBJ=gdpfs.o
OBJ=$(patsubst %,$(BUILDDIR)/%,$(_OBJ))

all: $(BINDIR)/gdpfs

$(BUILDDIR)/%.o: %.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR)/gdpfs: $(OBJ)
	mkdir -p $(BINDIR)
	gcc -o $@ $^ $(LFLAGS)

.PHONY clean:
	rm -rf $(BINDIR)
	rm -rf $(BUILDDIR)
