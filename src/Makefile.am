bin_PROGRAMS = uxlaunch
uxlaunch_SOURCES = dbus.c desktop.c lib.c misc.c oom_adj.c options.c pam.c \
		user.c uxlaunch.c xserver.c

uxlaunch_CFLAGS = $(DBUS_CFLAGS) $(GLIB2_CFLAGS)
uxlaunch_LDADD = $(DBUS_LIBS) $(GLIB2_LIBS)

if WITH_CONSOLEKIT
uxlaunch_SOURCES += consolekit.c
uxlaunch_CFLAGS += $(CONSOLEKIT_CFLAGS)
uxlaunch_LDADD += $(CONSOLEKIT_LIBS)
endif

if ENABLE_CHOOSER
include_HEADERS = uxlaunch-ipc.h
uxlaunch_SOURCES += chooser.c uxlaunch-ipc.h
endif

if ENABLE_ECRYPTFS
uxlaunch_SOURCES += efs.c
endif

noinst_HEADERS = uxlaunch.h

