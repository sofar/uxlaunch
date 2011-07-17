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
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <glib.h>
#include <limits.h>
#include <pwd.h>
#include <wordexp.h>
#include <asm/unistd.h>

#include "uxlaunch.h"

#define IOPRIO_WHO_PROCESS 1
#define IOPRIO_CLASS_IDLE 3
#define IOPRIO_CLASS_SHIFT 13
#define IOPRIO_IDLE_LOWEST (7 | (IOPRIO_CLASS_IDLE << IOPRIO_CLASS_SHIFT))


int session_pid;
gchar *session_filter = NULL;
gchar *session_exec = NULL;
static int delay = 0;

/*
 * 50ms steps in between async job startups
 * lower priority jobs are started up further apart
 */
#define DELAY_UNIT 50000

/* watchdog types */
#define WD_NONE 0
#define WD_HALT 1
#define WD_RESTART 2
#define WD_FAIL 3

struct desktop_entry_struct {
	gchar *file;
	gchar *exec;
	int prio;
	int watchdog;
};

static GList *desktop_entries;


/*
 * Test whether a file exists, expanding ~ and $HOME in the string
 */
static int file_expand_exists(const char *path)
{
	wordexp_t p;
	char **w;

	d_in();

	/* don't expand backticks and shell calls like $(foo) */
	wordexp(path, &p, WRDE_NOCMD | WRDE_UNDEF);
	w = p.we_wordv;
	if (p.we_wordc > 1) {
		/* expansion found multiple files - so the file exists */
		wordfree(&p);
		return -1;
	}

	/*
	 * expansion may have succeeded, or not, so we need to explicitly
	 * check if the file exists now
	 */
	if (!access(w[0], F_OK)) {
		wordfree(&p);
		return -1;
	}

	wordfree(&p);

	d_out();
	return 0;
}

static void desktop_entry_add(const gchar *file, const gchar *exec, int prio, int wd)
{
	GList *item;
	struct desktop_entry_struct *entry;

	d_in();

	/* make sure we don't insert items twice */
	item = g_list_first(desktop_entries);
	while (item) {
		entry = item->data;
		if (!strcmp(entry->file, file)) {
			dprintf("Overwriting existing entry: %s", file);
			/* overwrite existing entry with higher priority */
			desktop_entries = g_list_remove(desktop_entries, entry);
			goto overwrite;
		}
		item = g_list_next(item);
	}

	dprintf("Inserting new entry: %s", file);

overwrite:
	entry = malloc(sizeof(struct desktop_entry_struct));
	if (!entry) {
		lprintf("Error allocating memory for desktop entry");
		return;
	}

	entry->prio = prio; /* panels start at highest prio */
	entry->exec = g_strdup(exec);
	entry->file = g_strdup(file);
	entry->watchdog = wd;
	if (entry->exec)
		dprintf("Adding %s with prio %d", file, entry->prio);
	else 
		dprintf("Hiding %s", file);

	desktop_entries = g_list_prepend(desktop_entries, entry);

	d_out();
}


gint sort_entries(gconstpointer a, gconstpointer b)
{
	const struct desktop_entry_struct *A = a, *B = b;

	if (A->prio > B->prio)
		return 1;
	if (A->prio < B->prio)
		return -1;
	if (A->exec && B->exec)
		return strcmp(A->exec, B->exec);
	if (A->exec)
		return 1;
	return -1;
}


/*
 * Process a .desktop file
 * Objective: fine the "Exec=" line which has the command to run
 * Constraints:
 *   if there is a OnlyShowIn line, then it must contain the string "MOBLIN"
 *   (this is to allow moblin-only settings apps to show up, but not gnome settings apps)
 *   if there is a NotShowIn line, it must not contain "MOBLIN" or "GNOME"
 *   (this allows KDE etc systems to hide stuff for GNOME as they do today and not show it
 *    on moblin)
 */
