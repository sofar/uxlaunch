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
#include <signal.h>

#include <dbus/dbus.h>

#include "uxlaunch.h"

static char dbus_pid[255];
static char dbus_address[255];


void start_dbus_session_bus(void)
{
	int p_fd[2];
	int a_fd[2];
	char cmd[255];
	int ret;
	ssize_t result;

	d_in();

	if (pipe(p_fd) < 0)
		exit(EXIT_FAILURE);
	if (pipe(a_fd) < 0) {
		close(p_fd[0]);
		close(p_fd[1]);
		exit(EXIT_FAILURE);
	}

	snprintf(cmd, 254, "dbus-daemon --fork --session --print-pid %d --print-address %d",
		p_fd[1], a_fd[1]);

	lprintf("launching session bus: %s", cmd);

	ret = system(cmd);
	if (!ret)
		lprintf("Error starting session dbus-daemon... dbus may not be functional");

	close(p_fd[1]);
	close(a_fd[1]);

	result = read(p_fd[0], dbus_pid, sizeof(dbus_pid));
	if (result <= 0)
		lprintf("Error reading from dbus pipes... dbus may not be functional");
	result = read(a_fd[0], dbus_address, sizeof(dbus_address));
	if (result <= 0)
		lprintf("Error reading from dbus pipes... dbus may not be functional");

	/* chomp() */
	dbus_pid[strlen(dbus_pid) - 1] = '\0';
	dbus_address[strlen(dbus_address) - 1] = '\0';

	close(p_fd[0]);
	close(a_fd[0]);

	setenv("DBUS_SESSION_BUS_PID", dbus_pid, 1);
	setenv("DBUS_SESSION_BUS_ADDRESS", dbus_address, 1);
	d_out();
}

void stop_dbus_session_bus(void)
{
	d_in();
	kill(atoi(dbus_pid), SIGTERM);
	unsetenv("DBUS_SESSION_BUS_PID");
	unsetenv("DBUS_SESSION_BUS_ADDRESS");
	d_out();
}
