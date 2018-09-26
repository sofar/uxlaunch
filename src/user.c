/*
 * This file is part of uxlaunch
 *
 * (C) Copyright 2009 Intel Corporation
 * Authors:
 *     Auke Kok <auke@linux.intel.com>
 *     Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>


#include "uxlaunch.h"

#include <X11/Xauth.h>

static int uid;
struct passwd *pass;

char user_xauth_path[PATH_MAX];

static void do_env(void)
{
	char buf[PATH_MAX];
	FILE *file;

	d_in();

	/* start with a clean environ */
	clearenv();

	setenv("USER", pass->pw_name, 1);
	setenv("LOGNAME", pass->pw_name, 1);
	setenv("HOME", pass->pw_dir, 1);
	setenv("SHELL", pass->pw_shell, 1);
	snprintf(buf, PATH_MAX, "/var/spool/mail/%s", pass->pw_name);
	setenv("MAIL", buf, 1);
	setenv("DISPLAY", displayname, 1);
	snprintf(buf, PATH_MAX, "/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin:%s/bin", pass->pw_dir);
	setenv("PATH", buf, 1);
	snprintf(user_xauth_path, PATH_MAX, "%s/.Xauthority", pass->pw_dir);
	setenv("XAUTHORITY", user_xauth_path, 1);

	file = popen("/bin/bash -l -c export", "r");
	if (!file)
		return;

	while (!feof(file)) {
		char *c;
		memset(buf, 0, sizeof(buf));
		if (fgets(buf, sizeof(buf) - 1, file) == NULL)
				break;
		c = strchr(buf, '\n');

		if (strlen(buf) < 12)
			continue;
		if (c)
			*c = 0;

		if (strstr(buf, "PWD"))
			continue;
//		if (strstr(buf, "DISPLAY"))
//			continue;

		c = strchr(buf, '=');
		if (c) {
			char *c2;
			*c = 0;
			c++;
			if (*c == '"') c++;
			c2 = strchr(c, '"');
			if (c2)
				*c2 = 0;
			dprintf("Setting %s to %s\n", &buf[11], c);
			setenv(&buf[11], c, 1);
		}

	}

	pclose(file);
	d_out();
}

#define BACKLIGHT_CLASS "/sys/class/backlight"
#define BACKLIGHT_FILE "brightness"

static void set_backlight_driver_perms(const char *backlight_dir_path)
{
	char backlight_file_path[PATH_MAX];
	int ret;

	d_in();

	snprintf(backlight_file_path, sizeof(backlight_file_path),
		 "%s/%s", backlight_dir_path, BACKLIGHT_FILE);

	ret = chown(backlight_dir_path, pass->pw_uid, pass->pw_gid);
	if (ret)
		lprintf("Failed to set \"%s\" ownership", backlight_dir_path);

	ret = chown(backlight_file_path, pass->pw_uid, pass->pw_gid);
	if (ret)
		lprintf("Failed to set \"%s\" ownership", backlight_file_path);

	d_out();
}

static void set_backlight_perms(const char *backlight_class)
{
	DIR *dir;
	struct dirent *entry;
	char backlight_dir_path[PATH_MAX];

	d_in();

	dir = opendir(backlight_class);
	if (dir) {
		while (NULL != (entry = readdir (dir))) {
			if (entry->d_name[0] != '.' &&
			    entry->d_type == DT_LNK) {

				snprintf(backlight_dir_path,
					 sizeof (backlight_dir_path),
					 "%s/%s", backlight_class,
					  entry->d_name);
				set_backlight_driver_perms(backlight_dir_path);
			}
		}
		closedir (dir);
	} else {
		lprintf ("Failed to opendir(\"%s\")", backlight_class);
	}

	d_out();
}

/*
 * Change from root (as we started) to the target user.
 * Steps
 * 1) setuid/getgid
 * 2) env variables: HOME, MAIL, LOGNAME, USER, SHELL, DISPLAY and PATH
 * 3) chdir(/home/foo);
 */