static void do_desktop_file(const gchar *dir, const gchar *file)
{

	GKeyFile *keyfile;
	GError *error = NULL;
	gchar *exec_key;
	gchar *onlyshowin_key;
	gchar *notshowin_key;
	gchar *prio_key;
	gchar *onlystart_key;
	gchar *dontstart_key;
	gchar *wd_key;
	gchar *filename = NULL;
	int wd = 0;

	d_in();

	filename = g_strdup_printf("%s/%s", dir, file);

	int prio = 1; /* medium/normal prio */

	dprintf("Parsing %s", filename);

	keyfile = g_key_file_new();
	if (!g_key_file_load_from_file(keyfile, filename, 0, &error)) {
		g_error("%s\n", error->message);
		return;
	}

	exec_key = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
	if (!exec_key)
		goto hide;

	onlyshowin_key = g_key_file_get_string(keyfile, "Desktop Entry", "OnlyShowIn", NULL);
	notshowin_key = g_key_file_get_string(keyfile, "Desktop Entry", "NotShowIn", NULL);
	prio_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-MeeGo-Priority", NULL);
	if (!prio_key)
		prio_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Meego-Priority", NULL);
	if (!prio_key)
		prio_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Moblin-Priority", NULL);

	onlystart_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Meego-OnlyStartIfFileExists", NULL);
	if (!onlystart_key)
		onlystart_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Moblin-OnlyStartIfFileExists", NULL);

	dontstart_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Meego-DontStartIfFileExists", NULL);
	if (!dontstart_key)
		dontstart_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Moblin-DontStartIfFileExists", NULL);

	/*
	 * Filtering desktop files is case insensitive, e.g.
	 * when using gnome, GNOME is also matched in these keys.
	 */
	if (onlyshowin_key) {
		gchar **partial;
		int n;

		dprintf("OnlyShowIn=%s", onlyshowin_key);
		partial = g_strsplit(onlyshowin_key, ";", -1);

		for (n = 0; partial[n] != NULL; n++) {
			dprintf("...%s", partial[n]);
			if (!g_ascii_strcasecmp(partial[n], session_filter)) {
				g_strfreev(partial);
				goto notshowin;
			}
		}
		g_strfreev(partial);
		/* nothing matched - hide */
		goto hide;
	}
notshowin:
	if (notshowin_key) {
		gchar **partial;
		int n;

		dprintf("NotShowIn=%s", notshowin_key);
		partial = g_strsplit(notshowin_key, ";", -1);

		for (n = 0; partial[n] != NULL; n++) {
			dprintf("...%s", partial[n]);
			if (!g_ascii_strcasecmp(partial[n], session_filter)) {
				g_strfreev(partial);
				goto hide;
			}
		}
		g_strfreev(partial);
	}

	if (onlystart_key)
		if (!file_expand_exists(onlystart_key))
			goto hide;
	if (dontstart_key)
		if (file_expand_exists(dontstart_key))
			goto hide;

	if (prio_key) {
		gchar *p = g_utf8_casefold(prio_key, g_utf8_strlen(prio_key, -1));
		if (g_strstr_len(p, -1, "highest"))
			prio = -1;
		else if (g_strstr_len(p, -1, "high"))
			prio = 0;
		else if (g_strstr_len(p, -1, "low"))
			prio = 2;
		else if (g_strstr_len(p, -1, "late"))
			prio = 3;
		else
			lprintf("Unknown value for key X-MeeGo-Priority: %s", prio_key);
	}

	wd_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-Meego-Watchdog", NULL);
	if (!wd_key)
		wd_key = g_key_file_get_string(keyfile, "Desktop Entry", "X-MeeGo-Watchdog", NULL);
	if (wd_key) {
		gchar *p = g_utf8_casefold(wd_key, g_utf8_strlen(wd_key, -1));
		if (g_strstr_len(p, -1, "halt"))
			wd = WD_HALT;
		else if (g_strstr_len(p, -1, "restart"))
			wd = WD_RESTART;
		else if (g_strstr_len(p, -1, "fail"))
			wd = WD_FAIL;
		else
			lprintf("Unknown value for key X-Meego-Watchdog: %s", wd_key);
	}

	desktop_entry_add(file, g_shell_unquote(exec_key, &error), prio, wd);
	dprintf("NOT hiding %s", file);
	goto done;
hide:
	dprintf("Hiding %s", file);
	desktop_entry_add(file, NULL, -1, wd);
done:
	g_free(filename);
	d_out();
}


