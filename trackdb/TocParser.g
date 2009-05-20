/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001 Andreas Mueller <andreas@daneb.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#header <<
#include <config.h>
#include <stdlib.h>
#include "Toc.h"
#include "util.h"
#include "log.h"
#include "CdTextItem.h"
>>

<<
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
 >>

<<
class TocLexer : public TocLexerBase {
public:
  TocLexer(DLGInputStream *in, unsigned bufsize=2000)
    : TocLexerBase(in, bufsize) { parser_ = NULL; }

  virtual ANTLRTokenType erraction();

  TocParserGram *parser_;   
};
>>

#lexclass START
#token Eof		"@"
#token                  "[\t\r\ ]+"    << skip(); >>
#token Comment          "//~[\n@]*"  << skip(); >>
#token                  "\n"         << newline(); skip(); >>
#token BeginString      "\""         << mode(STRING); >>
#token Integer          "[0-9]+"
#token TrackDef         "TRACK"
#token FirstTrackNo     "FIRST_TRACK_NO"
#token Audio            "AUDIO"
#token Mode0            "MODE0"
#token Mode1            "MODE1"
#token Mode1Raw         "MODE1_RAW"
#token Mode2            "MODE2"
#token Mode2Raw         "MODE2_RAW"
#token Mode2Form1       "MODE2_FORM1"
#token Mode2Form2       "MODE2_FORM2"
#token Mode2FormMix     "MODE2_FORM_MIX"
#token SubChanRw        "RW"
#token SubChanRwRaw     "RW_RAW"
#token Index            "INDEX"
#token Catalog          "CATALOG"
#token Isrc             "ISRC"
#token No               "NO"
#token Copy             "COPY"
#token PreEmphasis     "PRE_EMPHASIS"
#token TwoChannel       "TWO_CHANNEL_AUDIO"
#token FourChannel      "FOUR_CHANNEL_AUDIO"
#tokclass AudioFile     { "AUDIOFILE" "FILE" }
#token DataFile         "DATAFILE"
#token Fifo             "FIFO"
#token Silence          "SILENCE"
#token Zero             "ZERO"
#token Pregap           "PREGAP"
#token Start            "START"
#token End              "END"
#token TocTypeCdda      "CD_DA"
#token TocTypeCdrom     "CD_ROM"
#token TocTypeCdromXa   "CD_ROM_XA"
#token TocTypeCdi       "CD_I"
#token Swap             "SWAP"

#token CdText           "CD_TEXT"
#token Language         "LANGUAGE"
#token LanguageMap      "LANGUAGE_MAP"
#token Title            "TITLE"
#token Performer        "PERFORMER"
#token Songwriter       "SONGWRITER"
#token Composer         "COMPOSER"
#token Arranger         "ARRANGER"
#token Message          "MESSAGE"
#token DiscId           "DISC_ID"
#token Genre            "GENRE"
#token TocInfo1         "TOC_INFO1"
#token TocInfo2         "TOC_INFO2"
#token Reserved1        "RESERVED1"
#token Reserved2        "RESERVED2"
#token Reserved3        "RESERVED3"
#token Reserved4        "RESERVED4"
#token UpcEan           "UPC_EAN"
#token SizeInfo         "SIZE_INFO"
#token LangEn           "EN"

#lexclass STRING
#token EndString        "\""         << mode(START); >>
#token StringQuote      "\\\""
#token StringOctal      "\\[0-9][0-9][0-9]"
#token String           "\\"
#token String           "[ ]+"
#token String           "~[\\\n\"\t ]*"

#lexclass START


class TocParserGram {
<<
public:
  const char *filename_;
  int error_;

void syn(_ANTLRTokenPtr tok, ANTLRChar *egroup, SetWordType *eset,
	 ANTLRTokenType etok, int k);
>>

toc > [ Toc *t ]
  : << $t = new Toc; 
       Track *tr = NULL;
       int lineNr = 0;
       char *catalog = NULL;
       Toc::TocType toctype;
       int firsttrack, firstLine;
    >>
  (  Catalog string > [ catalog ]
     << if (catalog != NULL) {
          if ($t->catalog(catalog) != 0) {
            log_message(-2, "%s:%d: Illegal catalog number: %s.\n",
                	filename_, $1->getLine(), catalog);
            error_ = 1;
          }
         delete[] catalog;
	 catalog = NULL;
       } 
     >> 
   | tocType > [ toctype ]
     << $t->tocType(toctype); >> 
  )*

