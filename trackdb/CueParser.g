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
#include <string.h>
#include "Toc.h"
#include "TrackData.h"
#include "util.h"
struct CueTrackData;
>>

<<
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "CueLexerBase.h"

/* Use 'ANTLRCommonToken' as 'ANTLRToken' to be compatible with some bug
 * fix releases of PCCTS-1.33. The drawback is that the token length is
 * limited to 100 characters. This might be a problem for long file names.
 * Define 'USE_ANTLRCommonToken' to 0 if you want to use the dynamic token
 * which handles arbitrary token lengths (there'll be still a limitation in
 * the lexer code but that's more than 100 characters). In this case it might
 * be necessary to adjust the types of the member functions to the types in
 * 'ANTLRAbstractToken' defined in PCCTSDIR/h/AToken.h'.
 */

#define USE_ANTLRCommonToken 1

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

#if 1
  // use this for basic PCCTS-1.33 release
  ANTLRTokenType getType()	 { return type_; }
  virtual int getLine()		 { return line_; }
  ANTLRChar *getText()		 { return text_; }
#else
  // use this for PCCTS-1.33 bug fix releases
  ANTLRTokenType getType() const { return type_; }
  virtual int getLine()    const { return line_; }
  ANTLRChar *getText()	   const { return text_; }
#endif

  void setType(ANTLRTokenType t) { type_ = t; }
  void setLine(int line)	 { line_ = line; }
  void setText(ANTLRChar *s)     { text_ = strdupCC(s); }

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
class CueLexer : public CueLexerBase {
public:
  CueLexer(DLGInputStream *in, unsigned bufsize=2000)
    : CueLexerBase(in, bufsize) { parser_ = NULL; }

  virtual ANTLRTokenType erraction();

  CueParserGram *parser_;   
};
>>

<<
struct CueTrackData {
  TrackData::Mode mode;
  long offset;
  long pregap;
  long indexCnt;
  long index[99];
  unsigned int swap : 1;
  unsigned int copy : 1;
  unsigned int fourChannelAudio : 1;
  unsigned int preemphasis : 1;
};
>>

#lexclass START
#token Eof		"@"
#token                  "[\t\r\ ]+"    << skip(); >>
#token                  "\n"         << newline(); skip(); >>
#token BeginString      "\""         << mode(STRING); >>
#token TrackDef         "TRACK"
#token Audio            "AUDIO"
#token Mode1_2048       "MODE1/2048"
#token Mode1_2352       "MODE1/2352"
#token Mode2_2336       "MODE2/2336"
#token Mode2_2352       "MODE2/2352"
#token Index            "INDEX"
#token File             "FILE"       << mode(FILE); >>
#token Pregap           "PREGAP"
#token Postgap          "POSTGAP"
#token Binary           "BINARY"
#token Motorola         "MOTOROLA"
#token Flags            "FLAGS"
#token Catalog          "CATALOG"   << mode(CATALOG); >>
#token Isrc             "ISRC"      << mode(ISRC); >>
#token                  "4CH"
#token Integer          "[0-9]+"

#lexclass STRING
#token EndString        "\""         << mode(START); >>
#token StringQuote      "\\\""
#token StringOctal      "\\[0-9][0-9][0-9]"
#token String           "[\ ]+"
#token String           "~[\\\n\"\t ]*"

#lexclass FILE
#token                  "\n"           << newline(); skip(); >>
#token                  "[\t\r\ ]*"    << skip(); >>
#token FileName         "~[\t\r\n\ ]*" << mode(START); >>

#lexclass CATALOG
#token                  "\n"          << newline(); skip(); >>
#token                  "[\t\r\ ]*"   << skip(); >>
#token CatalogNr        "[0-9]+"      << mode(START); >>

#lexclass ISRC
#token                  "\n"          << newline(); skip(); >>
#token                  "[\t\r\ ]*"   << skip(); >>
#token IsrcCode         "[0-9A-Z]+"   << mode(START); >>

#lexclass START


class CueParserGram {
<<
public:
  const char *filename_;
  int error_;

  void syn(_ANTLRTokenPtr tok, ANTLRChar *egroup, SetWordType *eset,
	   ANTLRTokenType etok, int k);


>>

cue [ CueTrackData *trackData ] > [ int nofTracks ]
  : << $nofTracks = 0;
       int lineNr = 0;
       int i;
       TrackData::Mode mode;
       int index;
       Msf m;
       int swapSamples = 0;
    >>
    (  File FileName (  Binary << swapSamples = 1; >>
                      | Motorola << swapSamples = 0; >>
                     )
     | Catalog CatalogNr
    )*

