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

#ifndef __SUBTRACK_H__
#define __SUBTRACK_H__

#include "TrackData.h"

class SubTrack : public TrackData {
public:
  enum Type { PAD, DATA };

  SubTrack(Type, unsigned long start, const TrackData &);
  SubTrack(Type, const TrackData &);
  SubTrack(const SubTrack &);
  ~SubTrack();

  Type type() const { return type_; }
  unsigned long start() const { return start_; }
  void start(unsigned long s) { start_ = s; }

private:
  unsigned long start_; // start postion (samples) within containing track
  Type type_;

  class SubTrack *next_;
  class SubTrack *pred_;

  friend class Track;
  friend class TrackReader;
  friend class SubTrackIterator;
};


#endif
