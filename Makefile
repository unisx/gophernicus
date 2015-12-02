NAME    = gophernicus
PACKAGE = $(NAME)-server
BINARY  = in.$(NAME)
VERSION = 0.5

SOURCES = $(NAME).c file.c menu.c string.c platform.c session.c
HEADERS = functions.h readme.h license.h error_gif.h
OBJECTS = $(SOURCES:.c=.o)
DOCS    = LICENSE README TODO ChangeLog GOPHERMAP

DESTDIR = /usr
SBINDIR = $(DESTDIR)/sbin
DOCDIR  = $(DESTDIR)/share/doc/$(PACKAGE)

DIST    = $(PACKAGE)-$(VERSION)
TGZ     = $(DIST).tar.gz
RELDIR  = /var/gopher/gophernicus.org/software/gophernicus/server/

CC      = gcc
CFLAGS  = -O2 -Wall -DVERSION="\"$(VERSION)\"" -DBINARY="\"$(BINARY)\""
LDFLAGS =


all: $(SOURCES) $(BINARY)

$(NAME).c: $(NAME).h $(HEADERS)
	
$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

headers: $(HEADERS)

functions.h:
	echo "/* Automatically generated function definitions */" > functions.h
	echo >> functions.h
	grep -h "^[a-z]" *.c | grep -v "int main" | sed -e "s/ =.*$$//" -e "s/ *$$/;/" >> functions.h

readme.h:
	./t2h.sh README > readme.h

license.h:
	./t2h.sh LICENSE > license.h

error_gif.h:
	./bin2h.sh error.gif ERROR_GIF > error_gif.h

clean:
	rm -f $(BINARY) $(OBJECTS) $(TGZ)

realclean: clean
	rm -f $(HEADERS)

dist: realclean $(HEADERS)
	mkdir -p /tmp/$(DIST)
	tar -cf - ./ | (cd /tmp/$(DIST) && tar -xf -)
	(cd /tmp/ && tar -cvf - $(DIST)) | gzip > $(TGZ)
	rm -rf /tmp/$(DIST)

release: dist
	cp $(TGZ) $(RELDIR)

install: $(BINARY)
	strip $(BINARY)
	mkdir -p $(SBINDIR) $(DOCDIR)
	install -m 755 $(BINARY) $(SBINDIR)
	install -m 644 $(DOCS) $(DOCDIR)

uninstall:
	rm -f $(SBINDIR)/$(BINARY)
	(cd $(DOCDIR) && rm -f $(DOCS))
	-rmdir -p $(SBINDIR) $(DOCDIR) 2>/dev/null

defines:
	$(CC) -dM -E $(NAME).c

