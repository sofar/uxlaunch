VERSION = 0.22.2

CC := gcc

all: uxlaunch

install: uxlaunch
	mkdir -p $(DESTDIR)/usr/sbin \
	         $(DESTDIR)/etc/sysconfig/ \
	         $(DESTDIR)/usr/share/man/man1/
	install uxlaunch $(DESTDIR)/usr/sbin/
	[ -f $(DESTDIR)/etc/sysconfig/uxlaunch ] || \
	    install uxlaunch.sysconfig $(DESTDIR)/etc/sysconfig/uxlaunch
	install uxlaunch.1 $(DESTDIR)/usr/share/man/man1/uxlaunch.1

OBJS := uxlaunch.o consolekit.o dbus.o desktop.o misc.o pam.o user.o xserver.o \
	lib.o options.o

CFLAGS += -Wall -W -Os -g -fstack-protector -D_FORTIFY_SOURCE=2 -Wformat -fno-common \
	 -Wimplicit-function-declaration  -Wimplicit-int \
	`pkg-config --cflags dbus-1` \
	`pkg-config --cflags ck-connector` \
	`pkg-config --cflags glib-2.0` \
	-D VERSION=\"$(VERSION)\"

LDADD  += `pkg-config --libs dbus-1` \
	  `pkg-config --libs ck-connector` \
	  `pkg-config --libs glib-2.0` \
	  -lpam -lpthread -lrt -lXau

%.o: %.c uxlaunch.h Makefile
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

uxlaunch: $(OBJS) Makefile
	@echo "  LD  $@"
	@$(CC) -o $@ $(OBJS) $(LDADD) $(LDFLAGS)

clean:
	rm -rf *.o *~ uxlaunch

dist:
	git tag v$(VERSION)
	git archive --format=tar --prefix="uxlaunch-$(VERSION)/" v$(VERSION) | \
		gzip > uxlaunch-$(VERSION).tar.gz
