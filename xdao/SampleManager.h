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
 * $Log: SampleManager.h,v $
 * Revision 1.1  2000/02/05 01:38:51  llanero
 * Initial revision
 *
 * Revision 1.3  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.2  1999/01/30 15:11:13  mueller
 * First released version. Compiles with Gtk-- 0.9.14.
 *
 * Revision 1.1  1998/11/20 18:56:06  mueller
 * Initial revision
 *
 */

#ifndef __SAMPLE_MANAGER_H
#define __SAMPLE_MANAGER_H

class TocEdit;
class TrackDataScrap;

class SampleManager {
public:
  SampleManager(unsigned long blocking);
  ~SampleManager();

  void setTocEdit(TocEdit *);

  unsigned long blocking() const;

  int scanToc(unsigned long start, unsigned long end);

  void getPeak(unsigned long start, unsigned long end,
	       short *leftNeg, short *leftPos,
	       short *rightNeg, short *rightPos);

  void removeSamples(unsigned long start, unsigned long end, TrackDataScrap *);
  void insertSamples(unsigned long pos, unsigned long len,
		     const TrackDataScrap *);

private:
  class SampleManagerImpl *impl_;
};

#endif
