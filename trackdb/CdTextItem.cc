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
#include <iomanip>
#include <stdio.h>
#include <string.h>

#include "CdTextItem.h"
#include "log.h"
#include "util.h"

namespace
{

// Converts UTF-8 'text' into the CD-TEXT encoding 'enc', including the
// terminating 0. Characters with no exact equivalent are approximated;
// 'warn' logs when that happens or when the conversion fails.
std::vector<u8> encodeUtf8(const std::string &text, Util::Encoding enc, bool warn)
{
    std::vector<u8> out;
    if (Util::from_utf8(text, out, enc))
        return out;

    out.clear();
    if (Util::from_utf8(text, out, enc, true)) {
        if (warn)
            log_message(-1, "CD-TEXT: \"%s\" cannot be represented exactly in %s, approximating.",
                        text.c_str(), Util::encodingToString(enc));
        return out;
    }

    if (warn)
        log_message(-2, "CD-TEXT: Unable to encode \"%s\" into %s.", text.c_str(),
                    Util::encodingToString(enc));
    out.assign(1, 0);
    return out;
}

void printChar(std::ostream &out, unsigned char c, bool ascii_only)
{
    if (c == '"')
        out << "\\\"";
    else if (c == '\\')
        out << "\\\\";
    else if (ascii_only && (c < 32 || c >= 127))
        out << "\\" << std::oct << std::setfill('0') << std::setw(3) << (unsigned int)c << std::dec
            << std::setfill(' ');
    else
        out << c;
}

std::vector<u8> stringBytes(const std::string &s)
{
    std::vector<u8> bytes(s.begin(), s.end());
    bytes.push_back(0);
    return bytes;
}

} // namespace

// ---------------------------------------------------------------------------
// CdTextItem

CdTextItem::CdTextItem(PackType packType, int blockNr)
    : packType_(packType), blockNr_(blockNr), trackNr_(0)
{
    assert(blockNr >= 0 && blockNr <= 7);
}

std::unique_ptr<CdTextItem> CdTextItem::create(PackType type, int blockNr, const std::string &text,
                                               bool isUtf8)
{
    if (isBinaryPack(type))
        return create(type, blockNr, stringBytes(text));

    if (isUtf8)
        return std::make_unique<CdTextUtf8Item>(type, blockNr, text);

    return std::make_unique<CdTextEncodedItem>(type, blockNr, stringBytes(text));
}

std::unique_ptr<CdTextItem> CdTextItem::create(PackType type, int blockNr, std::vector<u8> data)
{
    if (type == PackType::GENRE)
        return std::make_unique<CdTextGenreItem>(blockNr, std::move(data));

    if (isBinaryPack(type))
        return std::make_unique<CdTextBinaryItem>(type, blockNr, std::move(data));

    return std::make_unique<CdTextEncodedItem>(type, blockNr, std::move(data));
}

void CdTextItem::print(std::ostream &out, PrintParams &params) const
{
    out << packType2String(isTrackPack(), packType_);
    printValue(out, params);
}

bool CdTextItem::operator==(const CdTextItem &obj) const
{
    if (packType_ != obj.packType_ || blockNr_ != obj.blockNr_ || isBinary() != obj.isBinary())
        return false;

    return sameContent(obj);
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

bool CdTextItem::isBinaryPack(PackType type)
{
    switch (type) {
    case PackType::TOC_INFO1:
    case PackType::TOC_INFO2:
    case PackType::SIZE_INFO:
    case PackType::GENRE:
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// CdTextBinaryItem

CdTextBinaryItem::CdTextBinaryItem(PackType packType, int blockNr, std::vector<u8> data)
    : CdTextItem(packType, blockNr), data_(std::move(data))
{
}

void CdTextBinaryItem::printValue(std::ostream &out, const PrintParams &) const
{
    char buf[20];
    long i = 0;

    out << " {";
    for (auto c : data_) {
        if (i > 0) {
            if (i % 12 == 0)
                out << ",\n               ";
            else
                out << ", ";
        }
        snprintf(buf, sizeof(buf), "%2d", c);
        out << buf;
        i++;
    }
    out << "}";
}

bool CdTextBinaryItem::sameContent(const CdTextItem &obj) const
{
    return data_ == static_cast<const CdTextBinaryItem &>(obj).data_;
}

// ---------------------------------------------------------------------------
// CdTextGenreItem

CdTextGenreItem::CdTextGenreItem(int blockNr, std::vector<u8> data)
    : CdTextBinaryItem(PackType::GENRE, blockNr, std::move(data))
{
}

static std::vector<u8> genreData(u8 code1, u8 code2, const char *description)
{
    std::vector<u8> data{code1, code2};
    if (description) {
        while (*description)
            data.push_back(*description++);
        data.push_back(0);
    }
    return data;
}

CdTextGenreItem::CdTextGenreItem(int blockNr, u8 code1, u8 code2, const char *description)
    : CdTextBinaryItem(PackType::GENRE, blockNr, genreData(code1, code2, description))
{
}

std::string CdTextGenreItem::description() const
{
    if (data().size() <= 2)
        return {};

    // The terminating 0 may be missing.
    auto begin = data().begin() + 2;
    auto end = begin;
    while (end != data().end() && *end != 0)
        end++;
    return std::string(begin, end);
}

// ---------------------------------------------------------------------------
// CdTextTextItem

void CdTextTextItem::printValue(std::ostream &out, const PrintParams &params) const
{
    out << " \"";
    if (!params.no_utf8 && hasText()) {
        for (auto c : getText())
            printChar(out, c, false);
    } else {
        for (auto c : packData()) {
            if (c == '\0')
                break;
            printChar(out, c, true);
        }
    }
    out << "\"";
}

bool CdTextTextItem::sameContent(const CdTextItem &obj) const
{
    auto &other = static_cast<const CdTextTextItem &>(obj);

    // The UTF-8 text is authoritative when both sides have it; the
    // encoding may not have been set yet on one of them.
    if (hasText() && other.hasText())
        return getText() == other.getText();

    return packData() == other.packData();
}

// ---------------------------------------------------------------------------
// CdTextUtf8Item

CdTextUtf8Item::CdTextUtf8Item(PackType packType, int blockNr, std::string text)
    : CdTextTextItem(packType, blockNr), text_(std::move(text))
{
}

std::vector<u8> CdTextUtf8Item::packData() const
{
    if (encoding_ == Util::Encoding::UNSET)
        return {};

    return encodeUtf8(text_, encoding_, false);
}

void CdTextUtf8Item::encoding(Util::Encoding e)
{
    // Encode once to report text that does not fit the new encoding.
    if (e != encoding_ && e != Util::Encoding::UNSET)
        encodeUtf8(text_, e, true);

    encoding_ = e;
}

// ---------------------------------------------------------------------------
// CdTextEncodedItem

CdTextEncodedItem::CdTextEncodedItem(PackType packType, int blockNr, std::vector<u8> data)
    : CdTextTextItem(packType, blockNr), data_(std::move(data))
{
}

std::string CdTextEncodedItem::getText() const
{
    if (!hasText() || data_.empty())
        return {};

    return Util::to_utf8(data_.data(), data_.size(), encoding_);
}

void CdTextEncodedItem::encoding(Util::Encoding e)
{
    if (e == encoding_)
        return;

    // Switching from one known encoding to another: transcode, so that
    // the text is kept. Otherwise the bytes are simply (re)interpreted.
    if (encoding_ != Util::Encoding::UNSET && e != Util::Encoding::UNSET) {
        std::string text = getText();
        if (!text.empty())
            data_ = encodeUtf8(text, e, true);
    }

    encoding_ = e;
}
