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
 * $Log: TrackManager.h,v $
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
 * Revision 1.1  1998/11/20 18:56:46  mueller
 * Initial revision
 *
 */

#ifndef __TRACK_MANAGER_H
#define __TRACK_MANAGER_H

#include <glib.h>

class Toc;
class Track;

class TrackManager {
public:
  struct Entry {
    Entry(const Track *t, int tn, int in, gint x) {
      track = t; trackNr = tn; indexNr = in; xpos = x;
      extend = 0; drawn = 1; selected = 0;
    }
    const Track *track;
    int trackNr;
    int indexNr;
    gint xpos;
    unsigned int extend : 1;
    unsigned int drawn : 1;
    unsigned int selected : 1;
  };

  TrackManager(gint trackMarkerWidth);
  ~TrackManager();

  void update(const Toc *, unsigned long start, unsigned long end, gint width);

  // returns entry that is picked at given x-postion
  const Entry *pick(gint x, gint *stopXMin, gint *stopXMax); 

  // selects given entry, use 'NULL' to unselect all
  void select(const Entry *);

  // selected entry with specified track/index
  void select(int trackNr, int indexNr);

  // iterates entries
  const Entry *first();
  const Entry *next();


private:
  struct EntryList {
    Entry *ent;
    EntryList *next;
  };

  gint trackMarkerWidth_;
  gint width_;
  EntryList *entries_;
  EntryList *lastEntry_;
  EntryList *iterator_;

  void clear();
  void append(Entry *);
};

#endif
