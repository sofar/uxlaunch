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
#include <dirent.h>
#include <time.h>
#include <glib.h>
#include <limits.h>
#include <pwd.h>

#include "uxlaunch.h"
#if defined(__i386__)
#  define __NR_ioprio_set 289
#elif defined(__x86_64__)
#  define __NR_ioprio_set 251
#else
#  error "Unsupported arch"
#endif
#define IOPRIO_WHO_PROCESS 1
#define IOPRIO_CLASS_IDLE 3
#define IOPRIO_CLASS_SHIFT 13
#define IOPRIO_IDLE_LOWEST (7 | (IOPRIO_CLASS_IDLE << IOPRIO_CLASS_SHIFT))


int session_pid;
char session_filter[16] = "MOBLIN";
static int delay = 0;

/*
 * 50ms steps in between async job startups
 * lower priority jobs are started up further apart
 */
#define DELAY_UNIT 50000

struct desktop_entry_struct {
	char *exec;
	int prio;
};

static GList *desktop_entries;


static void desktop_entry_add(const char *exec, int prio)
{
	GList *item;
	struct desktop_entry_struct *entry;

	/* make sure we don't insert items twice */
	item = g_list_first(desktop_entries);
	while (item) {
		entry = item->data;
		if (!strcmp(entry->exec, exec)) {
			lprintf("Duplicate entry %s", exec);
			return;
		}
		item = g_list_next(item);
	}

	entry = malloc(sizeof(struct desktop_entry_struct));
	if (!entry) {
		lprintf("Error allocating memory for desktop entry");
		return;
	}
	entry->prio = prio; /* panels start at highest prio */
	entry->exec = g_strdup(exec);
	lprintf("Adding %s with prio %d", entry->exec, entry->prio);
	desktop_entries = g_list_prepend(desktop_entries, entry);
}


