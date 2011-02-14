VERSION = 0.59

CC := gcc

all: uxlaunch uxlaunch.1.gz

uxlaunch.1.gz: uxlaunch.1
	gzip -c uxlaunch.1 > uxlaunch.1.gz

install: uxlaunch
	mkdir -p $(DESTDIR)/usr/sbin \
	         $(DESTDIR)/etc/sysconfig/ \
	         $(DESTDIR)/usr/share/man/man1/ \
	         $(DESTDIR)/usr/share/uxlaunch
	install -m0755  uxlaunch $(DESTDIR)/usr/sbin/
	[ -f $(DESTDIR)/etc/sysconfig/uxlaunch ] || \
	    install -m0644 uxlaunch.sysconfig $(DESTDIR)/etc/sysconfig/uxlaunch
	install -m0644 uxlaunch.1.gz $(DESTDIR)/usr/share/man/man1/uxlaunch.1.gz
	install -m0644 dmi-dpi $(DESTDIR)/usr/share/uxlaunch/

OBJS := uxlaunch.o consolekit.o dbus.o desktop.o misc.o pam.o user.o xserver.o \
	lib.o options.o oom_adj.o efs.o

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
	@[ ! -x /usr/bin/cppcheck ] || /usr/bin/cppcheck -q $<
	@$(CC) $(CFLAGS) -c -o $@ $<

uxlaunch: $(OBJS) Makefile
	@echo "  LD  $@"
	@$(CC) -o $@ $(OBJS) $(LDADD) $(LDFLAGS)

clean:
	rm -rf *.o *~ uxlaunch uxlaunch.1.gz

dist:
	git tag v$(VERSION)
	git archive --format=tar --prefix="uxlaunch-$(VERSION)/" v$(VERSION) | \
		gzip > uxlaunch-$(VERSION).tar.gz
