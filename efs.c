/*
 * This file is part of uxlaunch
 *
 * (C) Copyright 2009, 2010 Intel Corporation
 * Authors:
 *     Yan Li <yan.i.li@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "uxlaunch.h"


static int ecryptfs_automount_set()
{
	const char *homedir = pass->pw_dir;
	char *file_path;

	if (asprintf(&file_path, "%s/.ecryptfs/auto-mount", homedir) < 0) {
		lprintf("Error: EFS: %s asprintf failed", __func__);
		return 0;
	}
	if (access(file_path, F_OK) == 0) {
		/* ecryptfs detected, need to mount */
		free(file_path);
		return -1;
	}
	if (errno != ENOENT) {
		lprintf("Error: EFS: access() failed when checking auto-mount file, errno: %d", errno);
	}
	free(file_path);
	/* no ecryptfs automount hint found */
	return 0;
}


static int grep(const char *filename, const char *pattern)
{
	char *buf = NULL;
	size_t buf_n;
	int ret = 1;
	FILE *f = fopen(filename, "r");
	if (!f) {
		lprintf("Error: EFS: unable to open %s", filename);
		return 1;
	}
	while (getline(&buf, &buf_n, f) > 0) {
		printf("n: %d\n", buf_n);
		if (strstr(buf, pattern)) {
			ret = 0;
			goto out;
		}
	}
out:
	free(buf);
	fclose(f);
	return ret;
}


/*
 * Check if the homedir is already mounted
 */
static int ecryptfs_mounted()
{
	char *search_pattern;

	if (asprintf(&search_pattern, " %s ecryptfs ", pass->pw_dir) < 0) {
		lprintf("Error: EFS: %s asprintf failed", __func__);
		return 0;
	}

	return grep("/proc/mounts", search_pattern) == 0;
}


static void start_greeter(void)
{
	int ret;

	init_screensaver(1);
	/* wait for screensaver to close */
	ret = system("/usr/bin/gnome-screensaver-command --wait");
	if (ret)
		lprintf("Failed on /usr/bin/gnome-screensaver-command --wait, rc: %d", ret);
}


void setup_efs(void)
{
	int ret;
	pid_t pid;

	/* we need to be fast, do nothing unless absolutely needed */
	if (!ecryptfs_automount_set())
		return;

	/* do nothing if it's alreay mounted */
	if (ecryptfs_mounted()) {
		lprintf("EFS is already mounted, do nothing");
		return;
	}

	if (grep("/proc/filesystems", "ecryptfs") != 0) {
		ret = system("/sbin/modprobe ecryptfs");
		if (0 != ret) {
			lprintf("Error: EFS: failed to modprobe ecryptfs");
			return;
		}
	}

	if ((pid = fork()) < 0) {
		lprintf("Error: EFS: Failed to fork in setup_efs");
		return;
	} else if (pid == 0) {
		/* child process */
		lprintf("EFS: start authentication");

		switch_to_user();
		start_X_server();
		wait_for_X_signal();

		/* start dbus session */
		start_dbus_session_bus();
		start_greeter();

		lprintf("EFS: authentication success");

		/* kill Xorg, clean up */
		kill(xpid, SIGTERM);
		wait_for_X_exit();
		stop_dbus_session_bus();

		exit(0);
	}

	/* parent */
	if ((pid = waitpid(pid, &ret, 0)) < 0) {
		lprintf("Error: EFS: setup_efs waitpid error");
		return;
	}
}
