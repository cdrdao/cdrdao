/*
 * A n t l r  T r a n s l a t i o n  H e a d e r
 *
 * Terence Parr, Will Cohen, and Hank Dietz: 1989-2001
 * Purdue University Electrical Engineering
 * With AHPCRC, University of Minnesota
 * ANTLR Version 1.33MR32
 *
 *   ../pccts/antlr/antlr -nopurify -k 3 -CC -w2 -fl TocLexer.dlg -ft TocParserTokens.h TocParser.g
 *
 */

#define ANTLR_VERSION	13332
#include "pcctscfg.h"
#include "pccts_stdio.h"
#include "TocParserTokens.h"

#include <config.h>
#include <stdlib.h>
#include <memory>
#include "Toc.h"
#include "util.h"
#include "log.h"
#include "CdTextItem.h"
#include "AParser.h"
#include "TocParserGram.h"
#include "DLexerBase.h"
#include "ATokPtr.h"

#include "CdTextContainer.h"
#include "TocLexerBase.h"

// Maximum length of binary data for CD-TEXT
#define MAX_CD_TEXT_DATA_LEN (256 * 12)


/* Use 'ANTLRCommonToken' as 'ANTLRToken' to be compatible with some bug
* fix releases of PCCTS-1.33. The drawback is that the token length is
* limited to 100 characters. This might be a problem for long file names.
* Define 'USE_ANTLRCommonToken' to 0 if you want to use the dynamic token
* which handles arbitrary token lengths (there'll be still a limitation in
* the lexer code but that's more than 100 characters). In this case it might
* be necessary to adjust the types of the member functions to the types in
* 'ANTLRAbstractToken' defined in PCCTSDIR/h/AToken.h'.
*/

#define USE_ANTLRCommonToken 0

#if USE_ANTLRCommonToken

typedef ANTLRCommonToken ANTLRToken;

#else

class ANTLRToken : public ANTLRRefCountToken {
private:
ANTLRTokenType type_;
int line_;
ANTLRChar *text_;

public:
ANTLRToken(ANTLRTokenType t, ANTLRChar *s)
: ANTLRRefCountToken(t, s)
{
setType(t);
line_ = 0;
setText(s);
}
ANTLRToken()
{
setType((ANTLRTokenType)0);
line_ = 0;
setText("");
}
virtual ~ANTLRToken() { delete[] text_; }

#if 0
// use this for basic PCCTS-1.33 release
ANTLRTokenType getType()	 { return type_; }
virtual int getLine()		 { return line_; }
ANTLRChar *getText()		 { return text_; }
void setText(ANTLRChar *s)     { text_ = strdupCC(s); }
#else
// use this for PCCTS-1.33 bug fix releases
ANTLRTokenType getType() const { return type_; }
virtual int getLine()    const { return line_; }
ANTLRChar *getText()	   const { return text_; }
void setText(const ANTLRChar *s)     { text_ = strdupCC(s); }
#endif

  void setType(ANTLRTokenType t) { type_ = t; }
void setLine(int line)	 { line_ = line; }

  virtual ANTLRAbstractToken *makeToken(ANTLRTokenType tt, ANTLRChar *txt,
int line)
{
ANTLRAbstractToken *t = new ANTLRToken(tt,txt);
t->setLine(line);
return t;
}
};
#endif

class TocLexer : public TocLexerBase {
  public:
  TocLexer(DLGInputStream *in, unsigned bufsize=2000)
  : TocLexerBase(in, bufsize) { parser_ = NULL; }
  
  virtual ANTLRTokenType erraction();
  
  TocParserGram *parser_;
};

