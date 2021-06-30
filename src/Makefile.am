AUTOMAKE_OPTIONS = foreign

# what flags you want to pass to the C compiler & linker
CFLAGS = -Wall -Wno-deprecated-declarations -g -O6 -D_FILE_OFFSET_BITS=64 -D_REENTRANT

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
bin_PROGRAMS = gwc

gwc_CFLAGS = $(DEPS_CFLAGS)
gwc_LDADD = $(DEPS_LIBS)
gwc_SOURCES = tap_reverb_file_io.c tap_reverb.c reverb.c dialog.c audio_device.c \
audio_edit.c audio_util.c preferences.c drawing.c amplify.c denoise.c undo.c \
declick.c sample_block.c decrackle.c stat.c dethunk.c i0.c i1.c chbevl.c \
markers.c soundfile.c pinknoise.c biquad.c gwc.c
