noinst_LIBRARIES = libtrackdb.a
libtrackdb_a_SOURCES = Cddb.cc lec.cc Toc.cc TrackDataList.cc CdTextContainer.cc Msf.cc Track.cc util.cc CdTextItem.cc SubTrack.cc TrackData.cc TocParser.cpp TocParserGram.cpp TocLexerBase.cpp CueParser.cpp CueParserGram.cpp CueLexerBase.cpp @pcctsinc@/AParser.cpp @pcctsinc@/DLexerBase.cpp @pcctsinc@/ATokenBuffer.cpp Cddb.h CdTextContainer.h CdTextItem.h CueLexerBase.h CueParserGram.h CueParserTokens.h lec.h Msf.h Sample.h SubTrack.h Toc.h TocLexerBase.h TocParserGram.h TocParserTokens.h TrackData.h TrackDataList.h Track.h util.h CueParser.g TocParser.g

INCLUDES = -I@pcctsinc@
ANTLR = @antlr_path@
DLG = @dlg_path@

TocParser.cpp TocParserGram.cpp TocLexerBase.cpp: $(srcdir)/TocParser.g
	$(ANTLR) -k 3 -CC  -w2 -fl TocLexer.dlg -ft TocParserTokens.h $(srcdir)/TocParser.g
	$(DLG) -C2 -CC -cl TocLexerBase TocLexer.dlg 

CueParser.cpp CueParserGram.cpp CueLexerBase.cpp: $(srcdir)/CueParser.g
	$(ANTLR) -k 1 -CC  -w2 -fl CueLexer.dlg -ft CueParserTokens.h $(srcdir)/CueParser.g
	$(DLG) -C2 -CC -cl CueLexerBase CueLexer.dlg 