  { FirstTrackNo integer > [ firsttrack, firstLine ]
    << if (firsttrack > 0 && firsttrack < 100) {
            $t->firstTrackNo(firsttrack);
       } else {
         log_message(-2, "%s:%d: Illegal track number: %d\n", filename_,
	         firstLine, firsttrack);
            error_ = 1;
       }
    >>
  }

  { cdTextGlobal [ $t->cdtext_ ] }

  ( track > [ tr, lineNr ]
    << if (tr != NULL) {
         if ($t->append(tr) != 0) {
           log_message(-2, "%s:%d: First track must not have a pregap.\n",
               		filename_, lineNr);
           error_ = 1;
         }
         delete tr, tr = NULL;
       }
    >>
  )+
  Eof
  ;
  // fail action
  << delete $t, $t = NULL;
     delete[] catalog;
  >>

track > [ Track *tr, int lineNr ]
  : << $tr = NULL;
       $lineNr = 0;
       SubTrack *st = NULL;
       char *isrcCode = NULL;
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
    >>
    TrackDef << $lineNr = $1->getLine(); >>
    trackMode > [ trackType ]
    { subChannelMode > [ subChanType ] }
    << $tr = new Track(trackType, subChanType); >>

    (  Isrc string > [ isrcCode ]
       << if (isrcCode != NULL) {
            if ($tr->isrc(isrcCode) != 0) {
              log_message(-2, "%s:%d: Illegal ISRC code: %s.\n",
                          filename_, $1->getLine(), isrcCode);
              error_ = 1;
            }
            delete[] isrcCode;
          }
       >>
     | { No << flag = 0; >> } Copy
       << $tr->copyPermitted(flag); flag = 1; >>
     | { No << flag = 0; >> } PreEmphasis
       << $tr->preEmphasis(flag); flag = 1; >>
     | TwoChannel 
       << $tr->audioType(0); >>
     | FourChannel
       << $tr->audioType(1); >>
    )*

    { cdTextTrack [ $tr->cdtext_ ] }

    { Pregap msf > [ length ] 
      << if (length.lba() == 0) {
	   log_message(-2, "%s:%d: Length of pregap is zero.\n",
	               filename_, $1->getLine());
	   error_ = 1;
	 }
         else {
           if (trackType == TrackData::AUDIO) {
             $tr->append(SubTrack(SubTrack::DATA, 
                                  TrackData(length.samples())));
           }
	   else {
             $tr->append(SubTrack(SubTrack::DATA,
                                  TrackData(trackType, subChanType,
                                            length.lba() * TrackData::dataBlockSize(trackType, subChanType))));
           }
           startPos = $tr->length();
	   startPosLine = $1->getLine();
         }
      >>
    }

    (  subTrack [ trackType, subChanType ] > [ st, lineNr ] 
       << 
          if (st != NULL && $tr->append(*st) == 2) {
	    log_message(-2,
		    "%s:%d: Mixing of FILE/AUDIOFILE/SILENCE and DATAFILE/ZERO statements not allowed.", filename_, lineNr);
	    log_message(-2,
		    "%s:%d: PREGAP acts as SILENCE in audio tracks.",
		    filename_, lineNr);
	    error_ = 1;
	  }

          delete st, st = NULL;
       >>
     | Start << posGiven = 0; >> { msf > [ pos ] << posGiven = 1; >> }
       << if (startPosLine != 0) {
            log_message(-2,
                    "%s:%d: Track start (end of pre-gap) already defined.\n",
                    filename_, $1->getLine());
            error_ = 1;
          }
          else {
            if (!posGiven) {
              pos = $tr->length(); // retrieve current position
            }
            startPos = pos;
	    startPosLine = $1->getLine();
          }
          pos = Msf(0);
       >>
     | End << posGiven = 0; >> { msf > [ pos ] << posGiven = 1; >> }
       << if (endPosLine != 0) {
            log_message(-2,
                    "%s:%d: Track end (start of post-gap) already defined.\n",
                    filename_, $1->getLine());
            error_ = 1;
          }
          else {
            if (!posGiven) {
              pos = $tr->length(); // retrieve current position
            }
            endPos = pos;
	    endPosLine = $1->getLine();
          }
          pos = Msf(0);
       >>
    )+

