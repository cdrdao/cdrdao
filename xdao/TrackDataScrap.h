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
 * $Log: TrackDataScrap.h,v $
 * Revision 1.2  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.1.1.1.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.1.1.1  2000/02/05 01:38:55  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/27 18:39:38  mueller
 * Initial revision
 *
 */

#ifndef __TRACK_DATA_SCRAP_H__
#define __TRACK_DATA_SCRAP_H__

class TrackDataList;

class TrackDataScrap {
public:
  TrackDataScrap(TrackDataList *);
  ~TrackDataScrap();

  const TrackDataList *trackDataList() const;

  void setPeaks(long blocks,
		short *leftNegSamples, short *leftPosSamples,
		short *rightNegSamples, short *rightPosSamples);
  void getPeaks(long blocks,
		short *leftNegSamples, short *leftPosSamples,
		short *rightNegSamples, short *rightPosSamples) const;

private:
  TrackDataList *list_;
  long blocks_;

  short *leftNegSamples_;
  short *leftPosSamples_;
  short *rightNegSamples_;
  short *rightPosSamples_;
};

#endif