void get_session_type(void)
{
	GKeyFile *keyfile;
	GError *error = NULL;
	gchar *filename = NULL;
	char buf[PATH_MAX];
	char *c;
	struct stat st;

	d_in();
	/*
	 * DE Session setup
	 *
	 * Here's how it works:
	 *
	 * packages can drop SESSION files in /usr/share/xsessions. These
	 * contain misc information used in choosers and Exec= lines containing
	 * the session master process. The session filter is determined
	 * by taking the session.desktop filename and removing the extension;
	 * so a gnome.desktop session file describes the GNOME session.
	 *
	 * priority order is determined in options.c
	 *
	 * by default, we look for 'default.desktop' which can reside
	 * in several locations: (listed in low to high priority)
	 * - /usr/share/xsessions/default.desktop
	 * - /etc/X11/dm/Sessions/default.desktop
	 * - ~/.config/xsessions/default.desktop
	 *
	 * if you change the session, it will look for the identifier
	 * you provided instead (and append ".desktop")
	 */

	filename = g_strdup_printf("%s/xsessions/%s.desktop", getenv("XDG_CONFIG_HOME"), session);
	if (!access(filename, R_OK))
		goto found_session;
	filename = g_strdup_printf("/etc/X11/dm/Sessions/%s.desktop", session);
	if (!access(filename, R_OK))
		goto found_session;
	filename = g_strdup_printf("/usr/share/xsessions/%s.desktop", session);
	if (!access(filename, R_OK))
		goto found_session;

	lprintf("Unable to find session \"%s.desktop\"!", filename);

	lprintf("WARNING: using DEPRECATED session mechanics. Please read `man uxlaunch` on");
	lprintf("how to setup a session file properly instead!");

	if (!access(session, X_OK)) {
		session_exec = g_strdup(session);
		lprintf("ERROR: unable to determine the session filter string, using invalid value");
	} else {
		lprintf("%s: not a valid executable", session);
		exit(EXIT_FAILURE);
	}

	session_filter = g_strdup("X-UNKNOWN");
	goto session_done;

found_session:
	keyfile = g_key_file_new();
	if (!g_key_file_load_from_file(keyfile, filename, 0, &error)) {
		g_error("%s\n", error->message);
		return;
	}

	session_exec = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
	if (!session_exec) {
		lprintf("%s: invalid session file: no valid Exec= key", filename);
		exit(EXIT_FAILURE);
	}

	/* is the file a symlink? e.g. default.desktop -> gnome.desktop? */
	if (lstat(filename, &st)) {
		lprintf("%s: unable to lstat()", filename);
		exit(EXIT_FAILURE);
	}
	if (S_ISLNK(st.st_mode)) {
		ssize_t l;
		/* now, readlink() the file.desktop and look at the basename */
		l = readlink(filename, buf, sizeof(buf) -1);
		if (l < 0) {
			lprintf("%s: unable to determine link target", filename);
			exit(EXIT_FAILURE);
		}
		buf[l] = 0;
		c = buf;
	} else {
		c = filename;
	}

	/* get the basename without .desktop */
	c = strrchr(c, '/');
	if (!c)
		c = buf; /* relative link, ignore */
	else
		c++; /* leading / itself */

	if (strlen(c) <= strlen(".desktop")) {
		lprintf("%s: funny, malformed link target", filename);
		exit(EXIT_FAILURE);
	}
	*(c + strlen(c) - strlen(".desktop")) = 0;

	/* and use that as session_filter */
	session_filter = g_strdup(c);

session_done:
	lprintf("Session filter key = \"%s\"", session_filter);
	lprintf("Session program = \"%s\"", session_exec);

	setenv("X_DESKTOP_SESSION", session_filter, 1);
	d_out();
}


static int entry_is_reg(const gchar *dir, const struct dirent *entry)
{
	struct stat info;
	gchar *filename = NULL;

	switch (entry->d_type) {
	case DT_REG:
		return 1;
		break;
	case DT_UNKNOWN: /* returned on e.g. NFS filesystem */
		filename = g_strdup_printf("%s/%s", dir, entry->d_name);
		if (!stat(filename, &info) && (S_ISREG(info.st_mode))) {
			g_free(filename);
			return 1;
		}
		g_free(filename);
		break;
	default:
		break;
	}

	return 0;
}

void do_dir(const gchar *dir)
{
	DIR *d;
	struct dirent *entry;

	d = opendir(dir);
	if (!d) {
		lprintf("autostart directory \"%s\" not found", dir);
	} else {
		while (1) {
			entry = readdir(d);
			if (!entry)
				break;
			if (entry->d_name[0] == '.')
				continue;
			if (!entry_is_reg(dir, entry))
				continue;
			if (strchr(entry->d_name, '~'))
				continue;  /* editor backup file */

			do_desktop_file(dir, entry->d_name);
		}
	}
	closedir(d);
}

/*
 * We need to process all the .desktop files in /etc/xdg/autostart.
 * Simply walk the directory
 */
void autostart_desktop_files(void)
{
	gchar *xdg_config_dirs = NULL;
	gchar *xdg_config_home = NULL;
	gchar **xdg_config_dir = NULL;
	gchar *x = NULL;
	int count = 0;

	d_in();

	if (getenv("XDG_CONFIG_HOME"))
		xdg_config_home = g_strdup(getenv("XDG_CONFIG_HOME"));
	else
		xdg_config_home = g_strdup_printf("%s/.config", getenv("HOME"));

	if (getenv("XDG_CONFIG_DIRS"))
		xdg_config_dirs = g_strdup(getenv("XDG_CONFIG_DIRS"));
	else
		xdg_config_dirs = g_strdup("/etc/xdg");

	/* count how many dirs are listed, so we can iterate backwards */
	xdg_config_dir = g_strsplit(xdg_config_dirs, ";", -1);
	g_assert(xdg_config_dir);
	while (xdg_config_dir[count])
		count++;

	while (count > 0) {
		x = g_strdup_printf("%s/autostart", xdg_config_dir[--count]);
		do_dir(x);
		g_free(x);
	}

	x = g_strdup_printf("%s/autostart", xdg_config_home);
	do_dir(x);
	g_free(x);

	g_strfreev(xdg_config_dir);
	g_free(xdg_config_home);

	d_out();
}


