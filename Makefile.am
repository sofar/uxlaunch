SUBDIRS = src

man_MANS = uxlaunch.1.gz

EXTRA_DIST = AUTHORS COPYING INSTALL uxlaunch.sysconfig dmi-dpi uxlaunch.1

uxlaunch.1.gz: uxlaunch.1
	gzip < $< > $@

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)/sysconfig
	$(install_sh_DATA) uxlaunch.sysconfig $(DESTDIR)$(sysconfdir)/sysconfig/uxlaunch
	$(MKDIR_P) $(DESTDIR)$(datadir)/uxlaunch
	$(install_sh_DATA) dmi-dpi $(DESTDIR)$(datadir)/uxlaunch/dmi-dpi
