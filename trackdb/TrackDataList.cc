/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999 Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: TrackDataList.cc,v $
 * Revision 1.1.1.1  2000/02/05 01:34:31  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.2  1999/03/27 19:50:44  mueller
 * Renamed class 'AudioData' to 'TrackData'.
 *
 * Revision 1.1  1998/11/15 12:17:06  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: TrackDataList.cc,v 1.1.1.1 2000/02/05 01:34:31 llanero Exp $";

#include <config.h>


#include "TrackDataList.h"
#include "TrackData.h"

TrackDataList::TrackDataList()
{
  list_ = last_ = NULL;
  iterator_ = NULL;
  count_ = 0;
}

 
void TrackDataList::append(TrackData *a)
{
  Entry *ent = new Entry;

  ent->data = a;
  ent->next = NULL;

  if (list_ == NULL) 
    list_ = ent;
  else
    last_->next = ent;

  last_ = ent;

  count_++;
}

unsigned long TrackDataList::length() const
{
  unsigned long len = 0;
  Entry *run;

  for (run = list_; run != NULL; run = run->next) 
    len += run->data->length();

  return len;
}

void TrackDataList::clear()
{
  Entry *next;

  while (list_ != NULL) {
    next = list_->next;

    delete list_->data;
    delete list_;

    list_ = next;
  }

  last_ = NULL;
  iterator_ = NULL;
  count_ = 0;
}

const TrackData *TrackDataList::first() const
{
  if ((((TrackDataList *)this)->iterator_ = list_) != NULL)
    return iterator_->data;
  else
    return NULL;
}

const TrackData *TrackDataList::next() const
{
  if (iterator_ == NULL ||
      (((TrackDataList *)this)->iterator_ = iterator_->next) == NULL)
    return NULL;
  else
    return iterator_->data;
}