void do_autostart(void)
{
	GList *item;
	struct desktop_entry_struct *entry;
	int restarts = 0;

	d_in();

	/* sort by priority */
	desktop_entries = g_list_sort(desktop_entries, sort_entries);

	item = g_list_first(desktop_entries);

	while (item) {
		char *ptrs[256];
		int count = 0;
		int ret = 0;
		int late = 0;
		int pid;
		int status;

		entry = item->data;

		if (!entry->exec) {
			/* hidden item */
			item = g_list_next(item);
			continue;
		}

		if (entry->prio >= 3) {
			if (!late) {
				/* then add late stuff */
				delay += 60000000;
				late = 1;
			}
			delay += 15000000; /* 15 seconds in between */
		} else {
			delay += ((1 << (entry->prio + 1)) * DELAY_UNIT);
		}
		dprintf("Queueing %s:%s with prio %d at %d", entry->file, entry->exec, entry->prio, delay);

		if (fork()) {
			item = g_list_next(item);
			continue;
		}

		if (entry->prio >= 1) {
			syscall(__NR_ioprio_set, IOPRIO_WHO_PROCESS, 0, IOPRIO_IDLE_LOWEST);
			ret = nice(5);
		}

		memset(ptrs, 0, sizeof(ptrs));

		ptrs[0] = strtok(entry->exec, " \t");
		while (ptrs[count] && count < 255)
			ptrs[++count] = strtok(NULL, " \t");

		usleep(delay);
		dprintf("Starting %s:%s with prio %d at %d", entry->file, entry->exec, entry->prio, delay);

		/* watchdog handling */
		if (entry->watchdog == WD_NONE) {
			execvp(ptrs[0], ptrs);
			exit(ret);
		}

restart:
		pid = fork();
		if (pid == 0) {
			execvp(ptrs[0], ptrs);
			lprintf("Failed to execvp(%s)", entry->exec);
			exit(EXIT_FAILURE);
		} else if (pid < 0) {
			lprintf("Failed to fork for %s", entry->exec);
			exit(EXIT_FAILURE);
		}

		/* stop and wait for child to exit */
		ret = waitpid(pid, &status, 0);

		if (WIFEXITED(status))
			lprintf("process %d (%s:%s) exited with exit code %d",
				ret, entry->file, entry->exec, WEXITSTATUS(status));

		if (WIFSIGNALED(status))
			lprintf("process %d (%s:%s) was killed by signal %d",
				ret, entry->file, entry->exec, WTERMSIG(status));

		if (((entry->watchdog == WD_FAIL) && (WEXITSTATUS(status))) ||
		    ((entry->watchdog == WD_FAIL) && (WIFSIGNALED(status))) ||
		     (entry->watchdog == WD_RESTART)) {
			/* safety: reasonable sleep here */
			sleep((restarts++ < 5) ? restarts : 900); /* 15 mins */
			lprintf("Watchdog: restarting %s:%s", entry->file, entry->exec);
			goto restart;
		}

		if (entry->watchdog == WD_HALT) {
			/* tear down the session */
			lprintf("Watchdog: %s:%s exited, tearing down session", entry->file, entry->exec);
			kill(session_pid, SIGTERM);
			exit(EXIT_FAILURE);
		}

		lprintf("Watchdog: unhandled exception");
		exit(EXIT_FAILURE);
	}

	d_out();
}

void wait_for_session_exit(void)
{
	d_in();

	for (;;) {
		errno = 0;
		if (waitpid (session_pid, NULL, 0) < 0) {
			if (errno == EINTR) {
				continue;
			} else if (errno == ECHILD)
				break; /* child already reaped */
			else
				lprintf("waidpid error '%s'", strerror (errno));
		}
		break;
	}

	d_out();
}

void start_desktop_session(void)
{
	int ret;
	int count = 0;
	char *ptrs[256];

	d_in();

	ret = fork();

	if (ret) {
		session_pid = ret;
		return; /* parent continues */
	}

	ret = system("/usr/bin/xdg-user-dirs-update");
	if (ret)
		lprintf("/usr/bin/xdg-user-dirs-update failed");

	memset(ptrs, 0, sizeof(ptrs));

	ptrs[0] = strtok(session_exec, " \t");
	while (ptrs[count] && count < 255)
		ptrs[++count] = strtok(NULL, " \t");

	ret = execvp(ptrs[0], ptrs);

	if (ret != EXIT_SUCCESS)
		lprintf("Failed to start %s", session_exec);

	d_out();
}
