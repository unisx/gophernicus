NAME    = gophernicus
PACKAGE = $(NAME)-server
BINARY  = in.$(NAME)
VERSION = 0.6

SOURCES = $(NAME).c file.c menu.c string.c platform.c session.c
HEADERS = functions.h files.h
OBJECTS = $(SOURCES:.c=.o)
DOCS    = LICENSE README INSTALL TODO ChangeLog GOPHERMAP

INSTALL = ./install-sh -o root -g 0
DESTDIR = /usr
SBINDIR = $(DESTDIR)/sbin
DOCDIR  = $(DESTDIR)/share/doc/$(PACKAGE)

DIST    = $(PACKAGE)-$(VERSION)
TGZ     = $(DIST).tar.gz
RELDIR  = /var/gopher/gophernicus.org/software/gophernicus/server/

CC      = gcc
CFLAGS  = -O2 -Wall -DVERSION="\"$(VERSION)\""
LDFLAGS =


all: $(SOURCES) $(BINARY)

$(NAME).c: $(NAME).h $(HEADERS)
	
$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@


headers: $(HEADERS)

functions.h:
	echo "/* Automatically generated function definitions */" > $@
	echo >> $@
	grep -h "^[a-z]" *.c | grep -v "int main" | sed -e "s/ =.*$$//" -e "s/ *$$/;/" >> $@

bin2c: bin2c.c
	$(CC) $< -o $@

files.h: bin2c
	./bin2c README > $@
	./bin2c LICENSE >> $@
	./bin2c error.gif ERROR_GIF >> $@


clean:
	rm -f $(BINARY) $(OBJECTS) $(TGZ) $(HEADERS) bin2c


install: $(BINARY)
	mkdir -p $(SBINDIR) $(DOCDIR)
	$(INSTALL) -s -m 755 $(BINARY) $(SBINDIR)
	$(INSTALL) -m 644 $(DOCS) $(DOCDIR)

uninstall:
	rm -f $(SBINDIR)/$(BINARY)
	(cd $(DOCDIR) && rm -f $(DOCS))
	-rmdir -p $(SBINDIR) $(DOCDIR) 2>/dev/null


dist: clean functions.h
	mkdir -p /tmp/$(DIST)
	tar -cf - ./ | (cd /tmp/$(DIST) && tar -xf -)
	(cd /tmp/ && tar -cvf - $(DIST)) | gzip > $(TGZ)
	rm -rf /tmp/$(DIST)

release: dist
	cp $(TGZ) $(RELDIR)


defines:
	$(CC) -dM -E $(NAME).c

