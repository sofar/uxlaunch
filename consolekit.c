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

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>

#include "uxlaunch.h"

#include <dbus/dbus.h>
#include <ck-connector.h>

static CkConnector *connector = NULL;


/*
 * Set up a ConsoleKit session. This is as easy as calling
 * one ck_connector function; the tricky part is that we
 * need to pass ConsoleKit the value of our X display ( :0 )
 * as well as the /dev/ttyX we're connected to.
 * These are needed so that ConsoleKit can set the "active"
 * flag on and off as you switch between consoles or X sessions.
 *
 * The outcome of this is a cookie which we need to put in the
 * XDG_SESSION_COOKIE environment variable.
 */
void setup_consolekit_session(void)
{
	DBusError error;
	char *d = &displaydev[0];
	char *n = &displayname[0];

	connector = ck_connector_new();
	if (!connector)
		exit(EXIT_FAILURE);

	dbus_error_init(&error);

	/*
	 * Note: ck_connector_open_* require a pointer to the value,
	 * even if the value is a string. So for a string you need
	 * to pass in a address that contains a pointer to the string.
	 */
	if (!ck_connector_open_session_with_parameters(connector, &error,
						       "unix-user", &pass->pw_uid,
						       "display-device", &d,
						       "x11-display-device", &d,
						       "x11-display", &n,
						       NULL)) {
		lprintf("Error: Unable to open session with ConsoleKit: %s: %s\n",
			error.name, error.message);
		return;
	}

	/*
	 * put the session cookie up as an environment variable
	 */
	setenv("XDG_SESSION_COOKIE", ck_connector_get_cookie(connector), 1);
}


/*
 * Undo the effects of setup_consolekit_sessions on shutdown
 */
void close_consolekit_session(void)
{
	DBusError error;

	dbus_error_init(&error);
	if (connector)
		ck_connector_close_session(connector, &error);

	unsetenv("XDG_SESSION_COOKIE");
}
