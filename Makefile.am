SUBDIRS = src

man_MANS = uxlaunch.1

noinst_DATA = uxlaunch.sysconfig dmi-dpi

EXTRA_DIST = AUTHORS COPYING INSTALL $(noinst_DATA) $(man_MANS)

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)/sysconfig
	$(install_sh_DATA) uxlaunch.sysconfig $(DESTDIR)$(sysconfdir)/sysconfig/uxlaunch
	$(MKDIR_P) $(DESTDIR)$(datadir)/uxlaunch
	$(install_sh_DATA) dmi-dpi $(DESTDIR)$(datadir)/uxlaunch/dmi-dpi
