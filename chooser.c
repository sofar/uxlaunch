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
#include <sys/shm.h>
#include <unistd.h>
#include "uxlaunch.h"
#include "uxlaunch-ipc.h"


void setup_chooser(void)
{
	int ret, shm_id;
	uxlaunch_chooser_shm *shm;
	char shm_id_str[50];
	const int shm_size = sizeof(uxlaunch_chooser_shm);
	pid_t pid;
	gid_t old_gid;

	old_gid = getegid();
	setegid(pass->pw_gid);
	shm_id = shmget(IPC_PRIVATE, shm_size,
		IPC_CREAT | IPC_EXCL | S_IRGRP | S_IWGRP);
		//IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
	shm = (uxlaunch_chooser_shm *)shmat(shm_id, 0, 0);
	if (shm == (void*)-1) {
		lprintf("chooser: Unable to allocate SHM, abort\n");
		return;
	}

	snprintf(shm_id_str, 50, "%i", shm_id);
	lprintf("chooser: shared memory with key %i attached at address %p\n",
		shm_id, shm);

	strncpy(shm->user, username, 255);
	strncpy(shm->session_path, session, 256);

	if ((pid = fork()) < 0) {
		lprintf("Error: chooser: Failed to fork in setup_chooser");
		return;
	} else if (pid == 0) {
		/* child process */
		lprintf("chooser: start authentication");

		switch_to_user();
		start_X_server();
		wait_for_X_signal();

		setenv("SHM_ID", shm_id_str, 1);
		ret = system(chooser);

		/* kill Xorg, clean up */
		kill(xpid, SIGTERM);
		wait_for_X_exit();

		exit(ret);
	}

	/* parent */
	if ((pid = waitpid(pid, &ret, 0)) < 0) {
		lprintf("Error: chooser: setup_chooser waitpid error");
	} else {
		lprintf("chooser: switching to user '%s', with session '%s'\n",
			shm->user, shm->session_path);
		strncpy(username, shm->user, 255);
		strncpy(session, shm->session_path, 256);
	}

	shmctl(shm_id, IPC_RMID, 0);
	setegid(old_gid);
}
