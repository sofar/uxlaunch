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

#include "uxlaunch.h"

static int ssh_agent_pid;

/*
 * ssh-agent prints a bunch of env variables to its stdout that we need to put in
 * the environment.
 */
void start_ssh_agent(void)
{
	FILE *file;
	char line[4096];

	memset(line, 0, 4096);

	file = popen("/usr/bin/ssh-agent", "r");
	if (!file) {
		lprintf("Failed to start ssh-agent");
		return;
	}
	/*
	 * ssh-agent output looks like this:
	 *
	 * SSH_AUTH_SOCK=/tmp/ssh-ccZMs16230/agent.16230; export SSH_AUTH_SOCK;
	 * SSH_AGENT_PID=16231; export SSH_AGENT_PID;
	 * echo Agent pid 16231;
	 *
	 * so search for "; export", cut that off. Then split at the = for env var name
	 * and value.
	 */
	while (!feof(file)) {
		char *c;
		if (fgets(line, 4095, file)==NULL)
			break;
		c = strstr(line, "; export");
		if (c) {
			char *c2;
			*c = 0;
			c2 = strchr(line, '=');
			if (c2) {
				*c2 = 0;
				c2++;
				setenv(line, c2, 1);
				/* store PID for later */
				if (!strcmp(line, "SSH_AGENT_PID"))
					ssh_agent_pid = atoi(c2);
			}
		}
	}
	pclose(file);
	log_environment();
}

void stop_ssh_agent(void)
{
	kill(ssh_agent_pid, SIGTERM);
}

/*
 * helper function to make debug easier
 */
void start_bash(void)
{
	int ret;

	fprintf(stderr, "Starting bash shell -- type exit to continue\n");
	ret = system("/bin/bash");
	if (ret != EXIT_SUCCESS)
		lprintf("bash returned an error");
}


/*
 * We want to start gconf early, by hand, so that it can start processing the
 * XML well before someone needs it to cut down the total time
 */
void start_gconf(void)
{
	int ret;

	ret = system("gconftool-2 --spawn");
	if (ret)
		lprintf("failed to start gconftool-2: %d", ret);
}

/*
 * Stop gconfd to save gconf keys before shutdown
 */
void stop_gconf(void)
{
	int ret;

	ret = system("gconftool-2 --shutdown");
	if (ret)
		lprintf("failed to shut down gconf %d", ret);
}

void init_screensaver(int lock_now)
{
	int ret;


	if (lock_now) {
		ret = system("/usr/bin/gnome-screensaver");
		if (ret)
			lprintf("failed to launch /usr/bin/gnome-screensaver");
		ret = system("/usr/bin/gnome-screensaver-command --lock --poke");
		if (ret)
			lprintf("failed to launch /usr/bin/gnome-screensaver-command --lock --poke");
	} else {
		/* the screensaver becomes a daemon .. but we don't need it right away */
		ret = system("/usr/bin/gnome-screensaver &");
		if (ret)
			lprintf("failed to launch /usr/bin/gnome-screensaver");
	}
}


/*
 * Start the background screensaver daemon.
 * Also, if /etc/sysconfig/lock-screen exists,
 * start with the screen locked.
 *
 * We do this instead of using a login screen, for the
 * cases that people want a password on their device.
 */
void maybe_start_screensaver(void)
{
	char home_path[4097];
	sprintf(home_path, "%s/.config/lock-screen", pass->pw_dir);
	if (!access("/etc/sysconfig/lock-screen", R_OK) ||
	    !access(home_path, R_OK)) {
		init_screensaver(1);
	} else {
		init_screensaver(0);
	}
}
