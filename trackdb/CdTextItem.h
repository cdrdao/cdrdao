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

#include "util.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// A single CD-TEXT item (one pack type, for one language block, for
// the disc or for one track).
//
// The hierarchy separates the binary packs from the text packs:
//
//   CdTextItem             abstract base: pack type, block, track
//   +- CdTextBinaryItem    raw bytes (TOC_INFO1/2, SIZE_INFO, ...)
//   |  +- CdTextGenreItem  GENRE: two genre codes + optional description
//   +- CdTextTextItem      abstract text item, with a CD-TEXT encoding
//      +- CdTextUtf8Item     text known as UTF-8 (TOC file, GUI, CDDB);
//      |                     the bytes to burn are derived from it
//      +- CdTextEncodedItem  text known only as bytes in a CD-TEXT
//                            encoding (read from disc, non-UTF-8 TOC
//                            string); the UTF-8 text is derived from it
//
// Each item stores exactly one representation of its content. Use the
// create() factories to get the right subclass for a pack type.
class CdTextItem
{
  public:
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
        SIZE_INFO = 0x8f
    };

    virtual ~CdTextItem() = default;

    // Factory for a string value (TOC file string, GUI entry, CDDB).
    // 'isUtf8' tells whether 'text' is UTF-8 or bytes already in the
    // block's CD-TEXT encoding. For a binary pack type, the string bytes
    // (plus a terminating 0) become the binary content.
    static std::unique_ptr<CdTextItem> create(PackType, int blockNr, const std::string &text,
                                              bool isUtf8);

    // Factory for a byte value (TOC file "{ ... }" list, packs read from
    // disc). For a text pack type, the bytes are text in the block's
    // CD-TEXT encoding.
    static std::unique_ptr<CdTextItem> create(PackType, int blockNr, std::vector<u8> data);

    static std::unique_ptr<CdTextItem> fromUtf8(PackType type, int blockNr, const std::string &text)
    {
        return create(type, blockNr, text, true);
    }

    PackType packType() const { return packType_; }
    int blockNr() const { return blockNr_; }

    bool isTrackPack() const { return trackNr_ > 0; }
    void trackNr(int t) { trackNr_ = t; }
    int trackNr() const { return trackNr_; }

    virtual bool isBinary() const = 0;

    // The bytes that go into the CD-TEXT packs.
    virtual std::vector<u8> packData() const = 0;

    virtual bool empty() const = 0;

    // UTF-8 text content. Empty for binary items, and for encoded text
    // items whose encoding is not known yet.
    virtual std::string getText() const { return {}; }

    // Sets the CD-TEXT encoding of the item's language block. Ignored
    // by binary items.
    virtual void encoding(Util::Encoding) {}

    void print(std::ostream &, PrintParams &params) const;

    bool operator==(const CdTextItem &) const;
    bool operator!=(const CdTextItem &obj) const { return !(*this == obj); }

    static const char *packType2String(int isTrack, PackType packType);

    static PackType int2PackType(int);
    static bool isBinaryPack(PackType);

  protected:
    CdTextItem(PackType packType, int blockNr);
    CdTextItem(const CdTextItem &) = default;
    CdTextItem &operator=(const CdTextItem &) = delete;

    // Prints the value part (after the pack type name).
    virtual void printValue(std::ostream &, const PrintParams &) const = 0;

    // Compares content with an item of the same kind (binary or text).
    virtual bool sameContent(const CdTextItem &) const = 0;

  private:
    PackType packType_;
    int blockNr_; // 0 ... 7

    // Info field only, ignored during burn
    int trackNr_;
};

class CdTextBinaryItem : public CdTextItem
{
  public:
    CdTextBinaryItem(PackType packType, int blockNr, std::vector<u8> data);

    const std::vector<u8> &data() const { return data_; }

    bool isBinary() const override { return true; }
    std::vector<u8> packData() const override { return data_; }
    bool empty() const override { return data_.empty(); }

  protected:
    void printValue(std::ostream &, const PrintParams &) const override;
    bool sameContent(const CdTextItem &) const override;

  private:
    std::vector<u8> data_;
};

class CdTextGenreItem : public CdTextBinaryItem
{
  public:
    CdTextGenreItem(int blockNr, std::vector<u8> data);
    CdTextGenreItem(int blockNr, u8 code1, u8 code2, const char *description);

    bool hasCodes() const { return data().size() >= 2; }
    u8 code1() const { return hasCodes() ? data()[0] : 0; }
    u8 code2() const { return hasCodes() ? data()[1] : 0; }
    std::string description() const;

    bool empty() const override { return !hasCodes(); }
};

class CdTextTextItem : public CdTextItem
{
  public:
    bool isBinary() const override { return false; }

    Util::Encoding encoding() const { return encoding_; }
    using CdTextItem::encoding;

    // True if the UTF-8 text is available (see getText()).
    virtual bool hasText() const = 0;

  protected:
    CdTextTextItem(PackType packType, int blockNr) : CdTextItem(packType, blockNr) {}

    void printValue(std::ostream &, const PrintParams &) const override;
    bool sameContent(const CdTextItem &) const override;

    Util::Encoding encoding_ = Util::Encoding::UNSET;
};

class CdTextUtf8Item final : public CdTextTextItem
{
  public:
    CdTextUtf8Item(PackType packType, int blockNr, std::string text);

    std::vector<u8> packData() const override;
    bool empty() const override { return text_.empty(); }
    std::string getText() const override { return text_; }
    bool hasText() const override { return true; }
    void encoding(Util::Encoding e) override;
    using CdTextTextItem::encoding;

  private:
    std::string text_;
};

class CdTextEncodedItem final : public CdTextTextItem
{
  public:
    CdTextEncodedItem(PackType packType, int blockNr, std::vector<u8> data);

    std::vector<u8> packData() const override { return data_; }
    bool empty() const override { return data_.empty() || data_[0] == 0; }
    std::string getText() const override;
    bool hasText() const override { return encoding_ != Util::Encoding::UNSET; }
    void encoding(Util::Encoding e) override;
    using CdTextTextItem::encoding;

  private:
    std::vector<u8> data_;
};

#endif
