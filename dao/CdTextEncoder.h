/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999 Andreas Mueller <mueller@daneb.ping.de>
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

/*
 * $Log: CdTextEncoder.h,v $
 * Revision 1.1.1.1  2000/02/05 01:34:49  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/06/13 19:31:15  mueller
 * Initial revision
 *
 */

#ifndef __CD_TEXT_ENCODER_H__
#define __CD_TEXT_ENCODER_H__

#include "CdrDriver.h"
#include "CdTextItem.h"

class CdTextItem;
class PWSubChannel96;

class CdTextEncoder {
public:
  CdTextEncoder(const Toc *);
  ~CdTextEncoder();

  int encode();

  const PWSubChannel96 **getSubChannels(long *subChannelCount);

private:
  struct CdTextSizeInfo {
    unsigned char characterCode;
    unsigned char firstTrack;
    unsigned char lastTrack;
    unsigned char copyright;
    unsigned char packTypeCount[16];
    unsigned char lastSequenceNumber[8];
    unsigned char languageCode[8];
  };

  const Toc *toc_;
  
  CdTextSizeInfo sizeInfo_[8];

  long packCount_;
  class CdTextPackEntry *packs_;
  class CdTextPackEntry *lastPack_;
  int lastPackPos_; // number of characters written to last pack

  long packId_;

  PWSubChannel96 **subChannels_;
  long subChannelCount_;

  static unsigned short CRCTAB_[256];

  void appendPack(CdTextPackEntry *);

  void buildPacks();
  void buildPacks(int blockNr, CdTextItem::PackType type);
  void buildSizeInfoPacks();
  void calcCrcs();
  void buildSubChannels();

  void encodeCdTextItem(int, int, const CdTextItem *);

};

#endif