    ( TrackDef integer > [ i, lineNr ] 
      << if ($nofTracks >= 99) {
           message(-2, "%s:%d: Too many tracks.", filename_, $1->getLine());
           error_ = 1;
         }
         else {
	   trackData[$nofTracks].pregap = 0;
	   trackData[$nofTracks].swap = (swapSamples ? 1 : 0);
	   trackData[$nofTracks].copy = 0;
	   trackData[$nofTracks].fourChannelAudio = 0;
	   trackData[$nofTracks].preemphasis = 0;
	   index = 0;
	 }
      >>

      trackMode > [ mode ]
      << if ($nofTracks < 99) {
           trackData[$nofTracks].mode = mode;
         }
      >>

      (  Flags (  "DCP" << trackData[$nofTracks].copy = 1; >>
                | "4CH" << trackData[$nofTracks].fourChannelAudio = 1; >>
                | "PRE" << trackData[$nofTracks].preemphasis = 1; >>
               )+
        | Isrc IsrcCode << message(0, "%s", $2->getText()); >>
      )*

      { Pregap msf > [ m ] 
        << if ($nofTracks < 99) {
             trackData[$nofTracks].pregap = m.lba();
	   }
        >>
      }

      Index integer > [ i, lineNr ] msf > [ m ]
      << if ($nofTracks < 99) {
           trackData[$nofTracks].offset = m.lba();

	   if (i == 1) {
	     trackData[$nofTracks].index[index] = m.lba();
	     index++;
	   }
         }
      >>

      ( Index integer > [ i, lineNr ] msf > [ m ]
        << if ($nofTracks < 99) {
             if (index < 99) {
               trackData[$nofTracks].index[index] = m.lba();
               index++;
	     }
	     else {
	       message(-2, "%s:%d: Too many index marks.",
		       filename_, $1->getLine());
	       error_ = 1;
	     }
	   }
        >>
      )*

      { Postgap msf > [ m ] }

      << if ($nofTracks < 99) {
           if (index == 0) {
	     message(-2, "%s:%d: Track %d: Missing Index 1.",
		     filename_, $1->getLine(), $nofTracks + 1);
	     error_ = 1;
	   }
	   trackData[$nofTracks].indexCnt = index;
	   $nofTracks += 1;
         }
      >>
    )+

