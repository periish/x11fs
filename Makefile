CC?=cc
LD=$(CC)

LIBS=`pkg-config fuse3 --libs` `pkg-config xcb --libs` `pkg-config xcb-icccm --libs`
CFLAGS=-Wall -std=gnu11 -c `pkg-config fuse3 --cflags` `pkg-config xcb --cflags`

PREFIX?=/usr/local

SRCDIR=src
OBJDIR=obj
BINDIR=bin

SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: x11fs

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(wildcard $(SRCDIR)/*.h) $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<

x11fs: $(BINDIR) $(OBJ)
	$(LD) $(OBJ) $(LIBS) -o $(BINDIR)/$@

install: x11fs
	mkdir -p $(DESTDIR)$(PREFIX)/bin/
	cp $(BINDIR)/x11fs $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/x11fs

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(BINDIR)/x11fs
