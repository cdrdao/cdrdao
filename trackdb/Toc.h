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
 * $Log: Toc.h,v $
 * Revision 1.1  2000/02/05 01:32:26  llanero
 * Initial revision
 *
 * Revision 1.7  1999/04/05 11:03:01  mueller
 * Added CD-TEXT support.
 *
 * Revision 1.6  1999/04/02 20:36:21  mueller
 * Created implementation class that contains all mutual member data.
 *
 * Revision 1.5  1999/03/27 19:52:26  mueller
 * Added data track support.
 *
 * Revision 1.4  1999/01/10 15:10:13  mueller
 * Added functions 'appendTrack()' and 'appendAudioData()'.
 *
 * Revision 1.3  1998/11/15 12:19:13  mueller
 * Added several functions for manipulating track/index marks.
 *
 * Revision 1.2  1998/09/22 19:17:19  mueller
 * Added seeking to and reading of samples for GUI.
 *
 */

#ifndef __TOC_H__
#define __TOC_H__

#include <iostream.h>
#include "Track.h"
#include "CdTextContainer.h"
#include "CdTextItem.h"

class Toc {
public:
  Toc();
  ~Toc();

  enum TocType { CD_DA, CD_ROM, CD_I, CD_ROM_XA };

  // sets/returns toc type
  void tocType(TocType);
  TocType tocType() const;

  int nofTracks() const { return nofTracks_; }

  Msf length() const { return length_; }

  // returns mode that should be used for lead-in and gap
  TrackData::Mode leadInMode() const;
  // returns mode that should be used for lead-out
  TrackData::Mode leadOutMode() const;

  int append(const Track *);
  void insert(int, const Track *);
  void remove(int);

  Track *getTrack(int trackNr);

  int moveTrackMarker(int trackNr, int indexNr, long lba);
  int removeTrackMarker(int trackNr, int indexNr);
  int addIndexMarker(long lba);
  int addTrackMarker(long lba);
  int addPregap(long lba);

  void appendTrack(const TrackDataList *, long *start, long *end);
  int appendTrackData(const TrackDataList *, long *start, long *end);

  int removeTrackData(unsigned long start, unsigned long end,
		      TrackDataList **);
  int insertTrackData(unsigned long pos, const TrackDataList *list);

  int check() const;

  static Toc *read(const char *);
  int write(const char *) const;

  int catalogValid() const { return catalogValid_; }

  int catalog(const char *); // sets catalog number
  char catalog(int i) const { return catalog_[i]; } // BCD
  const char *catalog() const;
  
  void addCdTextItem(int trackNr, CdTextItem *);
  void removeCdTextItem(int trackNr, CdTextItem::PackType, int blockNr);
  int existCdTextBlock(int blockNr) const;
  const CdTextItem *getCdTextItem(int trackNr, int blockNr,
				  CdTextItem::PackType) const;
  void cdTextLanguage(int blockNr, int lang);
  int cdTextLanguage(int blockNr) const;
  int checkCdTextData() const;


  void print(ostream &) const;

  static const char *tocType2String(TocType);

private:
  friend class TocImpl;
  friend class TocParserGram;

  struct TrackEntry {
    TrackEntry() : absStart(0), start(0), end(0) {
      trackNr = 0; track = 0; next = 0; pred = 0;
    }

    int trackNr;
    Track *track;
    Msf absStart; // absoulte track start (end of last track)
    Msf start; // logical track start (after pre-gap)
    Msf end;
    
    struct TrackEntry *next;
    struct TrackEntry *pred;
  };

  TocType tocType_; // type of TOC

  int nofTracks_;
  TrackEntry *tracks_;
  TrackEntry *lastTrack_;

  Msf length_; // total length of disc

  char catalog_[13];
  int catalogValid_;

  CdTextContainer cdtext_;

  void update();

  TrackEntry *findTrack(unsigned long sample) const;

  TrackEntry *findTrackByNumber(int trackNr) const;

  void remove(TrackEntry *);

  void checkConsistency();

  friend class TocReader;
  friend class TrackIterator;
};


class TocReader {
public:
  TocReader(const Toc * = 0);
  ~TocReader();

  void init(const Toc *);

  int openData();
  long readData(long lba, char *buf, long len);
  int seekSample(unsigned long sample);
  long readSamples(Sample *buf, long len);
  void closeData();
  
private:
  const Toc *toc_;

  TrackReader reader;

  const Toc::TrackEntry *readTrack_; // actual read track
  long readPos_; // actual read position (blocks)
  long readPosSample_; // actual read position (samples)
  int open_; // != 0 indicates that toc was opened for reading data
};


class TrackIterator {
public:
  TrackIterator(const Toc *);
  ~TrackIterator();

  const Track *find(int trackNr, Msf &start, Msf &end);
  const Track *find(unsigned long sample, Msf &start, Msf &end,
		    int *trackNr);
  const Track *first(Msf &start, Msf &end);
  const Track *next(Msf &start, Msf &end);

private:
  const Toc *toc_;
  Toc::TrackEntry *iterator_;
};



inline
void Toc::tocType(TocType t)
{
  tocType_ = t;
}

inline
Toc::TocType Toc::tocType() const
{
  return tocType_;
}





#endif