    Eof
    ;
    // fail action
    << $nofTracks = 0;
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
	 fprintf(stderr, "%s:%d: Illegal minute field: %d\n", filename_,
	         minLine, min);
         err = error_ = 1;
       }
       if (sec < 0 || sec > 59) {
	 fprintf(stderr, "%s:%d: Illegal second field: %d\n", filename_,
	         secLine, sec);
	 err = error_ = 1;
       }
       if (frac < 0 || frac > 74) {
	 fprintf(stderr, "%s:%d: Illegal fraction field: %d\n", filename_,
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

trackMode > [ TrackData::Mode m ]
  :
    (  Audio <<  $m = TrackData::AUDIO; >>
     | Mode1_2048 << $m = TrackData::MODE1; >>
     | Mode1_2352 << $m = TrackData::MODE1_RAW; >>
     | Mode2_2336 << $m = TrackData::MODE2_FORM_MIX; >>
     | Mode2_2352 << $m = TrackData::MODE2_RAW; >>
    )
    ;

}


<<
ANTLRTokenType CueLexer::erraction()
{
  message(-2, "%s:%d: Illegal token: %s", parser_->filename_,
	  _line, _lextext);
  parser_->error_ = 1;
  return Eof;
}
>>

<<
void CueParserGram::syn(_ANTLRTokenPtr tok, ANTLRChar *egroup,
			SetWordType *eset, ANTLRTokenType etok, int k)
{
  int line;

  error_ = 1;
  line = LT(1)->getLine();

  fprintf(stderr, "%s:%d: syntax error at \"%s\"", filename_, line,
       	  LT(1)->getType() == Eof ? "EOF" : LT(1)->getText());
  if ( !etok && !eset ) {
    fprintf(stderr, "\n");
    return;
  }
  if ( k==1 ) {
    fprintf(stderr, " missing");
  }
  else {
    fprintf(stderr, "; \"%s\" not", LT(1)->getText());
    if ( set_deg(eset)>1 ) fprintf(stderr, " in");
  }
  if ( set_deg(eset)>0 )
    edecode(eset);
  else fprintf(stderr, " %s", token_tbl[etok]);

  if ( strlen(egroup) > 0 )
    fprintf(stderr, " in %s", egroup);
	
  fprintf(stderr, "\n");
}
>>

<<
static Toc *createToc(const char *cueFileName, const char *binFileName,
		      CueTrackData *trackData, int nofTracks)
{
  int t, i;
  int fd, ret;
  struct stat sbuf;
  long binFileLength;
  long byteOffset = 0;
  long blockLen;
  TrackData::SubChannelMode subChanMode = TrackData::SUBCHAN_NONE;

  if ((fd = open(binFileName, O_RDONLY)) < 0) {
    message(-2, "Cannot open bin file '%s': %s", binFileName, strerror(errno));
    return NULL;
  }

  ret = fstat(fd, &sbuf);
  close(fd);

  if (ret != 0) {
    message(-2, "Cannot access bin file '%s': %s", binFileName,
	    strerror(errno));
    return NULL;
  }

  binFileLength = sbuf.st_size;

  Toc *toc = new Toc; 
  Msf trackLength;
  
  toc->tocType(Toc::CD_DA);
  
  for (t = 0; t < nofTracks; t++) {
    Track track(trackData[t].mode, subChanMode);

    blockLen = TrackData::dataBlockSize(trackData[t].mode, subChanMode);
    
    // very simplified guessing of toc type
    switch (trackData[t].mode) {
    case TrackData::MODE1:
    case TrackData::MODE1_RAW:
    case TrackData::MODE2:
      toc->tocType(Toc::CD_ROM);
      break;
    case TrackData::MODE2_RAW:
    case TrackData::MODE2_FORM_MIX:
      toc->tocType(Toc::CD_ROM_XA);
      break;
    default:
      break;
    }

    track.preEmphasis(trackData[t].preemphasis);
    track.copyPermitted(trackData[t].copy);
    track.audioType(trackData[t].fourChannelAudio);

    if (t < nofTracks - 1) {
      if (trackData[t + 1].offset <= trackData[t].offset) {
	message(-2, "%s: Invalid start offset for track %d.", 
		cueFileName, t + 2);
	delete toc;
	return NULL;
      }

      trackLength = Msf(trackData[t + 1].offset - trackData[t].offset);
    }
    else {
      if (byteOffset >= sbuf.st_size) {
	message(-2, "Length of bin file \"%s\" does not match cue file.",
		binFileName);
	delete toc;
	return NULL;
      }

      if (((sbuf.st_size - byteOffset) % blockLen) != 0) {
	message(-1,
		"Length of bin file \"%s\" is not a multiple of block size.",
		binFileName);
      }

      trackLength = Msf((sbuf.st_size - byteOffset) / blockLen);
    }

    /*
    if (trackLength.lba() < 4 * 75) {
      message(-2, "%s: Track %d is shorter than 4 seconds.", cueFileName,
	      t + 1);
      delete toc;
      return NULL;
    }
    */

    if (trackData[t].mode == TrackData::AUDIO) {
      if (trackData[t].pregap > 0) {
	track.append(SubTrack(SubTrack::DATA,
			      TrackData(Msf(trackData[t].pregap).samples())));
      }

      SubTrack st(SubTrack::DATA, TrackData(binFileName, byteOffset, 0,
					    trackLength.samples()));

      st.swapSamples(trackData[t].swap);

      track.append(st);
    }
    else {
      if (trackData[t].pregap > 0) {
	track.append(SubTrack(SubTrack::DATA, 
			      TrackData(trackData[t].mode, subChanMode,
					trackData[t].pregap * blockLen)));
      }
      
      track.append(SubTrack(SubTrack::DATA,
			    TrackData(trackData[t].mode, subChanMode,
				      binFileName, byteOffset,
				      trackLength.lba() * blockLen)));
    }

    if (trackData[t].index[0] < trackData[t].offset) {
      message(-2, "%s: Invalid offset for Index 1 of track %d.",
	      cueFileName, t + 1);
      delete toc;
      return NULL;
    }
    
    track.start(Msf(trackData[t].pregap +
		    trackData[t].index[0] - trackData[t].offset));

    for (i = 1; i < trackData[t].indexCnt; i++) {
      if (trackData[t].index[i] <= trackData[t].index[i - 1]) {
	message(-2, "%s: Invalid offset for index %d of track %d.",
		cueFileName, i + 1, t + 1);
	delete toc;
	return NULL;
      }

      track.appendIndex(Msf(trackData[t].index[i] - trackData[t].index[0]));
    }

    toc->append(&track);

    byteOffset += trackLength.lba() * blockLen;
  }

  return toc;
}
>>

<<
Toc *parseCue(FILE *fp, const char *filename)
{
  char *s, *p;
  char *binFileName;

  DLGFileInput in(fp);
  CueLexer scan(&in);
  ANTLRTokenBuffer pipe(&scan);
  ANTLRToken aToken;
  scan.setToken(&aToken);
  CueParserGram parser(&pipe);

  parser.filename_ = filename;
  scan.parser_ = &parser;

  parser.error_ = 0;
  parser.init();

  CueTrackData *trackData = new CueTrackData[99];

  int nofTracks = parser.cue(trackData);

  if (parser.error_ != 0 || nofTracks <= 0) {
    delete[] trackData;
    return NULL;
  }

  s = strdupCC(filename);
  if ((p = strrchr(s, '.')) != NULL)
    *p  = 0;

  binFileName = strdup3CC(s, ".bin", NULL);
  delete[] s;

  Toc *toc = createToc(filename, binFileName, trackData, nofTracks);

  delete[] binFileName;
  delete[] trackData;

  return toc;
}
>>
