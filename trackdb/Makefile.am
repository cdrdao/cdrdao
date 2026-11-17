noinst_LIBRARIES = libtrackdb.a

libtrackdb_a_SOURCES = \
	Cddb.cc			\
	lec.cc			\
	Toc.cc			\
	TrackDataList.cc	\
	CdTextContainer.cc	\
	Msf.cc			\
	Track.cc		\
	util.cc			\
	CdTextItem.cc		\
	CdTextEncoder.cc	\
	CdTextEncoder.h		\
	SubTrack.cc		\
	TrackData.cc		\
	Cddb.h			\
	CdTextContainer.h	\
	CdTextItem.h		\
	lec.h			\
	Msf.h			\
	Sample.h		\
	SubTrack.h		\
	Toc.h			\
	TrackData.h		\
	TrackDataList.h		\
	Track.h			\
	util.h			\
	TempFileManager.cc	\
	FormatConverter.cc	\
	TempFileManager.h	\
	FormatConverter.h	\
	Cue2Toc.cc		\
	Cue2Toc.h		\
	CueParser.h		\
	CueParser.cc		\
	log.h			\
	log.cc			\
	TocParser.cc		\
	TocParser.h


AM_CXXFLAGS = @AO_CFLAGS@

if COND_AO
libtrackdb_a_SOURCES += FormatSupport.cc FormatSupport.h
endif

if COND_MP3
AM_CXXFLAGS += @MAD_CFLAGS@
libtrackdb_a_SOURCES += FormatMp3.cc FormatMp3.h
endif

if COND_OGG
AM_CXXFLAGS += @VORBISFILE_CFLAGS@
libtrackdb_a_SOURCES += FormatOgg.cc FormatOgg.h
endif

if COND_FLAC
AM_CXXFLAGS += @FLAC_CFLAGS@ @SAMPLERATE_CFLAGS@
libtrackdb_a_SOURCES += FormatFlac.cc FormatFlac.h
endif
