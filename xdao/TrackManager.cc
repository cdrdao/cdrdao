/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: TrackManager.cc,v $
 * Revision 1.2  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.1.1.1.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.1.1.1  2000/02/05 01:40:19  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.2  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.1  1998/11/20 18:56:46  mueller
 * Initial revision
 *
 */

#include <stdio.h>
#include <assert.h>

#include "TrackManager.h"

#include "Toc.h"
#include "util.h"

TrackManager::TrackManager(gint trackMarkerWidth)
{
  trackMarkerWidth_ = trackMarkerWidth;
  width_ = 0;
  entries_ = NULL;
  lastEntry_ = NULL;
  iterator_ = NULL;
}

TrackManager::~TrackManager()
{
  clear();
}

void TrackManager::clear()
{
  EntryList *next;

  while (entries_ != NULL) {
    next = entries_->next;
    delete entries_->ent;
    delete entries_;
    entries_ = next;
  }

  lastEntry_ = NULL;
  iterator_ = NULL;
}

void TrackManager::append(Entry *ent)
{
  EntryList *entry = new EntryList;
  entry->ent = ent;
  entry->next = NULL;

  if (entries_ == NULL)
    entries_ = entry;
  else
    lastEntry_->next = entry;

  lastEntry_ = entry;
}


const TrackManager::Entry *TrackManager::first()
{
  if ((iterator_ = entries_) != NULL)
    return iterator_->ent;
  else
    return NULL;
}

const TrackManager::Entry *TrackManager::next()
{
  if (iterator_ == NULL ||
      (iterator_ = iterator_->next) == NULL) 
    return NULL;
  else
    return iterator_->ent;
}


void TrackManager::select(const Entry *ent)
{
  EntryList *run;

  for (run = entries_; run != NULL; run = run->next) {
    if (run->ent == ent)
      run->ent->selected = 1;
    else
      run->ent->selected = 0;
  }
}

void TrackManager::select(int trackNr, int indexNr)
{
  EntryList *run;

  for (run = entries_; run != NULL; run = run->next) {
    if (run->ent->trackNr == trackNr && run->ent->indexNr == indexNr)
      run->ent->selected = 1;
    else
      run->ent->selected = 0;
  }
}

const TrackManager::Entry *TrackManager::pick(gint x, gint *stopXMin,
					      gint *stopXMax)
{
  EntryList *run;
  Entry *pred;

  for (run = entries_, pred = NULL; run != NULL; 
       pred = run->ent, run = run->next) {
    if ((run->ent->indexNr == 1 || run->ent->selected)&& 
	run->ent->extend == 0 &&
	run->ent->drawn &&
	x >= run->ent->xpos && x < run->ent->xpos + trackMarkerWidth_) {

      if (stopXMin != NULL)
	*stopXMin = pred != NULL ? pred->xpos + 1 : 0;

      if (stopXMax != NULL)
	*stopXMax = run->next != NULL ? run->next->ent->xpos - 1 : width_ - 1;
	  
      return run->ent;
    }
  }

  for (run = entries_, pred = NULL; run != NULL;
       pred = run->ent, run = run->next) {
    if (run->ent->indexNr != 1 && run->ent->extend == 0 &&
	run->ent->drawn &&
	x >= run->ent->xpos && x < run->ent->xpos + trackMarkerWidth_) {
      
      if (stopXMin != NULL)
	*stopXMin = pred != NULL ? pred->xpos : 0;

      if (stopXMax != NULL)
	*stopXMax = run->next != NULL ? run->next->ent->xpos : width_;

      return run->ent;
    }
  }

  return NULL;
}


void TrackManager::update(const Toc *toc, unsigned long start,
			  unsigned long end, gint width)
{
  Msf tstart, tend;
  long startLba = start / SAMPLES_PER_BLOCK;
  long startRest = start % SAMPLES_PER_BLOCK;
  long endLba = end / SAMPLES_PER_BLOCK;
  int trackNr;
  const Track *t;
  Entry *ent;
  int nindex, index;
  long markStart = -1;
  int last = 0;

  clear();

  width_ = width;

  TrackIterator itr(toc);

  if (toc == NULL || start == end ||
      (t = itr.find(start, tstart, tend, &trackNr)) == NULL)
    return;

  double samp2pix = double(width - 1) / double(end - start);
#define lba2pixel(lba) \
gint(double(((lba) * SAMPLES_PER_BLOCK) - start) * samp2pix + 0.5)


  nindex = t->nofIndices();

  if (startLba < tstart.lba()) {
    // 'start' is in pre-gap of track
    ent = new Entry(t, trackNr, 0, 0);

    markStart = tstart.lba() - t->start().lba();
    index = 0;
  }
  else {
    if (nindex == 0 || startLba <= tstart.lba() + t->getIndex(0).lba()) {
      markStart = tstart.lba();
      index = 1;
    }
    else {
      for (index = 3; index - 2 < nindex; index++) {
	if (startLba >= tstart.lba() + t->getIndex(index - 3).lba() &&
	    startLba < tstart.lba() + t->getIndex(index - 2).lba()) {
	  markStart = tstart.lba() + t->getIndex(index - 3).lba();
	  index--;
	  break;
	}
      }
      if (markStart == -1) {
	markStart = tstart.lba() + t->getIndex(nindex - 1).lba();
	index = nindex + 1;
      }
    }
  }

  ent = new Entry(t, trackNr, index, 0);
  if (startLba != markStart || startRest > 0) 
    ent->extend = 1;
  append(ent);

  if (index == 0 && tstart.lba() <= endLba)
    append(new Entry(t, trackNr, 1, lba2pixel(tstart.lba())));

  if (index == 0) 
    index = 1;

  index++;

  for (; index - 2 < nindex; index++) {
    if (tstart.lba() + t->getIndex(index - 2).lba() > endLba) 
      break;

    append(new Entry(t, trackNr, index,
		     lba2pixel(tstart.lba() + t->getIndex(index - 2).lba())));
  }

  while ((t = itr.next(tstart, tend)) != NULL) {
    trackNr++;

    if (t->start().lba() != 0) {
      if (tstart.lba() - t->start().lba() > endLba)
	break;

      append(new Entry(t, trackNr, 0,
		       lba2pixel(tstart.lba() - t->start().lba())));
    }

    if (tstart.lba() > endLba)
      break;

    append(new Entry(t, trackNr, 1, lba2pixel(tstart.lba())));

    last = 0;

    nindex = t->nofIndices();

    for (index = 0; index < nindex; index++) {
      if (tstart.lba() + t->getIndex(index).lba() > endLba) {
	last = 1;
	break;
      }
      append(new Entry(t, trackNr, index + 2,
		       lba2pixel(tstart.lba() + t->getIndex(index).lba())));
    }

    if (last)
      break;
  }
}
