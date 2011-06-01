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

#include "uxlaunch.h"


extern char **environ;

static int first_time = 1;
static int logfile_enabled = 1;

struct timeval start;

static FILE *log;


void open_log(const char *logfile)
{
	int ret;

	/* retain any old logfile if there exists one */
	ret = system("/bin/mv " LOGFILE " " LOGBACKUPFILE);

	/* truncate log */
	log = fopen(LOGFILE, "w");
	if (!logfile)
		log = stdout;
	else
		log = fopen(logfile, "w");
	if (!log)
		logfile_enabled = 0;
}

void close_log()
{
	logfile_enabled = 0;
	fclose(log);
}


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

	gettimeofday(&current, NULL);

	secs = current.tv_sec - start.tv_sec;

	while (current.tv_usec < start.tv_usec) {
		secs --;
		current.tv_usec += 1000000;
	}
	usecs = current.tv_usec - start.tv_usec;

	va_start(args, fmt);
	vsnprintf(msg, 8192, fmt, args);
	va_end(args);

	if (msg[strlen(msg) - 1] == '\n')
		msg[strlen(msg) - 1] = '\0';

	snprintf(string, 8192, "[%02llu.%06llu] [%d] %s\n", secs, usecs, getpid(), msg);

	if (verbose)
		fprintf(stderr, "%s", string);

	if (!logfile_enabled && !verbose)
		fprintf(stderr, "%s", string);
		return;

	if (log) {
		fputs(string, log);
		fflush(log);
	} else {
		logfile_enabled = 0;
		lprintf("unable to write logfile, file logging disabled");
	}
}


void log_environment(void)
{
	char **env;

	env = environ;

	lprintf("--- environment variable dump:");
	while (*env) {
		lprintf("    %s", *env);
		env++;
	}
	lprintf("---");
}
