/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

#ifndef __PQ_CHANNEL_ENCODER_H__
#define __PQ_CHANNEL_ENCODER_H__

#include "Msf.h"
#include "SubChannel.h"

struct CueSheetEntry {
  unsigned char ctlAdr;
  unsigned char trackNr;
  unsigned char indexNr;
  unsigned char dataForm;
  unsigned char scms;
  unsigned char min;
  unsigned char sec;
  unsigned char frame;
};

class PQChannelEncoder {
public:
  struct SessionInfo {
    // required for all sessions
    int sessionNr;
    long leadInStart;
    long leadInLen;
    long leadOutLen;
    
    // required for all sessions of a multi session disk
    Msf lastLeadoutStart;

    // required for 1st session of a multi session disk
    int cdrw;
    Msf atipLeadinStart;
    unsigned char optimumRecordingPower;

    // required for a CD-RW for 1st session of a multi session disk
    unsigned char atipA1[3];
  };

  PQChannelEncoder();
  ~PQChannelEncoder();

  // sets cue sheet and initializes the module, 'len' is length of cure
  // sheet in bytes
  int setCueSheet(SubChannel *, unsigned char discType,
		  const unsigned char *, int len,
		  const Msf &leadInStart);

  // encodes 'blocks' blocks of data in 'in' and places them appended by
  // PQ sub channel data in caller provided buffer 'out'
  void encode(long lba, unsigned char *out, long blocks);

private:
  SubChannel *subChannel_; // template for all sub channel objects

  CueSheetEntry *cueSheet_;
  int cueSheetLen_;
  unsigned char discType_;

  int firstTrackNr_;
  int lastTrackNr_;
  Msf leadOutStart_;

  unsigned char firstTrackCtlAdr_;
  unsigned char lastTrackCtlAdr_;
  unsigned char leadOutCtlAdr_;

  CueSheetEntry *actCueSheetEntry_;
  long trlba_; // track relative lba
  long abslba_;  // absolute lba on disc
  long nextTransitionLba_;
  long nextTrackStartLba_;

  SubChannel **toc_;
  int tocLen_;
  int actTocEnt_; // actual toc entry 
  int actTocCount_; // counter for reapeating each toc entry three times

  SubChannel *catalog_;
  int writeCatalog_;
  int deferredCatalog_;

  SubChannel *isrc_;
  int writeIsrc_;
  int deferredIsrc_;

  SubChannel *current_;

  int analyzeCueSheet();
  const SubChannel *encodeSubChannel(long lba);
  void nextTransition();
  CueSheetEntry *nextCueSheetEntry(CueSheetEntry *act, int adr);
  CueSheetEntry *nextCueSheetEntry(CueSheetEntry *act, int trackNr,
				   int indexNr);

};

#endif
