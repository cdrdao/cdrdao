/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001 Andreas Mueller <andreas@daneb.de>
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

#ifndef __TOC_H__
#define __TOC_H__

#include <iostream>
#include <list>
#include <string>

#include "Track.h"
#include "CdTextContainer.h"
#include "CdTextItem.h"

class Toc {
public:
  Toc();
  Toc(const Toc &);
  ~Toc();

  enum TocType { CD_DA, CD_ROM, CD_I, CD_ROM_XA };

  // sets/returns toc type
  void tocType(TocType);
  TocType tocType() const;

  void firstTrackNo(int i) { firstTrackNo_ = i; }
  int firstTrackNo() const { return firstTrackNo_; }

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

  static Toc *read(const char *);
  int write(const char *) const;
  bool write(int fd, bool conversions = false) const;

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

  void trackSummary(int *nofAudioTracks, int *nofMode1Tracks,
		    int *nofMode2Tracks) const;

  // Verification methods
  bool resolveFilenames(const char* tocFilename);
  int check() const;
  int checkCdTextData() const;

  bool convertFilesToWav();
  bool recomputeLength();

  // if conversions is true, the TOc is printed with the converted WAV
  // or RAW filenames instead of the original ones.
  void print(std::ostream &, bool conversions = false) const;

  void collectFiles(std::set<std::string>& list);
  void markFileConversion(const char* src, const char* dst);

  static const char *tocType2String(TocType);

private:
  friend class TocImpl;
  friend class TocParserGram;
  friend class TocReader;
  friend class TrackIterator;

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

  void update();

  TrackEntry *findTrack(unsigned long sample) const;

  TrackEntry *findTrackByNumber(int trackNr) const;

  void remove(TrackEntry *);

  void fixLengths();

  void checkConsistency();


  TocType tocType_; // type of TOC

  int nofTracks_, firstTrackNo_;
  TrackEntry *tracks_;
  TrackEntry *lastTrack_;

  Msf length_; // total length of disc

  char catalog_[13];
  int catalogValid_;

  CdTextContainer cdtext_;
};


class TocReader {
public:
  TocReader(const Toc * = 0);
  ~TocReader();

  void init(const Toc *);

  int openData();
  //long readData(long lba, char *buf, long len);
  int seekSample(unsigned long sample);
  long readSamples(Sample *buf, long len);
  void closeData();

  const char* curFilename();
  
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
  const Track *first();
  const Track *next(Msf &start, Msf &end);
  const Track *next();

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
