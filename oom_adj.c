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
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "uxlaunch.h"


struct oom_adj_struct {
	pid_t pid;
	int prio;
};

static int oom_pipe[2];


void start_oom_task()
{
	struct oom_adj_struct request;
	pid_t pid;

	if (pipe(oom_pipe) == -1) {
		lprintf("Failed to open oom_adj pipe");
		exit(EXIT_FAILURE);
	}

	pid = fork();
	if (pid == -1) {
		lprintf("Failed to fork oom_adj task");
		exit(EXIT_FAILURE);
	}

	if (pid != 0) {
		close(oom_pipe[0]);
		return;
	}

	/* child */
	close(oom_pipe[1]);

	/* handle requests */
	while (read(oom_pipe[0], &request, sizeof(request)) > 0) {
		char path[PATH_MAX];
		char val[16];
		int fd;

		snprintf(path, PATH_MAX, "/proc/%d/oom_score_adj", request.pid);
		snprintf(val, 16, "%d", request.prio);
		fd = open(path, O_WRONLY);
		if (fd < 0) {
			lprintf("Failed to write oom_core_dj score file: %s",
			path);
			continue;
		}
		if (write(fd, &val, strlen(val)) < 0)
			lprintf("Failed to write oom_score_adj value: %s: %d",
				path, request.prio);
		close(fd);

	}

	/* close pipe and exit */
	close(oom_pipe[0]);
	exit(EXIT_SUCCESS);

}


void stop_oom_task()
{
	close(oom_pipe[1]);
}


void oom_adj(int pid, int prio)
{
	struct oom_adj_struct request;

	request.pid = pid;
	request.prio = prio;

	if (write(oom_pipe[1], &request, sizeof(request)) < 0)
		lprintf("Error: unable to write to oom_adj pipe: pid [%d] "
			"prio %d", pid, prio);
}
