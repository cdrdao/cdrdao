/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: CdTextContainer.h,v $
 * Revision 1.1  2000/02/05 01:32:23  llanero
 * Initial revision
 *
 * Revision 1.1  1999/04/05 11:02:11  mueller
 * Initial revision
 *
 */

#ifndef __CDTEXTCONTAINER_H__
#define __CDTEXTCONTAINER_H__

#include "CdTextItem.h"

class ostream;

class CdTextContainer {
public:
  CdTextContainer();
  CdTextContainer(const CdTextContainer &);

  ~CdTextContainer();

  long count() const { return count_; }

  void add(CdTextItem *);

  void remove(CdTextItem::PackType, int blockNr);

  void print(int isTrack, ostream &) const;

  // checks if a pack exists for given 'blockNr' (language)
  int existBlock(int blockNr) const;

  // return pack for given 'blockNr' and pack type
  const CdTextItem *getPack(int blockNr, CdTextItem::PackType) const;

  // sets/returns language code for block nr
  void language(int blockNr, int lang);
  int language(int blockNr) const;
  
private:
  long count_;
  CdTextItem *items_;

  int language_[8]; // mapping from block nr to language code

  void setDefaultLanguageMapping();

};

#endif