    // set track start (end of pre-gap) and check for minimal track length
    << if (startPosLine != 0 && $tr->start(startPos) != 0) {
         log_message(-2,
                 "%s:%d: START %s behind or at track end.\n", filename_,
	         startPosLine, startPos.str());
         error_ = 1;
       }
    >>

    ( Index msf > [ indexIncrement ]
      << if ($tr != NULL) {
           switch ($tr->appendIndex(indexIncrement)) {
           case 1:
             log_message(-2, "%s:%d: More than 98 index increments.\n",
                     filename_, $1->getLine());
             error_ = 1;
             break;

           case 2:
             log_message(-2, "%s:%d: Index beyond track end.\n",
                     filename_,  $1->getLine());
             error_ = 1;
             break;

           case 3:
             log_message(-2, "%s:%d: Index at start of track.\n",
                     filename_,  $1->getLine());
             error_ = 1;
             break;
           }
         }
      >>
    )*

    // set track end (start of post-gap)
    << if (endPosLine != 0) {
         switch ($tr->end(endPos)) {
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
    >>
    ;
    // fail action
    << delete $tr, $tr = NULL;
       delete[] isrcCode;
    >>


subTrack < [ TrackData::Mode trackType, TrackData::SubChannelMode subChanType ] > [ SubTrack *st, int lineNr ]
  : << $st = NULL;
       $lineNr = 0;
       char *filename = NULL;
       unsigned long start = 0;
       unsigned long len = 0;
       long offset = 0;
       TrackData::Mode dMode;
       int swapSamples = 0;
    >>
    (  AudioFile string > [ filename ] 
       { Swap << swapSamples = 1; >> }
       { "#" sLong > [ offset ] }
       samples > [start] { samples > [len] }
       << $st = new SubTrack(SubTrack::DATA,
	                     TrackData(filename, offset, start, len));
	  $st->swapSamples(swapSamples);

          $lineNr = $1->getLine();

          if (trackType != TrackData::AUDIO) {
            log_message(-2, "%s:%d: FILE/AUDIOFILE statements are only allowed for audio tracks.", filename_, $1->getLine());
	    error_ = 1;
          }

          if (subChanType != TrackData::SUBCHAN_NONE) {
	    log_message(-2, "%s:%d: FILE/AUDIOFILE statements are only allowed for audio tracks without sub-channel mode.", filename_, $1->getLine());
	    error_ = 1;
          }
       >>
     | DataFile string > [ filename ]
       << dMode = $trackType; >>
       // { dataMode > [ dMode ] }
       { "#" sLong > [ offset ] }
       { dataLength [ dMode, $subChanType ] > [ len ] }
       << $st = new SubTrack(SubTrack::DATA, TrackData(dMode, $subChanType,
                                                       filename, 
                                                       offset, len));
          $lineNr = $1->getLine();
       >>
     | Fifo string > [ filename ] 
            dataLength [$trackType, $subChanType ] > [ len ]
       << $st = new SubTrack(SubTrack::DATA, TrackData($trackType,
                                                       $subChanType,
                                                       filename, len));
       >>
     | Silence samples > [len]
       << $st = new SubTrack(SubTrack::DATA, TrackData(len));
          $lineNr = $1->getLine();
          if (len == 0) {
	    log_message(-2, "%s:%d: Length of silence is 0.\n",
		    filename_, $lineNr);
	    error_ = 1;
	  }

          if (trackType != TrackData::AUDIO) {
            log_message(-2, "%s:%d: SILENCE statements are only allowed for audio tracks.", filename_, $1->getLine());
	    error_ = 1;
          }

          if (subChanType != TrackData::SUBCHAN_NONE) {
	    log_message(-2, "%s:%d: SILENCE statements are only allowed for audio tracks without sub-channel mode.", filename_, $1->getLine());
	    error_ = 1;
          }
       >>
     | Zero 
       << dMode = $trackType; >>
       { dataMode > [ dMode ] }
       { subChannelMode > [ $subChanType ] }
       dataLength [ dMode, $subChanType ] > [ len ]
       << $st = new SubTrack(SubTrack::DATA, TrackData(dMode, $subChanType,
                                                       len));
          $lineNr = $1->getLine();
          if (len == 0) {
	    log_message(-2, "%s:%d: Length of zero data is 0.\n",
		    filename_, $lineNr);
	    error_ = 1;
	  }
       >>
    )
    << if ($st != NULL && $st->length() == 0) {
         // try to determine length 
         if ($st->determineLength() != 0) {
	         log_message(-2, "%s:%d: Cannot determine length of track data specification.",
		                 filename_, $lineNr);
	         error_ = 1;
      	 }
       }
    >> 
    ;
    // fail action
    << delete $st, $st = NULL;
       delete[] filename;
    >>

string > [ char *ret ]
 :  << $ret = strdupCC("");
       char *s;
       char buf[2];
    >>

    << buf[1] = 0; >>

    BeginString 
    ( (  String      << s = strdup3CC($ret, $1->getText(), NULL); >>
       | StringQuote << s = strdup3CC($ret, "\"", NULL); >>
       | StringOctal << buf[0] = strtol($1->getText() + 1, NULL, 8);
                        s = strdup3CC($ret, buf, NULL);
                     >>
      )
      << delete[] $ret;
         $ret = s;
      >>
    )+
 
    EndString
    ;

stringEmpty > [ char *ret ]
 :  << $ret = strdupCC("");
       char *s;
       char buf[2];
    >>

    << buf[1] = 0; >>

    BeginString 
    ( (  String      << s = strdup3CC($ret, $1->getText(), NULL); >>
       | StringQuote << s = strdup3CC($ret, "\"", NULL); >>
       | StringOctal << buf[0] = strtol($1->getText() + 1, NULL, 8);
                        s = strdup3CC($ret, buf, NULL);
                     >>
      )
      << delete[] $ret;
         $ret = s;
      >>
    )*
 
    EndString
    ;



uLong > [ unsigned long l ]
  : << $l = 0; >>
    Integer << $l = strtoul($1->getText(), NULL, 10); >>
    ;

sLong > [ long l ]
  : << $l = 0; >>
    Integer << $l = strtol($1->getText(), NULL, 10); >>
    ;

integer > [ int i, int lineNr ]
  : << $i = 0; >>
    Integer << $i = atol($1->getText()); $lineNr = $1->getLine(); >>
    ;
    
msf > [ Msf m ]
  : << int min = 0;
       int sec = 0;
       int frac = 0;
       int err = 0;
       int minLine;
       int secLine;
       int fracLine;
    >>
    integer > [min, minLine] ":" integer > [sec, secLine] 
    ":" integer > [frac, fracLine]
    << if (min < 0) {
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
	 $m = Msf(0);
       }
       else {
	 $m = Msf(min, sec, frac);
       }
    >>
    ;


samples > [ unsigned long s ]
  : << Msf m; >>
    (  msf > [ m ] << $s = m.samples(); >>
     | uLong > [ $s ]
    )
    ;
    // fail action
    << $s = 0; >>

dataLength [ TrackData::Mode mode, TrackData::SubChannelMode sm] > [ unsigned long len ]
  : << Msf m;
       unsigned long blen;

       blen = TrackData::dataBlockSize(mode, sm);
    >>

    (  msf > [ m] << $len = m.lba() * blen; >>
     | uLong > [ $len ]
    )
    ;
    // fail action
    << $len = 0; >>

dataMode > [ TrackData::Mode m ]
  :
    (  Audio <<  $m = TrackData::AUDIO; >>
     | Mode0 << $m = TrackData::MODE0; >>
     | Mode1 << $m = TrackData::MODE1; >>
     | Mode1Raw << $m = TrackData::MODE1_RAW; >>
     | Mode2 << $m = TrackData::MODE2; >>
     | Mode2Raw << $m = TrackData::MODE2_RAW; >>
     | Mode2Form1 << $m = TrackData::MODE2_FORM1; >>
     | Mode2Form2 << $m = TrackData::MODE2_FORM2; >>
     | Mode2FormMix << $m = TrackData::MODE2_FORM_MIX; >>
    )
    ;

trackMode > [ TrackData::Mode m ]
  :
    (  Audio <<  $m = TrackData::AUDIO; >>
     | Mode1 << $m = TrackData::MODE1; >>
     | Mode1Raw << $m = TrackData::MODE1_RAW; >>
     | Mode2 << $m = TrackData::MODE2; >>
     | Mode2Raw << $m = TrackData::MODE2_RAW; >>
     | Mode2Form1 << $m = TrackData::MODE2_FORM1; >>
     | Mode2Form2 << $m = TrackData::MODE2_FORM2; >>
     | Mode2FormMix << $m = TrackData::MODE2_FORM_MIX; >>
    )
    ;

subChannelMode > [ TrackData::SubChannelMode m ]
  :
    (  SubChanRw << $m = TrackData::SUBCHAN_RW; >>
     | SubChanRwRaw << $m = TrackData::SUBCHAN_RW_RAW; >>
    )
    ;

tocType > [ Toc::TocType t ]
  : (  TocTypeCdda << $t = Toc::CD_DA; >>
     | TocTypeCdrom << $t = Toc::CD_ROM; >>
     | TocTypeCdromXa << $t = Toc::CD_ROM_XA; >>
     | TocTypeCdi << $t = Toc::CD_I; >>
    )
    ;

packType > [ CdTextItem::PackType t, int lineNr ]
  : (  Title      << $t = CdTextItem::CDTEXT_TITLE; $lineNr = $1->getLine(); >>
     | Performer  << $t = CdTextItem::CDTEXT_PERFORMER; $lineNr = $1->getLine(); >>
     | Songwriter << $t = CdTextItem::CDTEXT_SONGWRITER; $lineNr = $1->getLine(); >>
     | Composer   << $t = CdTextItem::CDTEXT_COMPOSER; $lineNr = $1->getLine(); >>
     | Arranger   << $t = CdTextItem::CDTEXT_ARRANGER; $lineNr = $1->getLine(); >>
     | Message    << $t = CdTextItem::CDTEXT_MESSAGE; $lineNr = $1->getLine(); >>
     | DiscId     << $t = CdTextItem::CDTEXT_DISK_ID; $lineNr = $1->getLine(); >>
     | Genre      << $t = CdTextItem::CDTEXT_GENRE; $lineNr = $1->getLine(); >>
     | TocInfo1   << $t = CdTextItem::CDTEXT_TOC_INFO1; $lineNr = $1->getLine(); >>
     | TocInfo2   << $t = CdTextItem::CDTEXT_TOC_INFO2; $lineNr = $1->getLine(); >>
     | Reserved1  << $t = CdTextItem::CDTEXT_RES1; $lineNr = $1->getLine(); >>
     | Reserved2  << $t = CdTextItem::CDTEXT_RES2; $lineNr = $1->getLine(); >>
     | Reserved3  << $t = CdTextItem::CDTEXT_RES3; $lineNr = $1->getLine(); >>
     | Reserved4  << $t = CdTextItem::CDTEXT_RES4; $lineNr = $1->getLine(); >>
     | UpcEan     << $t = CdTextItem::CDTEXT_UPCEAN_ISRC; $lineNr = $1->getLine(); >>
     | Isrc       << $t = CdTextItem::CDTEXT_UPCEAN_ISRC; $lineNr = $1->getLine(); >>
     | SizeInfo   << $t = CdTextItem::CDTEXT_SIZE_INFO; $lineNr = $1->getLine(); >>
    )
    ;

binaryData > [ const unsigned char *data, long len ]
  : << static unsigned char buf[MAX_CD_TEXT_DATA_LEN];
       $data = buf;
       $len = 0;
       int i;
       int lineNr;
    >>
    "\{"
    { integer > [ i, lineNr ]
      << if (i < 0 || i > 255) {
           log_message(-2, "%s:%d: Illegal binary data: %d", filename_, lineNr, i);
           error_ = 1;
           i = 0;
         }

         buf[0] = i;
         $len = 1;
      >>
      ( "," integer > [ i, lineNr ]
        << if (i < 0 || i > 255) {
             log_message(-2, "%s:%d: Illegal binary data: %d",
                     filename_, lineNr, i);
             error_ = 1;
             i = 0;
           }

	   if ($len >= MAX_CD_TEXT_DATA_LEN) {
             log_message(-2, "%s:%d: Binary data exceeds maximum length (%d).",
                     filename_, lineNr, MAX_CD_TEXT_DATA_LEN);
             error_ = 1;
           }
           else {
             buf[$len] = i;
             $len += 1;
           }
        >>
      )*
    }
    "\}"
    ;
    // fail action
    << $len = 0; >>
         
cdTextItem [ int blockNr ] > [ CdTextItem *item, int lineNr ]
  : << $item = NULL;
       CdTextItem::PackType type;
       const char *s;
       const unsigned char *data;
       long len;
    >>

    packType > [ type, $lineNr ]
    (  stringEmpty > [ s ] 
       << if (s != NULL) {
            $item = new CdTextItem(type, blockNr, s);
            delete[] s;
          }
       >>
     | binaryData > [ data, len ]
       << $item = new CdTextItem(type, blockNr, data, len); >>
    )
    ;
    // fail action
    << delete $item;
       $item = NULL;
    >>
 
cdTextBlock [ CdTextContainer &container, int isTrack ]
  : << CdTextItem *item = NULL;
       int blockNr;
       int lineNr;
    >>

    Language integer > [ blockNr, lineNr ]
    "\{"
    << if (blockNr < 0 || blockNr > 7) {
         log_message(-2, "%s:%d: Invalid block number, allowed range: [0..7].",
                 filename_, lineNr);
         error_ = 1;
         blockNr = 0;
       }
    >>
    ( cdTextItem [ blockNr ] > [ item, lineNr ]
      << if (item != NULL) {
           int type = item->packType();

           if (isTrack && ((type > 0x86 && type <= 0x89) || type == 0x8f)) {
             log_message(-2, "%s:%d: Invalid CD-TEXT item for a track.",
                     filename_, lineNr);
             error_ = 1;
             delete item;
             item = NULL;
           }
           else {
             container.add(item);
             item = NULL;
           }
         }
      >>
    )*
    "\}"
    ;
    // fail action
    << delete item; >>


cdTextLanguageMap [ CdTextContainer &container ]
  : << int blockNr;
       int lang;
       int blockNrLine;
       int langLine = 0;
    >>

    LanguageMap "\{"
    ( integer > [ blockNr, blockNrLine] ":" 
      (  integer > [ lang, langLine ]
       | LangEn << lang = 9; >>
      )
      << if (blockNr >= 0 && blockNr <= 7) {
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
      >>
    )+
    "\}"
    ;

cdTextTrack [ CdTextContainer &container ]
  :
    CdText "\{"
    ( cdTextBlock [ container, 1 ] )*
    "\}"
    ;

cdTextGlobal [ CdTextContainer &container ]
  :
    CdText "\{"
    { cdTextLanguageMap [ container ] }
    ( cdTextBlock [ container, 0 ] )*
    "\}"
    ;
}


<<
ANTLRTokenType TocLexer::erraction()
{
  log_message(-2, "%s:%d: Illegal token: %s", parser_->filename_,
	  _line, _lextext);
  parser_->error_ = 1;
  return Eof;
}
>>

<<
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
>>
