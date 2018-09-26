/*
 * This file is part of uxlaunch
 *
 * (C) Copyright 2009 Intel Corporation
 * Authors:
 *     Auke Kok <auke@linux.intel.com>
 *     Arjan van de Ven <arjan@linux.intel.com>
 *     Michael Meeks <michael.meeks@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <security/pam_appl.h>

#include "uxlaunch.h"

static pam_handle_t *ph;
static struct pam_conv pc;

/*
 * Sometimes PAM likes to chat with you, before it is assured
 * enough to let you log-in: fun.
 */
static int
pam_conversation_fn(int msg_count,
		    const struct pam_message **messages,
		    struct pam_response      **responses,
		    void *user_data)
{
	int i;
	(void)user_data;

	d_in();

	lprintf("pam conversation with %d messages", msg_count);
	if (responses)
		*responses = NULL;

	if (msg_count < 1) /* ping */
		return PAM_SUCCESS;

	/* otherwise find any helpful data we can to print, and bail */
	if (!responses || !messages) {
		lprintf("pam conversation with no message, or response");
		return PAM_CONV_ERR;
	}
	*responses = calloc (msg_count, sizeof (struct pam_response));
	for (i = 0; i < msg_count; i++) {
		const struct pam_message *msg = messages[i];

		if (msg->msg_style == PAM_TEXT_INFO)
			lprintf("pam chats to us: '%s'", msg->msg);
		else if (msg->msg_style == PAM_ERROR_MSG)
			lprintf("Error: pam error msg '%s'", msg->msg);
		else
			lprintf("pam message %d style %d: '%s'",
				 i, msg->msg_style, msg->msg);
		(*responses)[i].resp = NULL;
		(*responses)[i].resp_retcode = PAM_SUCCESS;
	}

	d_out();
	return PAM_SUCCESS;
}

/*
 * Creating a PAM session. We need a pam "login" session so that the dbus
 * "at_console" logic will work correctly, as well as various /dev file
 * permissions.
 *
 * for pam_console to work we need to set the PAM_TTY and PAM_XDISPLAY variables,
 * before we open the session. "PAM_TTY" takes input in the form "ttyX", without
 * the /dev prefix, so we need to construct that in place here.
 */
void setup_pam_session(void)
{
	char x[256];
	int err;

	d_in();

	snprintf(x, 256, "tty%d", tty);

	pc.conv = pam_conversation_fn;
	pc.appdata_ptr = NULL;

	/*
	 * it's possible that /var/run/console does not exist, which is fine. for PAM
	 * to run without complaining, let's just create it on the fly.
	 */
	(void) mkdir("/var/run/console", 0755);

	err = pam_start("login", pass->pw_name, &pc, &ph);

	err = pam_set_item(ph, PAM_TTY, &x);
	if (err != PAM_SUCCESS) {
		lprintf("pam_set_item PAM_TTY returned %d: %s\n", err, pam_strerror(ph, err));
		exit(EXIT_FAILURE);
	}

	err = pam_set_item(ph, PAM_XDISPLAY, &displayname);
	if (err != PAM_SUCCESS) {
		lprintf("pam_set_item PAM_DISPLAY returned %d: %s\n", err, pam_strerror(ph, err));
		exit(EXIT_FAILURE);
	}

	err = pam_open_session(ph, 0);
	if (err != PAM_SUCCESS) {
		lprintf("pam_open_session returned %d: %s\n", err, pam_strerror(ph, err));
		exit(EXIT_FAILURE);
	}
	d_out();
}

void close_pam_session(void)
{
	int err;

	d_in();

	err = pam_close_session(ph, 0);
	if (err)
		lprintf("pam_close_session returned %d: %s\n", err, pam_strerror(ph, err));
	pam_end(ph, err);
	d_out();
}
