/*
 * CueParserGram: P a r s e r  H e a d e r 
 *
 * Generated from: ./CueParser.g
 *
 * Terence Parr, Russell Quong, Will Cohen, and Hank Dietz: 1989-1999
 * Parr Research Corporation
 * with Purdue University Electrical Engineering
 * with AHPCRC, University of Minnesota
 * ANTLR Version 1.33MR20
 */

#ifndef CueParserGram_h
#define CueParserGram_h

#ifndef ANTLR_VERSION
#define ANTLR_VERSION 13320
#endif

#include "AParser.h"


#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "Toc.h"
#include "TrackData.h"
#include "util.h"
struct CueTrackData;
class CueParserGram : public ANTLRParser {
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
	static SetWordType err1[8];
	static SetWordType err2[8];
	static SetWordType err3[8];
	static SetWordType err4[8];
	static SetWordType setwd1[38];
private:
	void zzdflthandlers( int _signal, int *_retsignal );

public:

struct _rv3 {
	int i;
	int lineNr  ;
};
	CueParserGram(ANTLRTokenBuffer *input);
	  int    cue(  CueTrackData *trackData  );
	  char *   string(void);
	struct _rv3 integer(void);
	  Msf    msf(void);
	  TrackData::Mode    trackMode(void);
};

#endif /* CueParserGram_h */