void switch_to_user(void)
{
	FILE *fp;
	char fn[PATH_MAX];
	int ret;

	d_in();

	initgroups(pass->pw_name, pass->pw_gid);

	/* make sure that the user owns /dev/ttyX */
	ret = chown(displaydev, pass->pw_uid, pass->pw_gid);
	if (ret)
		lprintf("Failed to fix /dev/tty permission");

	/* make sure the user owns the X backlight devices */
	set_backlight_perms (BACKLIGHT_CLASS);

	if (!((setgid(pass->pw_gid) == 0) && (setuid(pass->pw_uid) == 0))) {
		lprintf("Fatal: Unable to setgid()/setuid()\n");
		exit(EXIT_FAILURE);
	}

	if (access(pass->pw_dir, R_OK || W_OK || X_OK) != 0) {
		lprintf("Fatal: \"%s\" has incompatible permissions", pass->pw_dir);
		exit(EXIT_FAILURE);
	}

	/* This should fail, so, only print out info when it succeeded */
	ret = setpgid(0, getpgid(getppid()));
	if (ret != -1)
		lprintf("setpgid() returned %d", ret);
	ret = setsid();
	if (ret != -1)
		lprintf("setsid returned %d", ret);

	do_env();

	set_i18n();

	ret = chdir(pass->pw_dir);

	fp = fopen(user_xauth_path, "w");
	if (fp) {
		if (XauWriteAuth(fp, &x_auth) != 1)
			lprintf("Unable to write .Xauthority");
		fclose(fp);
	}

	/* redirect further IO to .xsession-errors */
	snprintf(fn, PATH_MAX, "%s/.xsession-errors", pass->pw_dir);
	fp = fopen(fn, "w");
	if (fp) {
		fclose(fp);
		/* xserver.c already truncates this file, so append */
		fp = freopen(fn, "a", stdout);
		fp = freopen(fn, "a", stderr);
	} else {
		lprintf("Unable to open \"%s\n\" for writing", fn);
	}

	d_out();
}

static char *scim_languages[] = { "zh_", "ja_", "ko_", "lo_", "th_" };

void setup_user_environment (void)
{
	unsigned int i;
	char buf[PATH_MAX];
	const char *lang = getenv ("LANG");

	d_in();

	for (i = 0; lang && i < sizeof(scim_languages) / sizeof(scim_languages[0]); i++) {
		if (strstr(lang, scim_languages[i])) {
			setenv("GTK_IM_MODULE", "scim-bridge", 0);
			setenv("CLUTTER_IM_MODULE","scim-bridge", 0);
		}
	}

	/* setup misc. user directories and variables */
	snprintf(buf, PATH_MAX, "%s/.cache", pass->pw_dir);
	mkdir(buf, 0700);
	setenv("XDG_CACHE_HOME", buf, 0);
	snprintf(buf, PATH_MAX, "%s/.config", pass->pw_dir);
	setenv("XDG_CONFIG_HOME", buf, 0);
	setenv("OOO_FORCE_DESKTOP","gnome", 0);
	setenv("LIBC_FATAL_STDERR_", "1", 0);

	d_out();
}

void set_i18n(void)
{
	FILE *f;
	char path[PATH_MAX];
	char buf[256];
	char *key;
	char *val;

	d_in();

	/*
	 * /etc/sysconfig/i18n contains shell code that sets
	 * various i18n options in environment, typically:
	 * LANG, SYSFONT
	 */
	snprintf(path, PATH_MAX, "%s/.config/i18n", pass->pw_dir);
	f = fopen(path, "r");
	if (f)
		goto parse;
	dprintf("Unable to open ~/.config/i18n, trying /etc/sysconfig/i18n");
	f = fopen("/etc/sysconfig/i18n", "r");
	if (f)
		goto parse;
	d_out();
	return;

parse:
	while (fgets(buf, 256, f) != NULL) {
		char *c;

		c = strchr(buf, '\n');
		if (c) *c = 0; /* remove trailing \n */
		if (buf[0] == '#')
			continue; /* skip comments */

		key = strtok(buf, "=");
		if (!key)
			continue;
		val = strtok(NULL, "=\""); /* note \" */
		if (!val)
			continue;

		/* grab the stuff we need, avoiding comments
		 * and other user stuff we don't care for now */
		if (!strcmp(key, "LANG"))
			setenv(key, val, 1);
		if (!strcmp(key, "SYSFONT"))
			setenv(key, val, 1);
	}
	fclose(f);

	d_out();
}
