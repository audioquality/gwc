# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([asound], [snd_pcm_open])
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([meschach], [v_get])
AC_CHECK_LIB([gtk-x11-2.0], [gtk_init])
AC_CHECK_LIB([gdk-x11-2.0], [gtk_init])
AC_CHECK_LIB([fftw3], [fftw_execute])
AC_CHECK_LIB([sndfile], [sf_open])
AC_CHECK_LIB([pthread], [main])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_FSEEKO
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ftime gettimeofday clock_gettime memmove memset mkfifo pow sqrt strcasecmp strerror strrch])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([alsa/asoundlib.h fcntl.h float.h limits.h malloc.h memory.h sndfile.h stddef.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