gint sort_entries(gconstpointer a, gconstpointer b)
{
	const struct desktop_entry_struct *A = a, *B = b;

	if (A->prio > B->prio)
		return 1;
	if (A->prio < B->prio)
		return -1;
	return strcmp(A->exec, B->exec);
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
static void do_desktop_file(const char *filename)
{
	FILE *file;
	char line[4096];
	char exec[4096];
	int show = 1;
	int prio = 1; /* medium/normal prio */

	file = fopen(filename, "r");
	if (!file)
		return;

	memset(exec, 0, 4096);
	while (!feof(file)) {
		char *c;
		if (fgets(line, 4096, file) == NULL)
			break;
		c = strchr(line, '\n');
		if (c) *c = 0;
		if (strstr(line, "Exec="))
			strncpy(exec, line+5, 4095);

		if (strstr(line, "OnlyShowIn"))
			if (strstr(line, session_filter) == NULL)
				show = 0;

		if (strstr(line, "NotShowIn")) {
			if (strstr(line, session_filter))
				show = 0;
			/* for moblin, hide stuff hidden to gnome */
			if (!strcmp(session_filter, "MOBLIN"))
				if (strstr(line, "GNOME"))
					show = 0;
		}

		if (strstr(line, "X-Moblin-Priority")) {
			if (strstr(line, "Highest"))
				prio = -1;
			if (strstr(line, "High"))
				prio = 0;
			/* default: prio = 1 */
			if (strstr(line, "Low"))
				prio = 2;

		}
	}
	fclose(file);

	if (show && strlen(exec)>0)
		desktop_entry_add(exec, prio);
}


void autostart_panels(void)
{

	if (!strstr(session_filter, "MOBLIN"))
		return;
	if (!strstr(session, "mutter"))
		return;

	desktop_entry_add("/usr/libexec/moblin-panel-myzone", -1);
	desktop_entry_add("/usr/libexec/moblin-panel-status", 0);
	desktop_entry_add("/usr/libexec/moblin-panel-people", 0);
	desktop_entry_add("/usr/libexec/moblin-panel-internet", 0);
	desktop_entry_add("/usr/libexec/moblin-panel-media", 1);
	desktop_entry_add("/usr/libexec/moblin-panel-pasteboard", 0);
	desktop_entry_add("/usr/libexec/moblin-panel-applications", 1);
}


void get_session_type(void)
{
	/* adjust filter based on what our session cmd is */
	if (strstr(session, "neskowin"))
		snprintf(session_filter, 16, "MUX");
	if (strstr(session, "xfce"))
		snprintf(session_filter, 16, "XFCE");
	if (strstr(session, "gnome"))
		snprintf(session_filter, 16, "GNOME");
	if (strstr(session, "kde"))
		snprintf(session_filter, 16, "KDE");
	/* default == MOBLIN */
}

/*
 * We need to process all the .desktop files in /etc/xdg/autostart.
 * Simply walk the directory
 */
void autostart_desktop_files(void)
{
	DIR *dir;
	struct dirent *entry;
	char user_path[PATH_MAX];
	char user_file[PATH_MAX];

	lprintf("Entering autostart_desktop_files");

	snprintf(user_path, PATH_MAX, "/home/%s/.config/autostart",
		 pass->pw_name);

	dir = opendir("/etc/xdg/autostart");
	if (!dir) {
		lprintf("System autostart directory not found");
	} else {
		while (1) {
			char filename[PATH_MAX];
			entry = readdir(dir);
			if (!entry)
				break;
			if (entry->d_name[0] == '.')
				continue;
			if (entry->d_type != DT_REG)
				continue;
			if (strchr(entry->d_name, '~'))
				continue;  /* editor backup file */

			/* 
			 * filter - don't run this file if same-named
			 * file exists in user_path
			 */
			snprintf(user_file, PATH_MAX, "%s/%s", user_path,
				 entry->d_name);
			if (!access(user_file, R_OK))
				continue;
	
			snprintf(filename, PATH_MAX, "/etc/xdg/autostart/%s",
				 entry->d_name);
			do_desktop_file(filename);
		}
	}
	closedir(dir);
	
	snprintf(user_path, PATH_MAX, "/home/%s/.config/autostart",
		 pass->pw_name);
	dir = opendir(user_path);
	if (!dir) {
		lprintf("User autostart directory not found");
	} else {
		while (1) {
			char filename[PATH_MAX];
			entry = readdir(dir);
			if (!entry)
				break;
			if (entry->d_name[0] == '.')
				continue;
			if (entry->d_type != DT_REG)
				continue;
			if (strchr(entry->d_name, '~'))
				continue;  /* editor backup file */
			snprintf(filename, PATH_MAX, "%s/%s", user_path,
				 entry->d_name);
			do_desktop_file(filename);
		}

	}
	closedir(dir);
}


void start_xinitrd_scripts(void)
{
	DIR* dir;

	dir = opendir("/etc/X11/xinit/xinitrc.d");
	while (dir) {
		int ret;
		struct dirent *entry;
		char cmd[PATH_MAX];

		entry = readdir(dir);
		if (!entry)
			break;
		if (entry->d_type != DT_REG)
			continue;

		/* queue every 0.4s after xdg/autostart stuff */
		delay = delay + 8 * DELAY_UNIT;

		if (fork())
			continue;

		usleep(delay);

		snprintf(cmd, PATH_MAX, "/etc/X11/xinit/xinitrc.d/%s", entry->d_name);
		lprintf("Starting \"%s\" at %d", cmd, delay);
		ret = system(cmd);
		if (ret)
			lprintf("Warning: \"%s\" returned %d", cmd, ret);
		exit(ret);
	}
}


void do_autostart(void)
{
	GList *item;
	struct desktop_entry_struct *entry;

	lprintf("Entering do_autostart");

	/* sort by priority */
	desktop_entries = g_list_sort(desktop_entries, sort_entries);

	item = g_list_first(desktop_entries);

	while (item) {
		char *ptrs[256];
		int count = 0;
		int ret = 0;

		entry = item->data;

		delay = delay + ((1 << (entry->prio + 1)) * DELAY_UNIT);
		lprintf("Queueing %s with prio %d at %d", entry->exec, entry->prio, delay);

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
		lprintf("Starting %s with prio %d at %d", entry->exec, entry->prio, delay);
		execvp(ptrs[0], ptrs);
		exit(ret);
	}

	start_xinitrd_scripts();
}


void start_desktop_session(void)
{
	int ret;
	int count = 0;
	char *ptrs[256];

	ret = fork();

	if (ret) {
		session_pid = ret;
		return; /* parent continues */
	}

	lprintf("Entering start_desktop_session");

	ret = system("/usr/bin/xdg-user-dirs-update");
	if (ret)
		lprintf("/usr/bin/xdg-user-dirs-update failed");

	memset(ptrs, 0, sizeof(ptrs));

	ptrs[0] = strtok(session, " \t");
	while (ptrs[count] && count < 255)
		ptrs[++count] = strtok(NULL, " \t");
	
	ret = execv(ptrs[0], ptrs);

	if (ret != EXIT_SUCCESS)
		lprintf("Failed to start %s", session);
}

