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
 * $Log: TrackDataScrap.cc,v $
 * Revision 1.1.1.1  2000/02/05 01:40:17  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/27 18:40:01  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: TrackDataScrap.cc,v 1.1.1.1 2000/02/05 01:40:17 llanero Exp $";

#include "TrackDataScrap.h"

#include <stddef.h>

#include "util.h"
#include "TrackData.h"
#include "TrackDataList.h"

TrackDataScrap::TrackDataScrap(TrackDataList *l)
{
  list_ = l;
  blocks_ = 0;

  leftNegSamples_ = NULL;
  leftPosSamples_ = NULL;
  rightNegSamples_ = NULL;
  rightPosSamples_ = NULL;
}

TrackDataScrap::~TrackDataScrap()
{
  delete list_;
  list_ = NULL;

  delete[] leftNegSamples_;
  leftNegSamples_ = NULL;

  delete[] leftPosSamples_;
  leftPosSamples_ = NULL;

  delete[] rightNegSamples_;
  rightNegSamples_ = NULL;

  delete[] rightPosSamples_;
  rightPosSamples_ = NULL;
}

const TrackDataList *TrackDataScrap::trackDataList() const
{
  return (const TrackDataList*)list_;
}

void TrackDataScrap::setPeaks(long blocks,
			      short *leftNegSamples, short *leftPosSamples,
			      short *rightNegSamples, short *rightPosSamples)
{
  long i;

  blocks_ = blocks;

  delete[] leftNegSamples_;
  delete[] leftPosSamples_;
  delete[] rightNegSamples_;
  delete[] rightPosSamples_;

  if (blocks > 0) {
    leftNegSamples_ = new short[blocks];
    leftPosSamples_ = new short[blocks];
    rightNegSamples_ = new short[blocks];
    rightPosSamples_ = new short[blocks];

    for (i = 0; i < blocks; i++) {
      leftNegSamples_[i] = leftNegSamples[i];
      leftPosSamples_[i] = leftPosSamples[i];
      rightNegSamples_[i] = rightNegSamples[i];
      rightPosSamples_[i] = rightPosSamples[i];
    }
  }
  else {
    leftNegSamples_ = NULL;
    leftPosSamples_ = NULL;
    rightNegSamples_ = NULL;
    rightPosSamples_ = NULL;
  }    
}

void TrackDataScrap::getPeaks(long blocks, short *leftNegSamples,
			      short *leftPosSamples, short *rightNegSamples,
			      short *rightPosSamples) const
{
  long n, i;

  if (leftNegSamples_ == NULL)
    return;

  n = (blocks_ < blocks) ? blocks_ : blocks;
  
  for (i = 0; i < n; i++) {
    leftNegSamples[i] = leftNegSamples_[i];
    leftPosSamples[i] = leftPosSamples_[i];
    rightNegSamples[i] = rightNegSamples_[i];
    rightPosSamples[i] = rightPosSamples_[i];
  }
}
