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
#include <string.h>
#include <syslog.h> 

#include "uxlaunch.h"


extern char **environ;

static int first_time = 1;

static struct timeval start;


void lprintf(const char* fmt, ...)
{
	va_list args;
	struct timeval current;
	uint64_t secs, usecs;
	char string[8192];
	char msg[8192];

	if (first_time) {
		first_time = 0;
		gettimeofday(&start, NULL);
	}

	va_start(args, fmt);
	vsnprintf(msg, 8192, fmt, args);
	va_end(args);

	if (msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = '\0';


	if (verbose) {
		gettimeofday(&current, NULL);

		secs = current.tv_sec - start.tv_sec;

		while (current.tv_usec < start.tv_usec) {
			secs --;
			current.tv_usec += 1000000;
		}
		usecs = current.tv_usec - start.tv_usec;

		snprintf(string, 8192, "[%02llu.%06llu] [%d] %s\n", secs, usecs,
			 getpid(), msg);

		fprintf(stderr, "%s", string);
	}

	openlog("uxlaunch", LOG_PID | LOG_CONS,
		geteuid() ? LOG_USER : LOG_USER);
	syslog(LOG_NOTICE, "%s", msg);
	closelog();
}


void log_environment(void)
{
	/* stub with debugging disabled */
#ifdef DEBUG
	char **env;

	env = environ;
	dprintf("--- environment variable dump:");
	while (*env) {
		dprintf("    %s", *env);
		env++;
	}
	dprintf("---");
#endif
}
