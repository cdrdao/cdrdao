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

#include <iterator>
#include <iostream>

#include "util.h"
#include "CdTextItem.h"


class CdTextContainer {
public:
    CdTextContainer();
    CdTextContainer(const CdTextContainer &);

    ~CdTextContainer();

    long count() const { return count_; }

    void add(CdTextItem *);

    void remove(CdTextItem::PackType, int blockNr);

    void print(int isTrack, std::ostream &, PrintParams&) const;

    // checks if a pack exists for given 'blockNr' (language)
    int existBlock(int blockNr) const;

    // return pack for given 'blockNr' and pack type
    const CdTextItem *getPack(int blockNr, CdTextItem::PackType) const;

    // sets/returns language code for block nr
    void language(int blockNr, int lang);
    int language(int blockNr) const;

    // sets/returns character encoding for block nr
    void encoding(int blockNr, Util::Encoding enc);
    Util::Encoding encoding(int blockNr) const;

    static const char *languageName(int lang);

    // Allow iteration over CD-TEXT items
    struct Iterator {
        using iterator_category = std::input_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type = CdTextItem;
        using pointer = CdTextItem*;
        using reference = CdTextItem&;

        Iterator(CdTextItem* item) : m_ptr(item) {}
        friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }
        Iterator& operator++() { m_ptr = m_ptr->next_; return *this; }
        reference operator*() const { return *m_ptr; }

        pointer m_ptr;
    };

    Iterator begin() { return Iterator(items_); }
    Iterator end() { return Iterator(nullptr); }

private:
    long count_;
    CdTextItem *items_;

    int language_[8]; // mapping from block nr to language code
    Util::Encoding encoding_[8]; // mapping from block_nr to encoding

    void setDefaultLanguageMapping();

};

#endif
