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

#include <algorithm>
#include <assert.h>

#include "CdTextContainer.h"
#include "CdTextItem.h"

struct LanguageCode {
    int code;
    const char *name;
};

static LanguageCode LANGUAGE_CODES[] = {
    {0x75, "Chinese"}, {0x06, "Czech"},     {0x07, "Danish"},  {0x1d, "Dutch"},
    {0x09, "English"}, {0x27, "Finnish"},   {0x0f, "French"},  {0x08, "German"},
    {0x70, "Greek"},   {0x1b, "Hungarian"}, {0x15, "Italian"}, {0x69, "Japanese"},
    {0x65, "Korean"},  {0x1e, "Norwegian"}, {0x20, "Polish"},  {0x21, "Portuguese"},
    {0x56, "Russian"}, {0x26, "Slovene"},   {0x0a, "Spanish"}, {0x28, "Swedish"}};

static int NOF_LANGUAGE_CODES = sizeof(LANGUAGE_CODES) / sizeof(LanguageCode);

CdTextContainer::CdTextContainer() : languages(8, -1), encodings(8, Util::Encoding::UNSET)
{
}

void CdTextContainer::print(int isTrack, std::ostream &out, PrintParams &params) const
{
    int i;
    int foundLanguageMapping;

    if (!items_.empty()) {
        int actBlockNr = (*items_.begin())->blockNr();

        out << "CD_TEXT {\n";

        if (!isTrack) {
            foundLanguageMapping = 0;
            for (i = 0; i < 8; i++) {
                if (languages[i] >= 0) {
                    foundLanguageMapping = 1;
                    break;
                }
            }

            if (foundLanguageMapping) {
                out << "  LANGUAGE_MAP {\n";

                for (i = 0; i < 8; i++) {
                    if (languages[i] >= 0)
                        out << "    " << i << ": " << languages[i] << "\n";
                }

                out << "  }\n";
            }
        }

        out << "  LANGUAGE " << actBlockNr << " {\n";

        for (const auto &item : items_) {
            if (item->blockNr() != actBlockNr) {
                actBlockNr = item->blockNr();
                out << "  }\n  LANGUAGE " << actBlockNr << " {\n";
            }
            out << "    ";
            item->print(out, params);
            out << "\n";
        }

        out << "  }\n}\n";
    }
}

void CdTextContainer::add(CdTextItem *item)
{
    remove(item->packType(), item->blockNr());

    items_.emplace_back(item);

    std::sort(items_.begin(), items_.end(),
              [](std::shared_ptr<CdTextItem> a, std::shared_ptr<CdTextItem> b) {
                  if (a->blockNr() != b->blockNr())
                      return a->blockNr() < b->blockNr();
                  else
                      return a->packType() < b->packType();
              });
}

void CdTextContainer::remove(CdTextItem::PackType type, int blockNr)
{
    for (auto it = items_.begin(); it != items_.end(); it++) {
        if ((*it)->packType() == type && (*it)->blockNr() == blockNr)
            items_.erase(it--);
    }
}

bool CdTextContainer::existBlock(int blockNr) const
{
    for (const auto &item : items_)
        if (item->blockNr() == blockNr)
            return true;

    return false;
}

const CdTextItem *CdTextContainer::getPack(int blockNr, CdTextItem::PackType type) const
{
    for (const auto &item : items_)
        if (item->blockNr() == blockNr && item->packType() == type)
            return item.get();

    return nullptr;
}

void CdTextContainer::language(int blockNr, int lang)
{
    assert(blockNr >= 0 && blockNr <= 7);
    assert(lang >= -1 && lang <= 255);

    languages[blockNr] = lang;
}

int CdTextContainer::language(int blockNr) const
{
    assert(blockNr >= 0 && blockNr <= 7);

    return languages[blockNr];
}

void CdTextContainer::encoding(int blockNr, Util::Encoding enc)
{
    assert(blockNr >= 0 && blockNr <= 7);

    encodings[blockNr] = enc;
}

void CdTextContainer::enforceEncoding(CdTextContainer *global)
{
    if (global == this) {
        // if an encoding is unset, set it based on the size_info
        for (int i = 0; i < encodings.size(); i++) {
            if (encodings[i] == Util::Encoding::UNSET) {
                const CdTextItem *size_info = getPack(i, CdTextItem::PackType::SIZE_INFO);
                if (size_info != NULL && size_info->dataLen() > 0) {
                    encodings[i] = Util::characterCodeToEncoding(size_info->data()[0]);
                } else {
                    encodings[i] = Util::Encoding::LATIN;
                }
            }
        }
    } else {
        encodings = global->encodings;
    }

    for (auto i : items_)
        i->encoding(encodings[i->blockNr()]);
}

Util::Encoding CdTextContainer::encoding(int blockNr) const
{
    assert(blockNr >= 0 && blockNr <= 7);
    return encodings[blockNr];
}

const char *CdTextContainer::languageName(int lang)
{
    int i;

    if (lang < 0)
        return "Undefined";

    for (i = 0; i < NOF_LANGUAGE_CODES; i++) {
        if (lang == LANGUAGE_CODES[i].code)
            return LANGUAGE_CODES[i].name;
    }

    return "Unknown";
}
