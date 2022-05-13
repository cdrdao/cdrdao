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

#ifndef __CDTEXTITEM_H__
#define __CDTEXTITEM_H__

#include <iostream>
#include "util.h"

class CdTextItem {
public:
    enum class DataType { SBCC, DBCC, BINARY };

    enum class PackType {
        TITLE = 0x80,
        PERFORMER = 0x81,
        SONGWRITER = 0x82,
        COMPOSER = 0x83,
        ARRANGER = 0x84,
        MESSAGE = 0x85,
        DISK_ID = 0x86,
        GENRE = 0x87,
        TOC_INFO1 = 0x88,
        TOC_INFO2 = 0x89,
        RES1 = 0x8a,
        RES2 = 0x8b,
        RES3 = 0x8c,
        CLOSED = 0x8d,
        UPCEAN_ISRC = 0x8e,
        SIZE_INFO = 0x8f };

    CdTextItem(PackType packType, int blockNr,
               const u8 *data, size_t len,
               Util::Encoding enc = Util::Encoding::RAW);

    CdTextItem(PackType packType, int blockNr,
               const unsigned char *data, long len);
    CdTextItem(int blockNr, unsigned char genreCode1, unsigned char genreCode2,
               const char *description);

    CdTextItem(const CdTextItem &);

    ~CdTextItem();

    DataType dataType() const { return dataType_; }

    PackType packType() const { return packType_; }

    int blockNr() const { return blockNr_; }

    bool isTrackPack() const { return trackNr_ > 0; }
    void trackNr(int t) { trackNr_ = t; }
    int trackNr() const { return trackNr_; }

    void encoding(Util::Encoding e) { encoding_ = e; }
    Util::Encoding encoding() const { return encoding_; }

    const unsigned char *data() const { return data_; }

    long dataLen() const { return dataLen_; }

    void print(std::ostream &, PrintParams& params) const;

    int operator==(const CdTextItem &);
    int operator!=(const CdTextItem &);

    static const char *packType2String(int isTrack, PackType packType);

    static PackType int2PackType(int);
    static int isBinaryPack(PackType);

private:
    friend class CdTextContainer;

    DataType dataType_;
    PackType packType_;
    int blockNr_; // 0 ... 7
    Util::Encoding encoding_;

    unsigned char *data_;
    long dataLen_;

    // Info fields only, ignored during burn
    int trackNr_;

    CdTextItem *next_;
};

#endif
