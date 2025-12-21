/*
 * TocParserGram: P a r s e r  H e a d e r
 *
 * Generated from: TocParser.g
 *
 * Terence Parr, Russell Quong, Will Cohen, and Hank Dietz: 1989-2001
 * Parr Research Corporation
 * with Purdue University Electrical Engineering
 * with AHPCRC, University of Minnesota
 * ANTLR Version 1.33MR32
 */

#ifndef TocParserGram_h
#define TocParserGram_h

#ifndef ANTLR_VERSION
#define ANTLR_VERSION 13332
#endif

#include "AParser.h"

#include "CdTextItem.h"
#include "Toc.h"
#include "log.h"
#include "util.h"
#include <config.h>
#include <memory>
#include <stdlib.h>
class TocParserGram : public ANTLRParser
{
  public:
    static const ANTLRChar *tokenName(int tk);
    enum {
        SET_SIZE = 79
    };

  protected:
    static const ANTLRChar *_token_tbl[];

  private:
  public:
    const char *filename_;
    int error_;

    void syn(_ANTLRTokenPtr tok, ANTLRChar *egroup, SetWordType *eset, ANTLRTokenType etok, int k);

  protected:
    static SetWordType err1[12];
    static SetWordType err2[12];
    static SetWordType err3[12];
    static SetWordType setwd1[79];
    static SetWordType err4[12];
    static SetWordType err5[12];
    static SetWordType err6[12];
    static SetWordType err7[12];
    static SetWordType setwd2[79];
    static SetWordType err8[12];
    static SetWordType err9[12];
    static SetWordType err10[12];
    static SetWordType AudioFile_set[12];
    static SetWordType AudioFile_errset[12];
    static SetWordType err13[12];
    static SetWordType err14[12];
    static SetWordType err15[12];
    static SetWordType err16[12];
    static SetWordType err17[12];
    static SetWordType setwd3[79];
    static SetWordType err18[12];
    static SetWordType err19[12];
    static SetWordType err20[12];
    static SetWordType err21[12];
    static SetWordType err22[12];
    static SetWordType setwd4[79];
    static SetWordType err23[12];
    static SetWordType err24[12];
    static SetWordType err25[12];
    static SetWordType err26[12];
    static SetWordType err27[12];
    static SetWordType setwd5[79];
    static SetWordType err28[12];
    static SetWordType err29[12];
    static SetWordType err30[12];
    static SetWordType err31[12];
    static SetWordType err32[12];
    static SetWordType err33[12];
    static SetWordType err34[12];
    static SetWordType setwd6[79];
    static SetWordType err35[12];
    static SetWordType err36[12];
    static SetWordType setwd7[79];

  private:
    void zzdflthandlers(int _signal, int *_retsignal);

  public:
    struct _rv2 {
        Track *tr;
        int lineNr;
    };

    struct _rv3 {
        SubTrack *st;
        int lineNr;
    };

    struct _rv5 {
        std::string ret;
        bool is_utf8;
    };

    struct _rv8 {
        int i;
        int lineNr;
    };

    struct _rv16 {
        CdTextItem::PackType t;
        int lineNr;
    };

    struct _rv17 {
        const unsigned char *data;
        long len;
    };

    struct _rv18 {
        CdTextItem *item;
        int lineNr;
    };
    TocParserGram(ANTLRTokenBuffer *input);
    Toc *toc(void);
    struct _rv2 track(void);
    struct _rv3 subTrack(TrackData::Mode trackType, TrackData::SubChannelMode subChanType);
    std::string string(void);
    struct _rv5 stringEmpty(void);
    unsigned long uLong(void);
    long sLong(void);
    struct _rv8 integer(void);
    Msf msf(void);
    unsigned long samples(void);
    unsigned long dataLength(TrackData::Mode mode, TrackData::SubChannelMode sm);
    TrackData::Mode dataMode(void);
    TrackData::Mode trackMode(void);
    TrackData::SubChannelMode subChannelMode(void);
    Toc::Type tocType(void);
    struct _rv16 packType(void);
    struct _rv17 binaryData(void);
    struct _rv18 cdTextItem(int blockNr);
    void cdTextBlock(CdTextContainer &container, int isTrack);
    void cdTextLanguageMap(CdTextContainer &container);
    void cdTextTrack(CdTextContainer &container);
    void cdTextGlobal(CdTextContainer &container);
};

#endif /* TocParserGram_h */
