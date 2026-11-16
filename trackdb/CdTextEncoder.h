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

#ifndef __CD_TEXT_ENCODER_H__
#define __CD_TEXT_ENCODER_H__

#include <vector>

#include "CdTextItem.h"
#include "util.h"

class Toc;

// One 18 byte CD-TEXT pack as stored in the lead-in R-W sub-channel, and
// as returned by the READ TOC/PMA/ATIP (format 5) MMC command.
struct CdTextPack {
    unsigned char packType;
    unsigned char trackNumber;
    unsigned char sequenceNumber;
    unsigned char blockCharacter;
    unsigned char data[12];
    unsigned char crc0;
    unsigned char crc1;
};

static_assert(sizeof(CdTextPack) == 18, "CdTextPack must be 18 bytes");

// Builds the CD-TEXT packs (including SIZE_INFO packs and CRCs) for the
// CD-TEXT data of a Toc.
class CdTextEncoder
{
  public:
    CdTextEncoder(const Toc *);
    virtual ~CdTextEncoder();

    // Builds the packs. Returns 0 on success.
    virtual int encode();

    // Packs built by encode(), in lead-in order.
    std::vector<CdTextPack> getPacks() const;
    long packCount() const { return packCount_; }

    static u8 characterCode(Util::Encoding);

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

    static unsigned short CRCTAB_[256];

    void appendPack(CdTextPackEntry *);

    void buildPacks();
    void buildPacks(int blockNr, CdTextItem::PackType type);
    void buildSizeInfoPacks();
    void calcCrcs();

    void encodeCdTextItem(int, int, const CdTextItem *);
};

#endif
