if COND_TOC2MP3
MAYBE_TOC2MP3 = toc2mp3
else
MAYBE_TOC2MP3 = 
endif

bin_PROGRAMS = toc2cue cue2toc $(MAYBE_TOC2MP3)

toc2cue_SOURCES = toc2cue.cc
cue2toc_SOURCES = main.c cue2toc.c cue2toc.h timecode.c timecode.h
toc2mp3_SOURCES = toc2mp3.cc

toc2cue_LDADD = -L$(top_builddir)/trackdb -ltrackdb
toc2mp3_LDADD = -L$(top_builddir)/trackdb -ltrackdb @LAME_LIBS@

toc2mp3_CXXFLAGS = @LAME_CFLAGS@

INCLUDES = -I$(top_builddir)/trackdb

man1_MANS = cue2toc.1
EXTRA_DIST = $(man1_MANS) toc2cddb.cc
