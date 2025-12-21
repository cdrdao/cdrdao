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

#ifndef __CDTEXTCONTAINER_H__
#define __CDTEXTCONTAINER_H__

#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "CdTextItem.h"
#include "util.h"

class CdTextContainer
{
  public:
    CdTextContainer();
    CdTextContainer(const CdTextContainer &) = default;

    long count() const
    {
        return items_.size();
    }

    void add(CdTextItem *);

    void remove(CdTextItem::PackType, int blockNr);

    void print(int isTrack, std::ostream &, PrintParams &) const;

    // checks if a pack exists for given 'blockNr' (language)
    bool existBlock(int blockNr) const;

    // return pack for given 'blockNr' and pack type
    const CdTextItem *getPack(int blockNr, CdTextItem::PackType) const;

    // sets/returns language code for block nr
    void language(int blockNr, int lang);
    int language(int blockNr) const;

    // sets/returns character encoding for block nr
    void encoding(int blockNr, Util::Encoding enc);
    Util::Encoding encoding(int blockNr) const;
    void enforceEncoding(CdTextContainer *global);

    static const char *languageName(int lang);

    // Allow iteration over CD-TEXT items
    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = CdTextItem;
        using pointer = std::vector<std::shared_ptr<CdTextItem>>::const_iterator;
        using reference = CdTextItem &;

        Iterator(pointer item) : m_ptr(item)
        {
        }
        friend bool operator==(const Iterator &a, const Iterator &b)
        {
            return a.m_ptr == b.m_ptr;
        }
        friend bool operator!=(const Iterator &a, const Iterator &b)
        {
            return a.m_ptr != b.m_ptr;
        }
        Iterator &operator++()
        {
            m_ptr++;
            return *this;
        }
        reference operator*() const
        {
            return *(*m_ptr);
        }

        pointer m_ptr;
    };

    Iterator begin() const
    {
        return Iterator(items_.begin());
    }
    Iterator end() const
    {
        return Iterator(items_.end());
    }

  private:
    std::vector<std::shared_ptr<CdTextItem>> items_;

    std::vector<int> languages;            // mapping from block nr to language code
    std::vector<Util::Encoding> encodings; // mapping from block_nr to encoding

    void setDefaultLanguageMapping();
};

#endif
