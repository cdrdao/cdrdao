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
 * $Log: CdTextContainer.cc,v $
 * Revision 1.1.1.1  2000/02/05 01:32:35  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/04/05 11:02:11  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: CdTextContainer.cc,v 1.1.1.1 2000/02/05 01:32:35 llanero Exp $";

#include <iostream.h>
#include <assert.h>

#include "CdTextContainer.h"
#include "CdTextItem.h"

CdTextContainer::CdTextContainer()
{
  count_ = 0;
  items_ = 0;

  setDefaultLanguageMapping();
}

CdTextContainer::CdTextContainer(const CdTextContainer &obj)
{
  CdTextItem *run;
  CdTextItem *last = NULL;
  int i;

  items_ = NULL;
  count_ = 0;

  for (run = obj.items_; run != NULL; run = run->next_) {
    if (last == NULL) {
      items_ = new CdTextItem(*run);
      last = items_;
    }
    else {
      last->next_ = new CdTextItem(*run);;
      last = last->next_;
    }
    count_++;
  }

  // copy language map
  for (i = 0; i < 8; i++)
    language_[i] = obj.language_[i];
}

CdTextContainer::~CdTextContainer()
{
  CdTextItem *next;

  while (items_ != NULL) {
    next = items_->next_;
    delete items_;
    items_ = next;
  }

  count_ = 0;
}

void CdTextContainer::setDefaultLanguageMapping()
{
  int i;

  // set all to undefined
  for (i = 0; i < 8; i++)
    language_[i] = -1;
}

void CdTextContainer::print(int isTrack, ostream &out) const
{
  CdTextItem *run;
  int i;
  int foundLanguageMapping;

  if (count_ > 0) {
    int actBlockNr = items_->blockNr();

    out << "CD_TEXT {\n";

    if (!isTrack) {
      foundLanguageMapping = 0;
      for (i = 0; i < 8; i++) {
	if (language_[i] >= 0) {
	  foundLanguageMapping = 1;
	  break;
	}
      }

      if (foundLanguageMapping) {
	out << "  LANGUAGE_MAP {\n";

	for (i = 0; i < 8; i++) {
	  if (language_[i] >= 0) 
	    out << "    " << i << ": " << language_[i] << "\n";
	}
      
	out << "  }\n";
      }
    }

    out << "  LANGUAGE " << actBlockNr << " {\n";

    
    for (run = items_; run != NULL; run = run->next_) {
      if (run->blockNr() != actBlockNr) {
	actBlockNr = run->blockNr();
	out << "  }\n  LANGUAGE " << actBlockNr << " {\n";
      }
      out << "    ";
      run->print(isTrack, out);
      out << "\n";
    }

    out << "  }\n}\n";
  }
}

void CdTextContainer::add(CdTextItem *item)
{
  CdTextItem *pred;
  CdTextItem *run;

  assert(item->next_ == NULL);

  remove(item->packType(), item->blockNr());

  for (pred = NULL, run = items_; run != NULL; pred = run, run = run->next_) {
    if (item->blockNr() < run->blockNr() ||
	(item->blockNr() == run->blockNr() &&
	 item->packType() < run->packType()))
      break;
  }

  if (pred == NULL) {
    item->next_ = items_;
    items_ = item;
  }
  else {
    item->next_ = pred->next_;
    pred->next_ = item;
  }

  count_++;
}

void CdTextContainer::remove(CdTextItem::PackType type, int blockNr)
{
  CdTextItem *run, *pred;

  for (pred = NULL, run = items_; run != NULL; pred = run, run = run->next_) {
    if (run->packType() == type && run->blockNr() == blockNr) {
      if (pred == NULL)
	items_ = run->next_;
      else
	pred->next_ = run->next_;

      count_--;

      delete run;
      return;
    }
  }
}

int CdTextContainer::existBlock(int blockNr) const
{
  CdTextItem *run;

  for (run = items_; run != NULL; run = run->next_) {
    if (run->blockNr() == blockNr) {
      return 1;
    }
  }

  return 0;
}

const CdTextItem *CdTextContainer::getPack(int blockNr,
					   CdTextItem::PackType type) const
{
  CdTextItem *run;

  for (run = items_; run != NULL; run = run->next_) {
    if (run->blockNr() == blockNr && run->packType() == type) {
      return run;
    }
  }

  return NULL;
}

void CdTextContainer::language(int blockNr, int lang)
{
  assert(blockNr >= 0 && blockNr <= 7);
  assert(lang >= -1 && lang <= 255);

  language_[blockNr] = lang;
}


int CdTextContainer::language(int blockNr) const
{
  assert(blockNr >= 0 && blockNr <= 7);

  return language_[blockNr];
}
