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
 * $Log: Track.h,v $
 * Revision 1.1  2000/02/05 01:32:33  llanero
 * Initial revision
 *
 * Revision 1.6  1999/04/05 11:03:01  mueller
 * Added CD-TEXT support.
 *
 * Revision 1.5  1999/04/02 20:36:21  mueller
 * Created implementation class that contains all mutual member data.
 *
 * Revision 1.4  1999/03/27 19:52:26  mueller
 * Added data track support.
 *
 * Revision 1.3  1998/11/15 12:19:13  mueller
 * Added several functions for manipulating track/index marks.
 *
 * Revision 1.2  1998/09/22 19:17:19  mueller
 * Added seeking to and reading of samples for GUI.
 *
 */

#ifndef __TRACK_H__
#define __TRACK_H__

#include <iostream.h>

#include "SubTrack.h"
#include "Msf.h"
#include "CdTextContainer.h"
#include "CdTextItem.h"

class TrackDataList;

class Track {
public:
  Track(TrackData::Mode);
  Track(const Track &);
  ~Track();
  
  TrackData::Mode type() const { return type_; }
  Msf length() const { return length_; }

  Msf start() const { return start_; }
  int start(Msf);

  Msf end() const { return end_; }
  int end(Msf);

  int isPadded() const;

  int nofSubTracks() const { return nofSubTracks_; }

  // return first/last sub-track
  const SubTrack *firstSubTrack() const;
  const SubTrack *lastSubTrack() const;

  int append(const SubTrack &);

  int nofIndices() const { return nofIndices_; }

  int appendIndex(const Msf &);
  int addIndex(const Msf &index);
  int removeIndex(int);
  Msf getIndex(int) const;

  int moveIndex(int index, long lba);
  TrackDataList *removeToEnd(unsigned long samplePos);
  TrackDataList *removeFromStart(unsigned long sample);

  void prependTrackData(const TrackDataList *);
  void appendTrackData(const TrackDataList *);
  void appendTrackData(const Track *);

  TrackDataList *removeTrackData(unsigned long start, unsigned long end);
  void insertTrackData(unsigned long pos, const TrackDataList *list);

  // fills provided buffer with an audio block that contains zero data
  // encoded with given mode
  static void encodeZeroData(int encMode, TrackData::Mode, long lba, char *);

  int check() const;

  int isrcValid() const { return isrcValid_; }

  // set ISRC code from given string
  int isrc(const char *); // sets ISRC code
  const char *isrc() const;

  char isrcCountry(int i) const { return isrcCountry_[i]; } // ASCII
  char isrcOwner(int i) const { return isrcOwner_[i]; }     // ASCII
  char isrcYear(int i) const { return isrcYear_[i]; }       // BCD
  char isrcSerial(int i) const { return isrcSerial_[i]; }   // BCD

  // return/set COPY flag (1: copy permitted, 0: copy not permitted)
  int copyPermitted() const { return flags_.copy; }
  void copyPermitted(int c) { flags_.copy = c != 0 ? 1 : 0; }

  // return/set PRE-EMPHASIS flag (1: audio with pre-emphasis,
  // 0: audio without pre-emphasis
  int preEmphasis() const { return flags_.preEmphasis; }
  void preEmphasis(int p) { flags_.preEmphasis = p != 0 ? 1 : 0; }

  // return/set audio type (0: two channel audio, 1: four channel audio)
  int audioType() const { return flags_.audioType; }
  void audioType(int t) { flags_.audioType = t != 0 ? 1 : 0; }

  void addCdTextItem(CdTextItem *);
  void removeCdTextItem(CdTextItem::PackType, int blockNr);
  int existCdTextBlock(int n) const { return cdtext_.existBlock(n); }
  const CdTextItem *getCdTextItem(int blockNr, CdTextItem::PackType t) const {
    return cdtext_.getPack(blockNr, t);
  }

  void print(ostream &) const;

private:
  friend class TocParserGram;
  friend class Toc;
  friend class TrackReader;

  TrackData::Mode type_; // track type

  Msf length_; // total length of track
  Msf start_;  // logical start of track data, end of pre-gap
               // (where index switches from 0 to 1)
  Msf end_;    // end of track data, start of post-gap
  
  int nofSubTracks_;    // number of sub tracks
  SubTrack *subTracks_; // list of subtracks
  SubTrack *lastSubTrack_; // points to last sub-track in list

  int nofIndices_;      // number of index increments
  Msf *index_;          // index increment times


  int  isrcValid_;
  char isrcCountry_[2];
  char isrcOwner_[3];
  char isrcYear_[2];
  char isrcSerial_[5];
  
  struct {
    unsigned int copy : 1;
    unsigned int preEmphasis : 1;
    unsigned int audioType : 1;
  } flags_;

  CdTextContainer cdtext_;

  void update();

  void insertSubTrackAfter(SubTrack *, SubTrack *newSubtrack);
  SubTrack *removeSubTrack(SubTrack *);

  void countSubTracks();
  void mergeSubTracks();

  SubTrack *findSubTrack(unsigned long sample) const;
  void checkConsistency();
};

class TrackReader {
public:
  TrackReader(const Track * = 0);
  ~TrackReader();

  void init(const Track *);

  int openData();
  long readData(int raw, long lba, char *buf, long len);
  int seekSample(unsigned long sample);
  long readSamples(Sample *buf, long len);
  void closeData();


private:
  const Track *track_;

  TrackDataReader reader;

  long readPos_; // current read position (blocks)
  long readPosSample_; // current read position (samples)
  const SubTrack *readSubTrack_; // actual read sub-track
  int open_; // 1 indicates the 'openData()' was called


  long readTrackData(Sample *buf, long len);
  int readBlock(int raw, long lba, Sample *buf);
};

#endif
