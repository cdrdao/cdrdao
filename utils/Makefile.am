if COND_TOC2MP3
MAYBE_TOC2MP3 = toc2mp3
else
MAYBE_TOC2MP3 = 
endif

bin_PROGRAMS = toc2cue $(MAYBE_TOC2MP3)

toc2cue_SOURCES = toc2cue.cc
toc2mp3_SOURCES = toc2mp3.cc

toc2cue_LDADD = -L$(top_builddir)/trackdb -ltrackdb
toc2mp3_LDADD = -L$(top_builddir)/trackdb -ltrackdb @LAME_LIBS@

toc2mp3_CXXFLAGS = @LAME_CFLAGS@

INCLUDES = -I$(top_builddir)/trackdb

