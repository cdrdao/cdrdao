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

#include <assert.h>
#include <ctype.h>
#include <iomanip>
#include <stdio.h>
#include <string.h>

#include "CdTextContainer.h"
#include "CdTextItem.h"
#include "log.h"
#include "util.h"

CdTextItem::CdTextItem(PackType packType, int blockNr)
{
    assert(blockNr >= 0 && blockNr <= 7);
    packType_ = packType;
    blockNr_ = blockNr;
    trackNr_ = 0;
    encoding_ = Util::Encoding::UNSET;
}

void CdTextItem::setData(const u8 *buffer, size_t buffer_len)
{
    data_.resize(buffer_len);
    memcpy(data_.data(), buffer, buffer_len);
    dataType_ = DataType::BINARY;
}

void CdTextItem::setRawText(const u8 *buffer, size_t buffer_len)
{
    setData(buffer, buffer_len);
    dataType_ = DataType::SBCC;
    updateEncoding();
}

void CdTextItem::setRawText(const std::string &str)
{
    data_.resize(str.size() + 1);
    auto writer = data_.begin();
    for (const auto c : str)
        *writer++ = c;
    *writer++ = '\0';
    dataType_ = DataType::SBCC;
    updateEncoding();
}

void CdTextItem::setText(const char *utf8_text)
{
    u8text = utf8_text;
    dataType_ = DataType::SBCC;
    updateEncoding();
}

void CdTextItem::setTextFromToc(const char *text)
{
    if (Util::isValidUTF8(text))
        setText(text);
    else
        setRawText((u8 *)text, strlen(text));
}

void CdTextItem::setGenre(u8 genreCode1, u8 genreCode2, const char *description)
{
    dataType_ = DataType::BINARY;

    data_.push_back(genreCode1);
    data_.push_back(genreCode2);

    if (description) {
        const char *ptr = description;
        while (*ptr)
            data_.push_back(*ptr++);
        data_.push_back(0);
    }
}

void CdTextItem::print(std::ostream &out, PrintParams &params) const
{
    char buf[20];
    out << packType2String(isTrackPack(), packType_);

    auto printchar = [&](unsigned char c, bool ascii_only) {
        if (c == '"')
            out << "\\\"";
        else if (c == '\\')
            out << "\\\\";
        else if (ascii_only && (c < 32 || c >= 127))
            out << "\\" << std::oct << std::setfill('0') << std::setw(3) << (unsigned int)c;
        else
            out << c;
    };

    if (dataType() == DataType::SBCC) {
        out << " \"";
        if (params.no_utf8 || u8text.empty()) {
            for (auto c : data_) {
                if (c == '\0')
                    break;
                printchar(c, true);
            }
        } else {
            for (auto c : u8text)
                printchar(c, false);
        }
        out << "\"";
    } else {
        long i = 0;

        out << " {";
        for (auto c : data_) {
            if (i == 0) {
                snprintf(buf, sizeof(buf), "%2d", c);
                out << buf;
            } else {
                if (i % 12 == 0)
                    out << ",\n               ";
                else
                    out << ", ";

                snprintf(buf, sizeof(buf), "%2d", c);
                out << buf;
            }
            i++;
        }

        out << "}";
    }
}

int CdTextItem::operator==(const CdTextItem &obj)
{
    return !(packType_ != obj.packType_ || blockNr_ != obj.blockNr_ || dataType_ != obj.dataType_ ||
             data_ != obj.data_ || u8text != obj.u8text);
}

int CdTextItem::operator!=(const CdTextItem &obj)
{
    return (*this == obj) ? 0 : 1;
}

const char *CdTextItem::packType2String(int isTrack, PackType packType)
{
    const char *ret = "UNKNOWN";

    switch (packType) {
    case PackType::TITLE:
        ret = "TITLE";
        break;
    case PackType::PERFORMER:
        ret = "PERFORMER";
        break;
    case PackType::SONGWRITER:
        ret = "SONGWRITER";
        break;
    case PackType::COMPOSER:
        ret = "COMPOSER";
        break;
    case PackType::ARRANGER:
        ret = "ARRANGER";
        break;
    case PackType::MESSAGE:
        ret = "MESSAGE";
        break;
    case PackType::DISK_ID:
        ret = "DISC_ID";
        break;
    case PackType::GENRE:
        ret = "GENRE";
        break;
    case PackType::TOC_INFO1:
        ret = "TOC_INFO1";
        break;
    case PackType::TOC_INFO2:
        ret = "TOC_INFO2";
        break;
    case PackType::RES1:
        ret = "RESERVED1";
        break;
    case PackType::RES2:
        ret = "RESERVED2";
        break;
    case PackType::RES3:
        ret = "RESERVED3";
        break;
    case PackType::CLOSED:
        ret = "CLOSED";
        break;
    case PackType::UPCEAN_ISRC:
        if (isTrack)
            ret = "ISRC";
        else
            ret = "UPC_EAN";
        break;
    case PackType::SIZE_INFO:
        ret = "SIZE_INFO";
        break;
    }

    return ret;
}

CdTextItem::PackType CdTextItem::int2PackType(int i)
{
    PackType t = PackType::TITLE;

    switch (i) {
    case 0x80:
        t = PackType::TITLE;
        break;
    case 0x81:
        t = PackType::PERFORMER;
        break;
    case 0x82:
        t = PackType::SONGWRITER;
        break;
    case 0x83:
        t = PackType::COMPOSER;
        break;
    case 0x84:
        t = PackType::ARRANGER;
        break;
    case 0x85:
        t = PackType::MESSAGE;
        break;
    case 0x86:
        t = PackType::DISK_ID;
        break;
    case 0x87:
        t = PackType::GENRE;
        break;
    case 0x88:
        t = PackType::TOC_INFO1;
        break;
    case 0x89:
        t = PackType::TOC_INFO2;
        break;
    case 0x8a:
        t = PackType::RES1;
        break;
    case 0x8b:
        t = PackType::RES2;
        break;
    case 0x8c:
        t = PackType::RES3;
        break;
    case 0x8d:
        t = PackType::CLOSED;
        break;
    case 0x8e:
        t = PackType::UPCEAN_ISRC;
        break;
    case 0x8f:
        t = PackType::SIZE_INFO;
        break;
    }

    return t;
}

int CdTextItem::isBinaryPack(PackType type)
{
    int ret;

    switch (type) {
    case PackType::TOC_INFO1:
    case PackType::TOC_INFO2:
    case PackType::SIZE_INFO:
    case PackType::GENRE:
        ret = 1;
        break;

    default:
        ret = 0;
        break;
    }

    return ret;
}

void CdTextItem::encoding(Util::Encoding e)
{
    encoding_ = e;

    updateEncoding();
}

void CdTextItem::updateEncoding()
{
    if (encoding_ != Util::Encoding::UNSET) {
        if (u8text.empty()) {
            u8text = Util::to_utf8(data(), dataLen(), encoding_);
        } else if (data_.empty()) {
            if (!Util::from_utf8(u8text, data_, encoding_))
                log_message(-2, "CD-TEXT: Unable to encode \"%s\" into compatible format");
        }
    }
}
