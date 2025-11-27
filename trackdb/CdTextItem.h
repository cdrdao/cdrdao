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
#include <vector>
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

    CdTextItem(PackType packType, int blockNr);
    CdTextItem(const CdTextItem &) = default;

    void setData(const u8* buffer, size_t buffer_len);
    void setRawText(const u8* buffer, size_t buffer_len);
    void setRawText(const std::string &buffer);
    void setText(const char* utf8_text);
    void setTextFromToc(const char* text);
    void setGenre(u8 genreCode1, u8 genreCode2, const char *description);

    DataType dataType() const { return dataType_; }
    PackType packType() const { return packType_; }

    int blockNr() const { return blockNr_; }

    bool isTrackPack() const { return trackNr_ > 0; }
    void trackNr(int t) { trackNr_ = t; }
    int trackNr() const { return trackNr_; }

    void encoding(Util::Encoding e);
    Util::Encoding encoding() const { return encoding_; }

    const u8* data() const { return data_.data(); }
    size_t dataLen() const { return data_.size(); }

    const std::string& getText() const { return u8text; }

    void print(std::ostream &, PrintParams& params) const;

    int operator==(const CdTextItem &);
    int operator!=(const CdTextItem &);

    static const char *packType2String(int isTrack, PackType packType);

    static PackType int2PackType(int);
    static int isBinaryPack(PackType);

private:
    DataType dataType_;
    PackType packType_;
    int blockNr_; // 0 ... 7
    Util::Encoding encoding_;

    // We potentially keep two copies of the CD-TEXT: the data_ vector
    // represents what will actulally be burned onto the CD, and can
    // be either binary data or text data, encoded using one of the
    // officially supported CD encodings (i.e. not UTF-8). The u8text
    // string, is set, represents the desired textual encoding and
    // comes from either the TOC file or from the gcdmaster dialog
    // boxes. That UTF-8 string will get translated into ASCII or
    // ISO-8859-1 as best as possible.

    // Raw binary content, or pre-encoded text content
    std::vector<u8> data_;

    // UTF-8 text content.
    std::string u8text;

    // Info fields only, ignored during burn
    int trackNr_;

    void updateEncoding();
};

#endif
