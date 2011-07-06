bin_PROGRAMS = uxlaunch
include_HEADERS = uxlaunch-ipc.h
uxlaunch_SOURCES = chooser.c consolekit.c dbus.c desktop.c efs.c \
		lib.c misc.c oom_adj.c options.c pam.c user.c \
		uxlaunch.c uxlaunch-ipc.h xserver.c
noinst_HEADERS = uxlaunch.h

uxlaunch_CFLAGS = $(DBUS_CFLAGS) $(CONSOLEKIT_CFLAGS) $(GLIB2_CFLAGS)
uxlaunch_LDADD = $(DBUS_LIBS) $(CONSOLEKIT_LIBS) $(GLIB2_LIBS)
