#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([uxlaunch],[0.63],[auke-jan.h.kok@intel.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_SRCDIR([src/uxlaunch.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lXau':
AC_CHECK_LIB([Xau], [main], , AC_MSG_ERROR([libXau is required but was not found]))
# FIXME: Replace `main' with a function in `-lpam':
AC_CHECK_LIB([pam], [main], , AC_MSG_ERROR([libpam is required but was not found]))
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main], , AC_MSG_ERROR([libpthread is required but was not found]))
# FIXME: Replace `main' with a function in `-lrt':
AC_CHECK_LIB([rt], [main], , AC_MSG_ERROR([librt is required but was not found]))

# PkgConfig tests
PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([CONSOLEKIT], [ck-connector >= 0.4])
PKG_CHECK_MODULES([GLIB2], [glib-2.0])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime gettimeofday memset mkdir setenv strchr strdup strerror strrchr strstr uname])

AC_OUTPUT

echo
echo "Build configuration:"
echo
echo -n " * ConsoleKit : "
test -n "$CONSOLEKIT_LIBS" && echo yes || echo no
echo -n " * Pam        : "
test -n "`echo $LIBS | grep lpam`" && echo yes || echo no
echo -n " * Xauth      : "
test -n "`echo $LIBS | grep lXau`" && echo yes || echo no
echo
