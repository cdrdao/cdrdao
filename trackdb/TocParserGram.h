/*
 * TocParserGram: P a r s e r  H e a d e r 
 *
 * Generated from: ./TocParser.g
 *
 * Terence Parr, Russell Quong, Will Cohen, and Hank Dietz: 1989-1999
 * Parr Research Corporation
 * with Purdue University Electrical Engineering
 * with AHPCRC, University of Minnesota
 * ANTLR Version 1.33MR20
 */

#ifndef TocParserGram_h
#define TocParserGram_h

#ifndef ANTLR_VERSION
#define ANTLR_VERSION 13320
#endif

#include "AParser.h"


#include <config.h>
#include <stdlib.h>
#include "Toc.h"
#include "util.h"
#include "CdTextItem.h"
class TocParserGram : public ANTLRParser {
public:
	static  const ANTLRChar *tokenName(int tk);
protected:
	static  const ANTLRChar *_token_tbl[];
private:

public:
const char *filename_;
int error_;

void syn(_ANTLRTokenPtr tok, ANTLRChar *egroup, SetWordType *eset,
ANTLRTokenType etok, int k);
protected:
	static SetWordType setwd1[68];
	static SetWordType err1[12];
	static SetWordType AudioFile_set[12];
	static SetWordType err3[12];
	static SetWordType err4[12];
	static SetWordType setwd2[68];
	static SetWordType err5[12];
	static SetWordType err6[12];
	static SetWordType err7[12];
	static SetWordType setwd3[68];
	static SetWordType err8[12];
	static SetWordType err9[12];
	static SetWordType err10[12];
	static SetWordType err11[12];
	static SetWordType err12[12];
	static SetWordType err13[12];
	static SetWordType err14[12];
	static SetWordType setwd4[68];
	static SetWordType err15[12];
	static SetWordType setwd5[68];
private:
	void zzdflthandlers( int _signal, int *_retsignal );

public:

struct _rv2 {
	Track *tr;
	int lineNr  ;
};

struct _rv3 {
	SubTrack *st;
	int lineNr  ;
};

struct _rv8 {
	int i;
	int lineNr  ;
};

struct _rv15 {
	CdTextItem::PackType t;
	int lineNr  ;
};

struct _rv16 {
	const unsigned char *data;
	long len  ;
};

struct _rv17 {
	CdTextItem *item;
	int lineNr  ;
};
	TocParserGram(ANTLRTokenBuffer *input);
	  Toc *   toc(void);
	struct _rv2 track(void);
	struct _rv3 subTrack(  TrackData::Mode trackType  );
	  char *   string(void);
	  char *   stringEmpty(void);
	  unsigned long    uLong(void);
	  long    sLong(void);
	struct _rv8 integer(void);
	  Msf    msf(void);
	  unsigned long    samples(void);
	  unsigned long    dataLength(  TrackData::Mode mode );
	  TrackData::Mode    dataMode(void);
	  TrackData::Mode    trackMode(void);
	  Toc::TocType    tocType(void);
	struct _rv15 packType(void);
	struct _rv16 binaryData(void);
	struct _rv17 cdTextItem(  int blockNr  );
	void cdTextBlock(  CdTextContainer &container, int isTrack  );
	void cdTextLanguageMap(  CdTextContainer &container  );
	void cdTextTrack(  CdTextContainer &container  );
	void cdTextGlobal(  CdTextContainer &container  );
};

#endif /* TocParserGram_h */
