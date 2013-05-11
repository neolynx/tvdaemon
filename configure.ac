#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([tvdaemon], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/tvdaemon.cpp])
AC_CONFIG_HEADERS([include/config.h])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AC_CHECK_LIB([rt], [main])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([udev], [udev_new],, AC_MSG_ERROR(libudev-dev not found))
AC_CHECK_LIB([ebml], [main],, AC_MSG_ERROR(libebml-dev not found))
AC_CHECK_LIB([matroska], [main],, AC_MSG_ERROR(libmatroska-dev not found))


# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for libraries.
PKG_CHECK_MODULES([LIBCONFIGXX], [libconfig++ >= 1.3.2],, AC_MSG_ERROR([libconfig++8-dev 1.3.2 or newer not found.]))
PKG_CHECK_MODULES([LIBJSONXX], [json >= 0.9],, AC_MSG_ERROR([libjson0-dev 0.9 or newer not found.]))
PKG_CHECK_MODULES([LIBCCRTP], [libccrtp >= 2.0.3],, AC_MSG_ERROR([libccrtp-dev 2.0.3 or newer not found.]))

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime memset mkdir select socket strcasecmp strdup strtoul])

AC_CONFIG_FILES([Makefile
                 lib/Makefile
                 src/Makefile])
AC_OUTPUT

