#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(clocksync, 1.0, jonathan@impactstudiopro.com)
AC_CONFIG_SRCDIR([libclocksync.c])
dnl AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_mutex_init])
AC_CHECK_LIB([dl], [dlopen])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_TYPE_UINT32_T
AC_C_VOLATILE

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MMAP
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([ftruncate gettimeofday memset munmap select socket strerror])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
