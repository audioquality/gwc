# Process this file with autoconf to produce a configure script.
#AC_PREREQ([2.69])
AC_INIT([gwc], [0.41.0], [dev@audioquality.org])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([tap_reverb.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([m], [cos], [#], [echo "ERROR: missing library"; exit 1])
AC_CHECK_LIB([pthread], [main], [#], [echo "ERROR: missing library"; exit 1])
AC_CHECK_LIB([gtk-x11-2.0], [gtk_init], [#], [echo "ERROR: missing library"; exit 1])
AC_CHECK_LIB([fftw3], [fftw_execute], [#], [echo "ERROR: missing library"; exit 1])
AC_DEFINE([HAVE_FFTW3], [1], [Use FFTW3])
AC_CHECK_LIB([asound], [snd_pcm_open], [#], [echo "ERROR: missing library"; exit 1])
AC_DEFINE([HAVE_ALSA], [1], [Use ALSA])
AC_CHECK_LIB([sndfile], [sf_open], [#], [echo "ERROR: missing library"; exit 1])
AC_CHECK_LIB([meschach], [v_get], [#], [echo "ERROR: missing library"; exit 1])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FSEEKO
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([ftime gettimeofday clock_gettime memmove memset pow sqrt mkfifo strcasecmp strerror strrchr])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([alsa/asoundlib.h fcntl.h float.h limits.h malloc.h math.h memory.h \
 meschach/matrix.h sndfile.h stddef.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Set compiler flags
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)
DEPS_CFLAGS=$(pkg-config --cflags gtk+-2.0 alsa fftw3 sndfile)
DEPS_LIBS="$(pkg-config    --libs gtk+-2.0 alsa fftw3 sndfile) -lm -lmeschach"

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