Toc *
TocParserGram::toc(void)
{
  Toc *   _retv;
  zzRULE;
  _retv = new Toc;
  Track *tr = NULL;
  int lineNr = 0;
  std::string catalog;
  Toc::Type toctype;
  int firsttrack, firstLine;
  {
    ANTLRTokenPtr _t21=NULL;
    for (;;) {
      if ( !((setwd1[LA(1)]&0x1))) break;
      if ( (LA(1)==Catalog) ) {
        zzmatch(Catalog); _t21 = (ANTLRTokenPtr)LT(1); consume();
          catalog   = string();

        if (!catalog.empty()) {
          if (_retv->catalog(catalog.c_str()) != 0) {
            log_message(-2, "%s:%d: Illegal catalog number: %s.\n",
            filename_, _t21->getLine(), catalog.c_str());
            error_ = 1;
          }
        }
      }
      else {
        if ( (setwd1[LA(1)]&0x2) ) {
            toctype   = tocType();

          _retv->tocType(toctype);
        }
        else break; /* MR6 code for exiting loop "for sure" */
      }
    }
  }
  {
    if ( (LA(1)==FirstTrackNo) ) {
      zzmatch(FirstTrackNo); consume();
      { struct _rv8 _trv; _trv = integer();

      firsttrack = _trv.i; firstLine   = _trv.lineNr; }
      if (firsttrack > 0 && firsttrack < 100) {
        _retv->firstTrackNo(firsttrack);
      } else {
        log_message(-2, "%s:%d: Illegal track number: %d\n", filename_,
        firstLine, firsttrack);
        error_ = 1;
      }
    }
    else {
      if ( (setwd1[LA(1)]&0x4)
 ) {
      }
      else {FAIL(1,err1,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  {
    if ( (LA(1)==CdText) ) {
      cdTextGlobal(  _retv->cdtext_  );
    }
    else {
      if ( (LA(1)==TrackDef) ) {
      }
      else {FAIL(1,err2,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  {
    int zzcnt=1;
    do {
      { struct _rv2 _trv; _trv = track();

      tr = _trv.tr; lineNr   = _trv.lineNr; }
      if (tr != NULL) {
        if (_retv->append(tr) != 0) {
          log_message(-2, "%s:%d: First track must not have a pregap.\n",
          filename_, lineNr);
          error_ = 1;
        }
        delete tr, tr = NULL;
      }
    } while ( (LA(1)==TrackDef) );
  }
  _retv->enforceTextEncoding();
  zzmatch(Eof); consume();
  return _retv;
fail:
  delete _retv, _retv = NULL;   
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd1, 0x8);
  return _retv;
}

TocParserGram::_rv2
TocParserGram::track(void)
{
  struct _rv2 _retv;
  zzRULE;
  ANTLRTokenPtr _t11=NULL;
  _retv.tr = NULL;
  _retv.lineNr = 0;
  SubTrack *st = NULL;
  std::string isrcCode;
  TrackData::Mode trackType;
  TrackData::SubChannelMode subChanType = TrackData::SUBCHAN_NONE;
  Msf length;
  Msf indexIncrement;
  Msf pos;
  int posGiven = 0;
  Msf startPos; // end of pre-gap
  Msf endPos;   // start if post-gap
  int startPosLine = 0;
  int endPosLine = 0;
  int lineNr = 0;
  int flag = 1;
  zzmatch(TrackDef); _t11 = (ANTLRTokenPtr)LT(1);
  _retv.lineNr = _t11->getLine();
 consume();
    trackType   = trackMode();

  {
    if ( (setwd1[LA(1)]&0x10) ) {
        subChanType   = subChannelMode();

    }
    else {
      if ( (setwd1[LA(1)]&0x20)
 ) {
      }
      else {FAIL(1,err3,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  _retv.tr = new Track(trackType, subChanType);
  {
    ANTLRTokenPtr _t21=NULL;
    for (;;) {
      if ( !((setwd1[LA(1)]&0x40))) break;
      if ( (LA(1)==Isrc) ) {
        zzmatch(Isrc); _t21 = (ANTLRTokenPtr)LT(1); consume();
          isrcCode   = string();

        if (!isrcCode.empty()) {
          if (_retv.tr->isrc(isrcCode.c_str()) != 0) {
            log_message(-2, "%s:%d: Illegal ISRC code: %s.\n",
            filename_, _t21->getLine(), isrcCode.c_str());
            error_ = 1;
          }
        }
      }
      else {
        if ( (setwd1[LA(1)]&0x80) && (setwd2[LA(2)]&0x1) && (setwd2[LA(3)]&0x2) && !(
 LA(1)==No && LA(2)==PreEmphasis
      && ( LA(3)==CdText || LA(3)==Pregap || LA(3)==29 || LA(3)==DataFile
         || LA(3)==Fifo || LA(3)==Silence || LA(3)==Zero || LA(3)==Start
         || LA(3)==End || LA(3)==Isrc || LA(3)==No || LA(3)==Copy
         || LA(3)==PreEmphasis || LA(3)==TwoChannel || LA(3)==FourChannel)
) ) {
          {
            if ( (LA(1)==No)
 ) {
              zzmatch(No);
              flag = 0;
 consume();
            }
            else {
              if ( (LA(1)==Copy) ) {
              }
              else {FAIL(1,err4,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
            }
          }
          zzmatch(Copy);
          _retv.tr->copyPermitted(flag); flag = 1;
 consume();
        }
        else {
          if ( (setwd2[LA(1)]&0x4) && (setwd2[LA(2)]&0x8) && (setwd2[LA(3)]&0x10) ) {
            {
              if ( (LA(1)==No) ) {
                zzmatch(No);
                flag = 0;
 consume();
              }
              else {
                if ( (LA(1)==PreEmphasis)
 ) {
                }
                else {FAIL(1,err5,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
              }
            }
            zzmatch(PreEmphasis);
            _retv.tr->preEmphasis(flag); flag = 1;
 consume();
          }
          else {
            if ( (LA(1)==TwoChannel) ) {
              zzmatch(TwoChannel); _t21 = (ANTLRTokenPtr)LT(1);
              _retv.tr->audioType(0);
 consume();
            }
            else {
              if ( (LA(1)==FourChannel) ) {
                zzmatch(FourChannel); _t21 = (ANTLRTokenPtr)LT(1);
                _retv.tr->audioType(1);
 consume();
              }
              else break; /* MR6 code for exiting loop "for sure" */
            }
          }
        }
      }
    }
  }
  {
    if ( (LA(1)==CdText) ) {
      cdTextTrack(  _retv.tr->cdtext_  );
    }
    else {
      if ( (setwd2[LA(1)]&0x20) ) {
      }
      else {FAIL(1,err6,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  {
    ANTLRTokenPtr _t21=NULL;
    if ( (LA(1)==Pregap)
 ) {
      zzmatch(Pregap); _t21 = (ANTLRTokenPtr)LT(1); consume();
        length   = msf();

      if (length.lba() == 0) {
        log_message(-2, "%s:%d: Length of pregap is zero.\n",
        filename_, _t21->getLine());
        error_ = 1;
      }
      else {
        if (trackType == TrackData::AUDIO) {
          _retv.tr->append(SubTrack(SubTrack::DATA,
          TrackData(length.samples())));
        }
        else {
          _retv.tr->append(SubTrack(SubTrack::DATA,
          TrackData(trackType, subChanType,
          length.lba() * TrackData::dataBlockSize(trackType, subChanType))));
        }
        startPos = _retv.tr->length();
        startPosLine = _t21->getLine();
      }
    }
    else {
      if ( (setwd2[LA(1)]&0x40) ) {
      }
      else {FAIL(1,err7,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  {
    ANTLRTokenPtr _t21=NULL;
    int zzcnt=1;
    do {
      if ( (setwd2[LA(1)]&0x80) ) {
        { struct _rv3 _trv; _trv = subTrack(  trackType, subChanType  );

        st = _trv.st; lineNr   = _trv.lineNr; }
        
        if (st != NULL && _retv.tr->append(*st) == 2) {
          log_message(-2,
          "%s:%d: Mixing of FILE/AUDIOFILE/SILENCE and DATAFILE/ZERO statements not allowed.", filename_, lineNr);
          log_message(-2,
          "%s:%d: PREGAP acts as SILENCE in audio tracks.",
          filename_, lineNr);
          error_ = 1;
        }
        
          delete st, st = NULL;
      }
      else {
        if ( (LA(1)==Start) ) {
          zzmatch(Start); _t21 = (ANTLRTokenPtr)LT(1);
          posGiven = 0;
 consume();
          {
            if ( (LA(1)==Integer) ) {
                pos   = msf();

              posGiven = 1;
            }
            else {
              if ( (setwd3[LA(1)]&0x1)
 ) {
              }
              else {FAIL(1,err8,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
            }
          }
          if (startPosLine != 0) {
            log_message(-2,
            "%s:%d: Track start (end of pre-gap) already defined.\n",
            filename_, _t21->getLine());
            error_ = 1;
          }
          else {
          if (!posGiven) {
          pos = _retv.tr->length(); // retrieve current position
        }
        startPos = pos;
        startPosLine = _t21->getLine();
      }
      pos = Msf(0);
        }
        else {
          if ( (LA(1)==End) ) {
            zzmatch(End); _t21 = (ANTLRTokenPtr)LT(1);
            posGiven = 0;
 consume();
            {
              if ( (LA(1)==Integer) ) {
                  pos   = msf();

                posGiven = 1;
              }
              else {
                if ( (setwd3[LA(1)]&0x2) ) {
                }
                else {FAIL(1,err9,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
              }
            }
            if (endPosLine != 0) {
              log_message(-2,
              "%s:%d: Track end (start of post-gap) already defined.\n",
              filename_, _t21->getLine());
              error_ = 1;
            }
            else {
            if (!posGiven) {
            pos = _retv.tr->length(); // retrieve current position
          }
          endPos = pos;
          endPosLine = _t21->getLine();
        }
        pos = Msf(0);
          }
          /* MR10 ()+ */ else {
            if ( zzcnt > 1 ) break;
            else {FAIL(1,err10,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
          }
        }
      }
      zzcnt++;
    } while ( 1 );
  }
  if (startPosLine != 0 && _retv.tr->start(startPos) != 0) {
    log_message(-2,
    "%s:%d: START %s behind or at track end.\n", filename_,
    startPosLine, startPos.str());
    error_ = 1;
  }
  {
    ANTLRTokenPtr _t21=NULL;
    while ( (LA(1)==Index) ) {
      zzmatch(Index); _t21 = (ANTLRTokenPtr)LT(1); consume();
        indexIncrement   = msf();

      if (_retv.tr != NULL) {
        switch (_retv.tr->appendIndex(indexIncrement)) {
          case 1:
          log_message(-2, "%s:%d: More than 98 index increments.\n",
          filename_, _t21->getLine());
          error_ = 1;
          break;
          
           case 2:
          log_message(-2, "%s:%d: Index beyond track end.\n",
          filename_,  _t21->getLine());
          error_ = 1;
          break;
          
           case 3:
          log_message(-2, "%s:%d: Index at start of track.\n",
          filename_,  _t21->getLine());
          error_ = 1;
          break;
        }
      }
    }
  }
  if (endPosLine != 0) {
    switch (_retv.tr->end(endPos)) {
      case 1:
      log_message(-2, "%s:%d: END %s behind or at track end.\n",
      filename_, endPosLine, endPos.str());
      error_ = 1;
      break;
      case 2:
      log_message(-2, "%s:%d: END %s within pre-gap.\n",
      filename_, endPosLine, endPos.str());
      error_ = 1;
      break;
      case 3:
      log_message(-2,
      "%s:%d: END %s: Cannot create index mark for post-gap.\n",
      filename_, endPosLine, endPos.str());
      error_ = 1;
      break;
    }
  }
  return _retv;
fail:
  delete _retv.tr, _retv.tr = NULL;
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd3, 0x4);
  return _retv;
}

TocParserGram::_rv3
TocParserGram::subTrack(TrackData::Mode trackType,TrackData::SubChannelMode subChanType)
{
  struct _rv3 _retv;
  zzRULE;
  _retv.st = NULL;
  _retv.lineNr = 0;
  std::string filename;
  unsigned long start = 0;
  unsigned long len = 0;
  long offset = 0;
  TrackData::Mode dMode;
  int swapSamples = 0;
  {
    ANTLRTokenPtr _t21=NULL;
    if ( (setwd3[LA(1)]&0x8)
 ) {
      zzsetmatch(AudioFile_set, AudioFile_errset); _t21 = (ANTLRTokenPtr)LT(1); consume();
        filename   = string();

      {
        if ( (LA(1)==Swap) ) {
          zzmatch(Swap);
          swapSamples = 1;
 consume();
        }
        else {
          if ( (setwd3[LA(1)]&0x10) ) {
          }
          else {FAIL(1,err13,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
        }
      }
      {
        if ( (LA(1)==74) ) {
          zzmatch(74); consume();
            offset   = sLong();

        }
        else {
          if ( (LA(1)==Integer) ) {
          }
          else {FAIL(1,err14,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
        }
      }
       start  = samples();

      {
        if ( (LA(1)==Integer)
 ) {
           len  = samples();

        }
        else {
          if ( (setwd3[LA(1)]&0x20) ) {
          }
          else {FAIL(1,err15,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
        }
      }
      _retv.st = new SubTrack(SubTrack::DATA,
      TrackData(filename.c_str(), offset, start, len));
      _retv.st->swapSamples(swapSamples);
      
          _retv.lineNr = _t21->getLine();
      
          if (trackType != TrackData::AUDIO) {
        log_message(-2, "%s:%d: FILE/AUDIOFILE statements are only allowed for audio tracks.", filename_, _t21->getLine());
        error_ = 1;
      }
      
          if (subChanType != TrackData::SUBCHAN_NONE) {
        log_message(-2, "%s:%d: FILE/AUDIOFILE statements are only allowed for audio tracks without sub-channel mode.", filename_, _t21->getLine());
        error_ = 1;
      }
    }
    else {
      if ( (LA(1)==DataFile) ) {
        zzmatch(DataFile); _t21 = (ANTLRTokenPtr)LT(1); consume();
          filename   = string();

        dMode =  trackType;
        {
          if ( (LA(1)==74) ) {
            zzmatch(74); consume();
              offset   = sLong();

          }
          else {
            if ( (setwd3[LA(1)]&0x40) ) {
            }
            else {FAIL(1,err16,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
          }
        }
        {
          if ( (LA(1)==Integer)
 ) {
              len   = dataLength(  dMode,  subChanType  );

          }
          else {
            if ( (setwd3[LA(1)]&0x80) ) {
            }
            else {FAIL(1,err17,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
          }
        }
        _retv.st = new SubTrack(SubTrack::DATA, TrackData(dMode,  subChanType,
        filename.c_str(),
        offset, len));
        _retv.lineNr = _t21->getLine();
      }
      else {
        if ( (LA(1)==Fifo) ) {
          zzmatch(Fifo); _t21 = (ANTLRTokenPtr)LT(1); consume();
            filename   = string();

            len   = dataLength(  trackType,  subChanType  );

          _retv.st = new SubTrack(SubTrack::DATA, TrackData( trackType,
          subChanType,
          filename.c_str(), len));
        }
        else {
          if ( (LA(1)==Silence) ) {
            zzmatch(Silence); _t21 = (ANTLRTokenPtr)LT(1); consume();
             len  = samples();

            _retv.st = new SubTrack(SubTrack::DATA, TrackData(len));
            _retv.lineNr = _t21->getLine();
            if (len == 0) {
              log_message(-2, "%s:%d: Length of silence is 0.\n",
              filename_, _retv.lineNr);
              error_ = 1;
            }
            
          if (trackType != TrackData::AUDIO) {
              log_message(-2, "%s:%d: SILENCE statements are only allowed for audio tracks.", filename_, _t21->getLine());
              error_ = 1;
            }
            
          if (subChanType != TrackData::SUBCHAN_NONE) {
              log_message(-2, "%s:%d: SILENCE statements are only allowed for audio tracks without sub-channel mode.", filename_, _t21->getLine());
              error_ = 1;
            }
          }
          else {
            if ( (LA(1)==Zero) ) {
              zzmatch(Zero); _t21 = (ANTLRTokenPtr)LT(1);
              dMode =  trackType;
 consume();
              {
                if ( (setwd4[LA(1)]&0x1)
 ) {
                    dMode   = dataMode();

                }
                else {
                  if ( (setwd4[LA(1)]&0x2) ) {
                  }
                  else {FAIL(1,err18,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
                }
              }
              {
                if ( (setwd4[LA(1)]&0x4) ) {
                     subChanType   = subChannelMode();

                }
                else {
                  if ( (LA(1)==Integer) ) {
                  }
                  else {FAIL(1,err19,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
                }
              }
                len   = dataLength(  dMode,  subChanType  );

              _retv.st = new SubTrack(SubTrack::DATA, TrackData(dMode,  subChanType,
              len));
              _retv.lineNr = _t21->getLine();
              if (len == 0) {
                log_message(-2, "%s:%d: Length of zero data is 0.\n",
                filename_, _retv.lineNr);
                error_ = 1;
              }
            }
            else {FAIL(1,err20,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
          }
        }
      }
    }
  }
  if (_retv.st != NULL && _retv.st->length() == 0) {
    // try to determine length
    if (_retv.st->determineLength() != 0) {
      log_message(-2, "%s:%d: Cannot determine length of track data specification.",
      filename_, _retv.lineNr);
      error_ = 1;
    }
  }
  return _retv;
fail:
  delete _retv.st, _retv.st = NULL;
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd4, 0x8);
  return _retv;
}

std::string
TocParserGram::string(void)
{
  std::string   _retv;
  zzRULE;
  ANTLRTokenPtr _t11=NULL;
  int linenr = 0; bool is_utf8;
  zzmatch(BeginString); _t11 = (ANTLRTokenPtr)LT(1);
  linenr = _t11->getLine();
 consume();
  {
    int zzcnt=1;
    do {
      {
        ANTLRTokenPtr _t31=NULL;
        if ( (LA(1)==String) ) {
          zzmatch(String); _t31 = (ANTLRTokenPtr)LT(1);
          _retv += _t31->getText();
 consume();
        }
        else {
          if ( (LA(1)==StringBackslash)
 ) {
            zzmatch(StringBackslash); _t31 = (ANTLRTokenPtr)LT(1);
            _retv += "\\";
 consume();
          }
          else {
            if ( (LA(1)==StringQuote) ) {
              zzmatch(StringQuote); _t31 = (ANTLRTokenPtr)LT(1);
              _retv += "\"";
 consume();
            }
            else {
              if ( (LA(1)==StringOctal) ) {
                zzmatch(StringOctal); _t31 = (ANTLRTokenPtr)LT(1);
                _retv += _t31->getText();
 consume();
              }
              else {FAIL(1,err21,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
            }
          }
        }
      }
    } while ( (setwd4[LA(1)]&0x10) );
  }
  if (!Util::processMixedString(_retv, is_utf8)) {
    log_message(-2, "%s:%d: Illegal mixed UTF-8 and binary.", filename_, linenr);
    error_ = 1;
  }
  zzmatch(EndString); consume();
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd4, 0x20);
  return _retv;
}

TocParserGram::_rv5
TocParserGram::stringEmpty(void)
{
  struct _rv5 _retv;
  zzRULE;
  ANTLRTokenPtr _t11=NULL;
  int linenr = 0;
  zzmatch(BeginString); _t11 = (ANTLRTokenPtr)LT(1);
  linenr = _t11->getLine();
 consume();
  {
    while ( (setwd4[LA(1)]&0x40) ) {
      {
        ANTLRTokenPtr _t31=NULL;
        if ( (LA(1)==String)
 ) {
          zzmatch(String); _t31 = (ANTLRTokenPtr)LT(1);
          _retv.ret += _t31->getText();
 consume();
        }
        else {
          if ( (LA(1)==StringBackslash) ) {
            zzmatch(StringBackslash); _t31 = (ANTLRTokenPtr)LT(1);
            _retv.ret += "\\";
 consume();
          }
          else {
            if ( (LA(1)==StringQuote) ) {
              zzmatch(StringQuote); _t31 = (ANTLRTokenPtr)LT(1);
              _retv.ret += "\"";
 consume();
            }
            else {
              if ( (LA(1)==StringOctal) ) {
                zzmatch(StringOctal); _t31 = (ANTLRTokenPtr)LT(1);
                _retv.ret += _t31->getText();
 consume();
              }
              else {FAIL(1,err22,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
            }
          }
        }
      }
    }
  }
  if (!Util::processMixedString(_retv.ret, _retv.is_utf8)) {
    log_message(-2, "%s:%d: Illegal mixed UTF-8 and binary.", filename_, linenr);
    error_ = 1;
  }
  zzmatch(EndString); consume();
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd4, 0x80);
  return _retv;
}

unsigned long
TocParserGram::uLong(void)
{
  unsigned long   _retv;
  zzRULE;
  ANTLRTokenPtr _t11=NULL;
  _retv = 0;
  zzmatch(Integer); _t11 = (ANTLRTokenPtr)LT(1);
  _retv = strtoul(_t11->getText(), NULL, 10);
 consume();
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd5, 0x1);
  return _retv;
}

long
TocParserGram::sLong(void)
{
  long   _retv;
  zzRULE;
  ANTLRTokenPtr _t11=NULL;
  _retv = 0;
  zzmatch(Integer); _t11 = (ANTLRTokenPtr)LT(1);
  _retv = strtol(_t11->getText(), NULL, 10);
 consume();
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd5, 0x2);
  return _retv;
}

TocParserGram::_rv8
TocParserGram::integer(void)
{
  struct _rv8 _retv;
  zzRULE;
  ANTLRTokenPtr _t11=NULL;
  _retv.i = 0;
  zzmatch(Integer); _t11 = (ANTLRTokenPtr)LT(1);
  _retv.i = atol(_t11->getText()); _retv.lineNr = _t11->getLine();
 consume();
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd5, 0x4);
  return _retv;
}

Msf
TocParserGram::msf(void)
{
  Msf   _retv;
  zzRULE;
  int min = 0;
  int sec = 0;
  int frac = 0;
  int err = 0;
  int minLine;
  int secLine;
  int fracLine;
  { struct _rv8 _trv; _trv = integer();

  min = _trv.i; minLine  = _trv.lineNr; }
  zzmatch(75); consume();
  { struct _rv8 _trv; _trv = integer();

  sec = _trv.i; secLine  = _trv.lineNr; }
  zzmatch(75); consume();
  { struct _rv8 _trv; _trv = integer();

  frac = _trv.i; fracLine  = _trv.lineNr; }
  if (min < 0) {
    log_message(-2, "%s:%d: Illegal minute field: %d\n", filename_,
    minLine, min);
    err = error_ = 1;
  }
  if (sec < 0 || sec > 59) {
    log_message(-2, "%s:%d: Illegal second field: %d\n", filename_,
    secLine, sec);
    err = error_ = 1;
  }
  if (frac < 0 || frac > 74) {
    log_message(-2, "%s:%d: Illegal fraction field: %d\n", filename_,
    fracLine, frac);
    err = error_ = 1;
  }
  
       if (err != 0) {
    _retv = Msf(0);
  }
  else {
    _retv = Msf(min, sec, frac);
  }
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd5, 0x8);
  return _retv;
}

unsigned long
TocParserGram::samples(void)
{
  unsigned long   _retv;
  zzRULE;
  Msf m;
  {
    if ( (LA(1)==Integer) && (LA(2)==75)
 ) {
        m   = msf();

      _retv = m.samples();
    }
    else {
      if ( (LA(1)==Integer) && (setwd5[LA(2)]&0x10) ) {
          _retv   = uLong();

      }
      else {FAIL(2,err23,err24,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  return _retv;
fail:
  _retv = 0;   
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd5, 0x20);
  return _retv;
}

unsigned long
TocParserGram::dataLength(TrackData::Mode mode,TrackData::SubChannelMode sm)
{
  unsigned long   _retv;
  zzRULE;
  Msf m;
  unsigned long blen;
  
       blen = TrackData::dataBlockSize(mode, sm);
  {
    if ( (LA(1)==Integer) && (LA(2)==75) ) {
        m  = msf();

      _retv = m.lba() * blen;
    }
    else {
      if ( (LA(1)==Integer) && 
(setwd5[LA(2)]&0x40) ) {
          _retv   = uLong();

      }
      else {FAIL(2,err25,err26,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  return _retv;
fail:
  _retv = 0;   
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd5, 0x80);
  return _retv;
}

TrackData::Mode
TocParserGram::dataMode(void)
{
  TrackData::Mode   _retv;
  zzRULE;
  {
    if ( (LA(1)==Audio) ) {
      zzmatch(Audio);
      _retv = TrackData::AUDIO;
 consume();
    }
    else {
      if ( (LA(1)==Mode0) ) {
        zzmatch(Mode0);
        _retv = TrackData::MODE0;
 consume();
      }
      else {
        if ( (LA(1)==Mode1) ) {
          zzmatch(Mode1);
          _retv = TrackData::MODE1;
 consume();
        }
        else {
          if ( (LA(1)==Mode1Raw)
 ) {
            zzmatch(Mode1Raw);
            _retv = TrackData::MODE1_RAW;
 consume();
          }
          else {
            if ( (LA(1)==Mode2) ) {
              zzmatch(Mode2);
              _retv = TrackData::MODE2;
 consume();
            }
            else {
              if ( (LA(1)==Mode2Raw) ) {
                zzmatch(Mode2Raw);
                _retv = TrackData::MODE2_RAW;
 consume();
              }
              else {
                if ( (LA(1)==Mode2Form1) ) {
                  zzmatch(Mode2Form1);
                  _retv = TrackData::MODE2_FORM1;
 consume();
                }
                else {
                  if ( (LA(1)==Mode2Form2) ) {
                    zzmatch(Mode2Form2);
                    _retv = TrackData::MODE2_FORM2;
 consume();
                  }
                  else {
                    if ( (LA(1)==Mode2FormMix)
 ) {
                      zzmatch(Mode2FormMix);
                      _retv = TrackData::MODE2_FORM_MIX;
 consume();
                    }
                    else {FAIL(1,err27,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x1);
  return _retv;
}

TrackData::Mode
TocParserGram::trackMode(void)
{
  TrackData::Mode   _retv;
  zzRULE;
  {
    if ( (LA(1)==Audio) ) {
      zzmatch(Audio);
      _retv = TrackData::AUDIO;
 consume();
    }
    else {
      if ( (LA(1)==Mode1) ) {
        zzmatch(Mode1);
        _retv = TrackData::MODE1;
 consume();
      }
      else {
        if ( (LA(1)==Mode1Raw) ) {
          zzmatch(Mode1Raw);
          _retv = TrackData::MODE1_RAW;
 consume();
        }
        else {
          if ( (LA(1)==Mode2) ) {
            zzmatch(Mode2);
            _retv = TrackData::MODE2;
 consume();
          }
          else {
            if ( (LA(1)==Mode2Raw)
 ) {
              zzmatch(Mode2Raw);
              _retv = TrackData::MODE2_RAW;
 consume();
            }
            else {
              if ( (LA(1)==Mode2Form1) ) {
                zzmatch(Mode2Form1);
                _retv = TrackData::MODE2_FORM1;
 consume();
              }
              else {
                if ( (LA(1)==Mode2Form2) ) {
                  zzmatch(Mode2Form2);
                  _retv = TrackData::MODE2_FORM2;
 consume();
                }
                else {
                  if ( (LA(1)==Mode2FormMix) ) {
                    zzmatch(Mode2FormMix);
                    _retv = TrackData::MODE2_FORM_MIX;
 consume();
                  }
                  else {FAIL(1,err28,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
                }
              }
            }
          }
        }
      }
    }
  }
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x2);
  return _retv;
}

TrackData::SubChannelMode
TocParserGram::subChannelMode(void)
{
  TrackData::SubChannelMode   _retv;
  zzRULE;
  {
    if ( (LA(1)==SubChanRw) ) {
      zzmatch(SubChanRw);
      _retv = TrackData::SUBCHAN_RW;
 consume();
    }
    else {
      if ( (LA(1)==SubChanRwRaw)
 ) {
        zzmatch(SubChanRwRaw);
        _retv = TrackData::SUBCHAN_RW_RAW;
 consume();
      }
      else {FAIL(1,err29,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x4);
  return _retv;
}

Toc::Type
TocParserGram::tocType(void)
{
  Toc::Type   _retv;
  zzRULE;
  {
    if ( (LA(1)==TocTypeCdda) ) {
      zzmatch(TocTypeCdda);
      _retv = Toc::Type::CD_DA;
 consume();
    }
    else {
      if ( (LA(1)==TocTypeCdrom) ) {
        zzmatch(TocTypeCdrom);
        _retv = Toc::Type::CD_ROM;
 consume();
      }
      else {
        if ( (LA(1)==TocTypeCdromXa) ) {
          zzmatch(TocTypeCdromXa);
          _retv = Toc::Type::CD_ROM_XA;
 consume();
        }
        else {
          if ( (LA(1)==TocTypeCdi) ) {
            zzmatch(TocTypeCdi);
            _retv = Toc::Type::CD_I;
 consume();
          }
          else {FAIL(1,err30,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
        }
      }
    }
  }
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x8);
  return _retv;
}

TocParserGram::_rv16
TocParserGram::packType(void)
{
  struct _rv16 _retv;
  zzRULE;
  {
    ANTLRTokenPtr _t21=NULL;
    if ( (LA(1)==Title)
 ) {
      zzmatch(Title); _t21 = (ANTLRTokenPtr)LT(1);
      _retv.t = CdTextItem::PackType::TITLE; _retv.lineNr = _t21->getLine();
 consume();
    }
    else {
      if ( (LA(1)==Performer) ) {
        zzmatch(Performer); _t21 = (ANTLRTokenPtr)LT(1);
        _retv.t = CdTextItem::PackType::PERFORMER; _retv.lineNr = _t21->getLine();
 consume();
      }
      else {
        if ( (LA(1)==Songwriter) ) {
          zzmatch(Songwriter); _t21 = (ANTLRTokenPtr)LT(1);
          _retv.t = CdTextItem::PackType::SONGWRITER; _retv.lineNr = _t21->getLine();
 consume();
        }
        else {
          if ( (LA(1)==Composer) ) {
            zzmatch(Composer); _t21 = (ANTLRTokenPtr)LT(1);
            _retv.t = CdTextItem::PackType::COMPOSER; _retv.lineNr = _t21->getLine();
 consume();
          }
          else {
            if ( (LA(1)==Arranger) ) {
              zzmatch(Arranger); _t21 = (ANTLRTokenPtr)LT(1);
              _retv.t = CdTextItem::PackType::ARRANGER; _retv.lineNr = _t21->getLine();
 consume();
            }
            else {
              if ( (LA(1)==Message)
 ) {
                zzmatch(Message); _t21 = (ANTLRTokenPtr)LT(1);
                _retv.t = CdTextItem::PackType::MESSAGE; _retv.lineNr = _t21->getLine();
 consume();
              }
              else {
                if ( (LA(1)==DiscId) ) {
                  zzmatch(DiscId); _t21 = (ANTLRTokenPtr)LT(1);
                  _retv.t = CdTextItem::PackType::DISK_ID; _retv.lineNr = _t21->getLine();
 consume();
                }
                else {
                  if ( (LA(1)==Genre) ) {
                    zzmatch(Genre); _t21 = (ANTLRTokenPtr)LT(1);
                    _retv.t = CdTextItem::PackType::GENRE; _retv.lineNr = _t21->getLine();
 consume();
                  }
                  else {
                    if ( (LA(1)==TocInfo1) ) {
                      zzmatch(TocInfo1); _t21 = (ANTLRTokenPtr)LT(1);
                      _retv.t = CdTextItem::PackType::TOC_INFO1; _retv.lineNr = _t21->getLine();
 consume();
                    }
                    else {
                      if ( (LA(1)==TocInfo2) ) {
                        zzmatch(TocInfo2); _t21 = (ANTLRTokenPtr)LT(1);
                        _retv.t = CdTextItem::PackType::TOC_INFO2; _retv.lineNr = _t21->getLine();
 consume();
                      }
                      else {
                        if ( (LA(1)==Reserved1)
 ) {
                          zzmatch(Reserved1); _t21 = (ANTLRTokenPtr)LT(1);
                          _retv.t = CdTextItem::PackType::RES1; _retv.lineNr = _t21->getLine();
 consume();
                        }
                        else {
                          if ( (LA(1)==Reserved2) ) {
                            zzmatch(Reserved2); _t21 = (ANTLRTokenPtr)LT(1);
                            _retv.t = CdTextItem::PackType::RES2; _retv.lineNr = _t21->getLine();
 consume();
                          }
                          else {
                            if ( (LA(1)==Reserved3) ) {
                              zzmatch(Reserved3); _t21 = (ANTLRTokenPtr)LT(1);
                              _retv.t = CdTextItem::PackType::RES3; _retv.lineNr = _t21->getLine();
 consume();
                            }
                            else {
                              if ( (LA(1)==Reserved4) ) {
                                zzmatch(Reserved4); _t21 = (ANTLRTokenPtr)LT(1);
                                _retv.t = CdTextItem::PackType::CLOSED; _retv.lineNr = _t21->getLine();
 consume();
                              }
                              else {
                                if ( (LA(1)==Closed) ) {
                                  zzmatch(Closed); _t21 = (ANTLRTokenPtr)LT(1);
                                  _retv.t = CdTextItem::PackType::CLOSED; _retv.lineNr = _t21->getLine();
 consume();
                                }
                                else {
                                  if ( (LA(1)==UpcEan)
 ) {
                                    zzmatch(UpcEan); _t21 = (ANTLRTokenPtr)LT(1);
                                    _retv.t = CdTextItem::PackType::UPCEAN_ISRC; _retv.lineNr = _t21->getLine();
 consume();
                                  }
                                  else {
                                    if ( (LA(1)==Isrc) ) {
                                      zzmatch(Isrc); _t21 = (ANTLRTokenPtr)LT(1);
                                      _retv.t = CdTextItem::PackType::UPCEAN_ISRC; _retv.lineNr = _t21->getLine();
 consume();
                                    }
                                    else {
                                      if ( (LA(1)==SizeInfo) ) {
                                        zzmatch(SizeInfo); _t21 = (ANTLRTokenPtr)LT(1);
                                        _retv.t = CdTextItem::PackType::SIZE_INFO; _retv.lineNr = _t21->getLine();
 consume();
                                      }
                                      else {FAIL(1,err31,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return _retv;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x10);
  return _retv;
}

TocParserGram::_rv17
TocParserGram::binaryData(void)
{
  struct _rv17 _retv;
  zzRULE;
  static unsigned char buf[MAX_CD_TEXT_DATA_LEN];
  _retv.data = buf;
  _retv.len = 0;
  int i;
  int lineNr;
  zzmatch(76); consume();
  {
    if ( (LA(1)==Integer) ) {
      { struct _rv8 _trv; _trv = integer();

      i = _trv.i; lineNr   = _trv.lineNr; }
      if (i < 0 || i > 255) {
        log_message(-2, "%s:%d: Illegal binary data: %d", filename_, lineNr, i);
        error_ = 1;
        i = 0;
      }
      
         buf[0] = i;
      _retv.len = 1;
      {
        while ( (LA(1)==77) ) {
          zzmatch(77); consume();
          { struct _rv8 _trv; _trv = integer();

          i = _trv.i; lineNr   = _trv.lineNr; }
          if (i < 0 || i > 255) {
            log_message(-2, "%s:%d: Illegal binary data: %d",
            filename_, lineNr, i);
            error_ = 1;
            i = 0;
          }
          
	   if (_retv.len >= MAX_CD_TEXT_DATA_LEN) {
            log_message(-2, "%s:%d: Binary data exceeds maximum length (%d).",
            filename_, lineNr, MAX_CD_TEXT_DATA_LEN);
            error_ = 1;
          }
          else {
            buf[_retv.len] = i;
            _retv.len += 1;
          }
        }
      }
    }
    else {
      if ( (LA(1)==78)
 ) {
      }
      else {FAIL(1,err32,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  zzmatch(78); consume();
  return _retv;
fail:
  _retv.len = 0;   
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x20);
  return _retv;
}

TocParserGram::_rv18
TocParserGram::cdTextItem(int blockNr)
{
  struct _rv18 _retv;
  zzRULE;
  _retv.item = NULL;
  CdTextItem::PackType type;
  std::string s;
  bool is_utf8 = false;
  const unsigned char *data;
  long len;
  { struct _rv16 _trv; _trv = packType();

  type = _trv.t; _retv.lineNr   = _trv.lineNr; }
  {
    if ( (LA(1)==BeginString) ) {
      { struct _rv5 _trv; _trv = stringEmpty();

      s = _trv.ret; is_utf8   = _trv.is_utf8; }
      if (!s.empty()) {
        _retv.item = new CdTextItem(type, blockNr);
        if (is_utf8)
        _retv.item->setText(s.c_str());
        else
        _retv.item->setRawText(s);
      }
    }
    else {
      if ( (LA(1)==76) ) {
        { struct _rv17 _trv; _trv = binaryData();

        data = _trv.data; len   = _trv.len; }
        _retv.item = new CdTextItem(type, blockNr);
        _retv.item->setData(data, len);
      }
      else {FAIL(1,err33,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  return _retv;
fail:
  
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd6, 0x40);
  return _retv;
}

void
TocParserGram::cdTextBlock(CdTextContainer & container,int isTrack)
{
  zzRULE;
  CdTextItem* item = nullptr;
  int blockNr = 0;
  int lineNr = 0;
  zzmatch(Language); consume();
  { struct _rv8 _trv; _trv = integer();

  blockNr = _trv.i; lineNr   = _trv.lineNr; }
  zzmatch(76);
  if (blockNr < 0 || blockNr > 7) {
    log_message(-2, "%s:%d: Invalid block number, allowed range: [0..7].",
    filename_, lineNr);
    error_ = 1;
    blockNr = 0;
  }
 consume();
  {
    if ( (LA(1)==EncodingLatin) ) {
      zzmatch(EncodingLatin);
      container.encoding(blockNr, Util::Encoding::LATIN);
 consume();
    }
    else {
      if ( (LA(1)==EncodingAscii) ) {
        zzmatch(EncodingAscii);
        container.encoding(blockNr, Util::Encoding::ASCII);
 consume();
      }
      else {
        if ( (LA(1)==EncodingJis)
 ) {
          zzmatch(EncodingJis);
          container.encoding(blockNr, Util::Encoding::MSJIS);
 consume();
        }
        else {
          if ( (LA(1)==EncodingKorean) ) {
            zzmatch(EncodingKorean);
            container.encoding(blockNr, Util::Encoding::KOREAN);
 consume();
          }
          else {
            if ( (LA(1)==EncodingMandarin) ) {
              zzmatch(EncodingMandarin);
              container.encoding(blockNr, Util::Encoding::MANDARIN);
 consume();
            }
            else {
              if ( (setwd6[LA(1)]&0x80) ) {
              }
              else {FAIL(1,err34,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
            }
          }
        }
      }
    }
  }
  {
    while ( (setwd7[LA(1)]&0x1) ) {
      { struct _rv18 _trv; _trv = cdTextItem(  blockNr  );

      item = _trv.item; lineNr   = _trv.lineNr; }
      if (item) {
        int type = (int)item->packType();
        
           if (isTrack && ((type > 0x86 && type <= 0x89) || type == 0x8f)) {
          log_message(-2, "%s:%d: Invalid CD-TEXT item for a track.",
          filename_, lineNr);
          error_ = 1;
        }
        else {
          container.add(item);
        }
      }
    }
  }
  zzmatch(78); consume();
  return;
fail:
  
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd7, 0x2);
}

void
TocParserGram::cdTextLanguageMap(CdTextContainer & container)
{
  zzRULE;
  int blockNr;
  int lang;
  int blockNrLine;
  int langLine = 0;
  zzmatch(LanguageMap); consume();
  zzmatch(76); consume();
  {
    int zzcnt=1;
    do {
      { struct _rv8 _trv; _trv = integer();

      blockNr = _trv.i; blockNrLine  = _trv.lineNr; }
      zzmatch(75); consume();
      {
        if ( (LA(1)==Integer)
 ) {
          { struct _rv8 _trv; _trv = integer();

          lang = _trv.i; langLine   = _trv.lineNr; }
        }
        else {
          if ( (LA(1)==LangEn) ) {
            zzmatch(LangEn);
            lang = 9;
 consume();
          }
          else {FAIL(1,err35,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
        }
      }
      if (blockNr >= 0 && blockNr <= 7) {
        if (lang >= 0 && lang <= 255) {
          container.language(blockNr, lang);
        }
        else {
          log_message(-2,
          "%s:%d: Invalid language code, allowed range: [0..255].",
          filename_, langLine);
          error_ = 1;
        }
      }
      else {
      log_message(-2,
      "%s:%d: Invalid language number, allowed range: [0..7].",
      filename_, blockNrLine);
      error_ = 1;
    }
    } while ( (LA(1)==Integer) );
  }
  zzmatch(78); consume();
  return;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd7, 0x4);
}

void
TocParserGram::cdTextTrack(CdTextContainer & container)
{
  zzRULE;
  zzmatch(CdText); consume();
  zzmatch(76); consume();
  {
    while ( (LA(1)==Language) ) {
      cdTextBlock(  container, 1  );
    }
  }
  zzmatch(78); consume();
  return;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd7, 0x8);
}

void
TocParserGram::cdTextGlobal(CdTextContainer & container)
{
  zzRULE;
  zzmatch(CdText); consume();
  zzmatch(76); consume();
  {
    if ( (LA(1)==LanguageMap) ) {
      cdTextLanguageMap(  container  );
    }
    else {
      if ( (setwd7[LA(1)]&0x10)
 ) {
      }
      else {FAIL(1,err36,&zzMissSet,&zzMissText,&zzBadTok,&zzBadText,&zzErrk); goto fail;}
    }
  }
  {
    while ( (LA(1)==Language) ) {
      cdTextBlock(  container, 0  );
    }
  }
  zzmatch(78); consume();
  return;
fail:
  syn(zzBadTok, (ANTLRChar *)"", zzMissSet, zzMissTok, zzErrk);
  resynch(setwd7, 0x20);
}

ANTLRTokenType TocLexer::erraction()
{
  log_message(-2, "%s:%d: Illegal token: %s", parser_->filename_,
  _line, _lextext);
  parser_->error_ = 1;
  return Eof;
}

void TocParserGram::syn(_ANTLRTokenPtr tok, ANTLRChar *egroup,
SetWordType *eset, ANTLRTokenType etok, int k)
{
  int line;
  
  error_ = 1;
  line = LT(1)->getLine();
  
  log_message(-2, "%s:%d: syntax error at \"%s\" ", filename_, line,
  LT(1)->getType() == Eof ? "EOF" : LT(1)->getText());
  if ( !etok && !eset ) {
    log_message(0, "");
    return;
  }
  if ( k==1 ) {
    log_message(0, "missing ");
  }
  else {
    log_message(0, "; \"%s\" not ", LT(1)->getText());
    if ( set_deg(eset)>1 ) log_message(-2, " in ");
  }
  if ( set_deg(eset)>0 )
  edecode(eset);
  else log_message(0, "%s ", token_tbl[etok]);
  
  if ( strlen(egroup) > 0 )
  log_message(0, "in %s ", egroup);
  
  log_message(0, "");
}


Toc *parseToc(const char* inp, const char *filename)
{
  DLGStringInput in(inp);
  TocLexer scan(&in);
  ANTLRTokenBuffer pipe(&scan);
  ANTLRToken aToken;
  scan.setToken(&aToken);
  TocParserGram parser(&pipe);
  
  parser.filename_ = filename;
  scan.parser_ = &parser;
  
  parser.init();
  parser.error_ = 0;
  
  Toc *t = parser.toc();
  
  if (parser.error_ != 0) {
    return NULL;
  }
  else {
    return t;
  }
}

Toc *parseToc(FILE *fp, const char *filename)
{
  DLGFileInput in(fp);
  TocLexer scan(&in);
  ANTLRTokenBuffer pipe(&scan);
  ANTLRToken aToken;
  scan.setToken(&aToken);
  TocParserGram parser(&pipe);
  
  parser.filename_ = filename;
  scan.parser_ = &parser;
  
  parser.init();
  parser.error_ = 0;
  
  Toc *t = parser.toc();
  
  if (parser.error_ != 0) {
    return NULL;
  }
  else {
    return t;
  }
}
