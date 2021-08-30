VERSION   = 1.3
NAME      = mping
PKG       = $(NAME)-$(VERSION)
ARCHIVE   = $(PKG).tar.gz

prefix   ?= /usr/local
bindir    = $(prefix)/bin
docdir    = $(prefix)/share/doc/$(NAME)
mandir    = $(prefix)/share/man/man1
MAN1      = mping.1
DOCFILES  = README.md LICENSE

CPPFLAGS ?= -W -Wall -Werror -DVERSION='"$(VERSION)"'
CFLAGS   ?= -g -O2 -std=gnu99

all: $(NAME)

check: all
	$(MAKE) -C test $@

clean:
	-$(RM) $(NAME) *.o

install: $(LIBNAME)
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(docdir)
	install -d $(DESTDIR)$(mandir)
	install -m 0755 $(NAME) $(DESTDIR)$(bindir)/$(NAME)
	install -m 0655 $(MAN1) $(DESTDIR)$(mandir)/$(MAN1)
	gzip -f $(DESTDIR)$(mandir)/$(MAN1)
	for file in $(DOCFILES); do					\
		install -m 0644 $$file $(DESTDIR)$(docdir)/$$file;	\
	done

install-strip: install
	strip $(DESTDIR)$(bindir)/$(NAME)

uninstall:
	-$(RM) $(DESTDIR)$(bindir)/$(NAME)
	-$(RM) -r $(DESTDIR)$(docdir)

dist:
	git archive --format=tar.gz --prefix=$(PKG)/ -o ../$(ARCHIVE) v$(VERSION)

distclean: clean
	-$(RM) *~

release: dist
	(cd ..; md5sum $(ARCHIVE) > $(ARCHIVE).md5)
