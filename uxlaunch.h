#ifndef __INCLUDE_GUARD_UXLAUNCH_
#define __INCLUDE_GUARD_UXLAUNCH_

#include <X11/Xauth.h>


/*
 * Target user information
 */
extern struct passwd *pass;

extern char displaydev[];
extern char displayname[];
extern char xauth_cookie_file[];
extern Xauth x_auth;
extern char user_xauth_path[];

extern int tty;
extern char session[];
extern char username[];

extern int session_pid;

extern int verbose;

extern void get_options(int argc, char **argv);
extern void set_i18n(void);
extern void setup_pam_session(void);
extern void close_pam_session(void);
extern void switch_to_user(void);
extern void set_tty(void);
extern void setup_xauth(void);
extern void start_X_server(void);
extern void wait_for_X_signal(void);
extern void start_dbus_session_bus(void);
extern void stop_dbus_session_bus(void);
extern void setup_consolekit_session(void);
extern void start_ssh_agent(void);
extern void stop_ssh_agent(void);
extern void start_gconf(void);
extern void maybe_start_screensaver(void);
extern void get_session_type(void);
extern void autostart_panels(void);
extern void autostart_desktop_files(void);
extern void do_autostart(void);
extern void start_desktop_session(void);
extern void start_bash(void);
extern void wait_for_X_exit(void);
extern void set_text_mode(void);

extern void open_log(void);
extern void lprintf(const char *, ...);
extern void log_environment(void);

extern void close_log(void);

#define NORMAL 0
#define NICE 1
#define PIN 2
#define DELAYED 4
#define BACKGROUND 8

extern void start_daemon(int flags, char *cmd, char *args);

#endif
