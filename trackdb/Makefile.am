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
	TocParser.g		\
	TempFileManager.cc	\
	FormatConverter.cc	\
	TempFileManager.h	\
	FormatConverter.h	\
	Cue2Toc.cc		\
	Cue2Toc.h		\
	CueParser.h		\
	CueParser.cc		\
	log.h			\
	log.cc

PCCTS_GEN_FILES = \
	TocParser.cpp		\
	TocParserGram.cpp	\
	TocLexerBase.cpp	\
	TocLexerBase.h		\
	TocParserGram.h		\
	TocParserTokens.h	\
	AParser.cpp		\
	DLexerBase.cpp		\
	ATokenBuffer.cpp

nodist_libtrackdb_a_SOURCES = ${PCCTS_GEN_FILES}

AM_CPPFLAGS = -I@pcctsinc@
ANTLR = @antlr_path@
DLG = @dlg_path@

TocParser.cpp: $(srcdir)/TocParser.g
	$(ANTLR) -k 3 -CC  -w2 -fl TocLexer.dlg -ft TocParserTokens.h $(srcdir)/TocParser.g
	$(DLG) -C2 -CC -cl TocLexerBase TocLexer.dlg 

TocParserGram.cpp TocLexerBase.cpp: TocParser.cpp

AParser.cpp: @pcctsinc@/$@
	cp @pcctsinc@/$@ .

DLexerBase.cpp: @pcctsinc@/$@
	cp @pcctsinc@/$@ .

ATokenBuffer.cpp: @pcctsinc@/$@
	cp @pcctsinc@/$@ .

AM_CXXFLAGS = @AO_CFLAGS@

if COND_MP3
AM_CXXFLAGS += @MAD_CFLAGS@
libtrackdb_a_SOURCES += FormatMp3.cc FormatMp3.h
endif

if COND_OGG
AM_CXXFLAGS += @VORBISFILE_CFLAGS@
libtrackdb_a_SOURCES += FormatOgg.cc FormatOgg.h
endif

CLEANFILES = ${PCCTS_GEN_FILES} CueLexer.dlg TocLexer.dlg
