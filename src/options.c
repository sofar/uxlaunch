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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <dirent.h>
#include <pwd.h>

#include "uxlaunch.h"

#include "../config.h"

/* builtin defaults */
int tty = 1;
#ifdef ENABLE_CHOOSER
char chooser[256] = "";
#endif
char session[256] = "default";
char username[256] = DEFAULT_USERNAME;
char dpinum[256] = "auto";
char addn_xopts[256] = "";

int verbose = 0;
int x_session_only = 0;
int settle = 0;

static struct option opts[] = {
#ifdef ENABLE_CHOOSER
	{ "chooser",  1, NULL, 'c' },
#endif
	{ "user",     1, NULL, 'u' },
	{ "tty",      1, NULL, 't' },
	{ "session",  1, NULL, 's' },
	{ "xsession", 0, NULL, 'x' },
	{ "settle",   0, NULL, 'S' },
	{ "help",     0, NULL, 'h' },
	{ "verbose",  0, NULL, 'v' },
	{ NULL, 0, NULL, 0 }
};


static void usage(const char *name)
{
	printf("Usage: %s [OPTION...] [-- [session cmd] [session args]]\n", name);
#ifdef ENABLE_CHOOSER
	printf("  -c, --chooser   Start specified UX chooser\n");
#endif
	printf("  -u, --user      Start session as specific username\n");
	printf("  -t, --tty       Start session on alternative tty number\n");
	printf("  -s, --session   Start a non-default session\n");
	printf("  -x, --xsession  Start X apps inside an existing X session\n");
	printf("  -S, --settle    Wait for udev to settle\n");
	printf("  -n, --nosettle  Do not wait for udev to settle\n");
	printf("  -v, --verbose   Display lots of output to the console\n");
	printf("  -h, --help      Display this help message\n");
}

static void get_dmi_dpi(void)
{
	/* see if we have a dmi-based override dpi value for
	 *  this board */
	FILE *f;
	char boardname[PATH_MAX];

	d_in();

	f = fopen("/etc/boardname", "r");
	if (!f) {
		lprintf("Unable to open /etc/boardname");
		return;
	}
	if (fscanf(f, "%s", boardname) <= 0) {
		lprintf("Unable to read /etc/boardname");
		fclose(f);
		return;
	}
	fclose(f);
	dprintf("boardname=%s", boardname);

	f = fopen("/usr/share/uxlaunch/dmi-dpi", "r");
	if (!f) {
		lprintf("No DMI-DPI table present (/usr/share/uxlaunch/dmi-dpi)");
		return;
	}
	while (!feof(f)) {
		char b[PATH_MAX];
		char dpi[PATH_MAX];
		if (fscanf(f, "%s %s", b, dpi) < 0) {
			fclose(f);
			return;
		}
		dprintf("dmi-dpi entry: boardname=%s dpi=%s", b, dpi);
		if (!strcmp(boardname, b)) {
			strncpy(dpinum, dpi, sizeof(dpinum)-1);
			lprintf("Using dpi=%s based on dmi-dpi table", dpinum);
			fclose(f);
			return;
		}
	}
	fclose(f);

	d_out();
}

void get_options(int argc, char **argv)
{
	int i = 0;
	int c;
	FILE *f;
	DIR *dir;
	struct dirent *entry;

	d_in();
	/*
	 * default fallbacks are listed above in the declations
	 * each step below overrides them in order
	 */

	/* try and find a user in /home/ */
	dir = opendir("/home");
	while (dir) {
		char buf[80];
		char *u;
		struct passwd *p;

		entry = readdir(dir);
		if (!entry)
			break;
		if (entry->d_name[0] == '.')
			continue;
		if (strcmp(entry->d_name, "lost+found")==0)
			continue;
		if (entry->d_type != DT_DIR)
			continue;

		u = strdup(entry->d_name);
		dprintf("Found potential user folder %s", u);
		/* check if this is actually a valid user */
		p = getpwnam(u);
		if (!p)
			goto next;
		/* and make sure this is actually the guys homedir */
		snprintf(buf, 80, "/home/%s", u);
		if (strcmp(p->pw_dir, buf))
			goto next;
		strncpy(username, u, sizeof(username) - 1);
next:
		free(u);
	}
	if (dir)
		closedir(dir);

	/*
	 * DPI setting order:
	 * - auto by default
	 * - dmi lookup table if it exists
	 * - config file overrides as normal
	 */
	get_dmi_dpi();

	/* parse config file */
	f = fopen("/etc/sysconfig/uxlaunch", "r");
	if (f) {
		char buf[256];
		char *key;
		char *val;

		while (fgets(buf, 80, f) != NULL) {
			char *c;

			c = strchr(buf, '\n');
			if (c) *c = 0; /* remove trailing \n */

			dprintf("config file: %s", buf);

			if (buf[0] == '#')
				continue; /* comment line */

			key = strtok(buf, "=");
			if (!key)
				continue;
			val = strtok(NULL, "=");
			if (!val)
				continue;

			// todo: filter leading/trailing whitespace

#ifdef ENABLE_CHOOSER
			if (!strcmp(key, "chooser"))
				strncpy(chooser, val, sizeof(chooser) - 1);
#endif
			if (!strcmp(key, "user"))
				strncpy(username, val, sizeof(username) - 1);
			if (!strcmp(key, "tty"))
				tty = atoi(val);
			if (!strcmp(key, "session"))
				strncpy(session, val, sizeof(session) - 1);
			if (!strcmp(key, "settle"))
				settle = atoi(val);
			if (!strcmp(key, "dpi"))
				strncpy(dpinum, val, sizeof(dpinum) - 1);
			if (!strcmp(key, "xopts")) {
			        strncpy(addn_xopts, val, sizeof(addn_xopts) - 1);
			}
 		}
		fclose(f);
	}

	/* parse cmdline - overrides */
	while (1) {
		c = getopt_long(argc, argv,
#ifdef ENABLE_CHOOSER
				"c:u:t:s:Shvx",
#else
				"u:t:s:Shvx",
#endif
				opts, &i);
		if (c == -1)
			break;

		switch (c) {
#ifdef ENABLE_CHOOSER
		case 'c':
			strncpy(chooser, optarg, sizeof(chooser) - 1);
			break;
#endif
		case 'u':
			strncpy(username, optarg, sizeof(username) - 1);
			break;
		case 't':
			tty = atoi(optarg);
			break;
		case 's':
			strncpy(session, optarg, sizeof(session) - 1);
			break;
		case 'S':
			settle = 1;
			break;
		case 'h':
			usage(argv[0]);
			exit (EXIT_SUCCESS);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'x':
			x_session_only = 1;
			if (getenv ("USER"))
				strncpy (username, getenv ("USER"), sizeof(username) - 1);
			break;
		default:
			break;
		}
	}

	/* Get session command from startup line */
	while (i < argc) {
		if (!strcmp(argv[i++], "--")) {
			session[0] = '\0';
			while (i < argc) {
				strncat(session, argv[i++], 256 - strlen(session));
				if (i != argc)
					strncat(session, " ", 256 - strlen(session));
			}
		}
	}

	lprintf("uxlaunch v%s started%s.", VERSION, x_session_only ? " for x session only" : "" );
	lprintf("user \"%s\", tty #%d, session \"%s\"", username, tty, session);

	pass = getpwnam(username);
	if (!pass) {
		lprintf("Error: can't find user \"%s\"", username);
		exit(EXIT_FAILURE);
	}
	d_out();
}
