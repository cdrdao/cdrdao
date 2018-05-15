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

#ifndef __TOC_EDIT_H__
#define __TOC_EDIT_H__

#include <string>
#include <list>
#include <sigc++/signal.h>

#include "Toc.h"
#include "CdTextItem.h"
#include "FormatConverter.h"

class Toc;
class TrackData;
class TrackDataScrap;
class SampleManager;
class TocEditView;

class TocEdit {
public:
  TocEdit(Toc *, const char *);
  ~TocEdit();

  void toc(Toc *, const char *);
  Toc *toc() const;

  SampleManager *sampleManager();

  unsigned long lengthSample() const;

  void tocDirty(bool);
  bool tocDirty() const            { return tocDirty_; }

  void blockEdit();
  void unblockEdit();
  bool editable() const            { return (editBlocked_ == 0); }

  // returns and resets update level
  unsigned long updateLevel();

  void filename(const char *);
  const char *filename() const;

  int readToc(const char *);
  int saveToc();
  int saveAsToc(const char *);
  
  int moveTrackMarker(int trackNr, int indexNr, long lba);
  int addTrackMarker(long lba);
  int removeTrackMarker(int trackNr, int indexNr);
  int addIndexMarker(long lba);
  int addPregap(long lba);

  // Asynchronous interface.
  void queueConversion(const char* filename);
  void queueAppendTrack(const char* filename);
  void queueAppendFile(const char* filename);
  void queueInsertFile(const char* filename, unsigned long pos);
  void queueScan(long start, long end);

  // Abort all queued work.
  void queueAbort();

  // Is queue active
  bool isQueueActive();

  int appendSilence(unsigned long);
  int insertSilence(unsigned long length, unsigned long pos);

  int removeTrackData(TocEditView *);
  int insertTrackData(TocEditView *);
  
  void setTrackCopyFlag(int trackNr, int flag);
  void setTrackPreEmphasisFlag(int trackNr, int flag);
  void setTrackAudioType(int trackNr, int flag);
  void setTrackIsrcCode(int trackNr, const char *);

  void setCdTextItem(int trackNr, CdTextItem::PackType, int blockNr,
		     const char *);
  void setCdTextGenreItem(int blockNr, int code1, int code2,
			  const char *description);
  void setCdTextLanguage(int blockNr, int langCode);

  void setCatalogNumber(const char *);
  void setTocType(Toc::TocType);

  // Signals
  sigc::signal0<void> signalProgressPulse;
  sigc::signal1<void, double> signalProgressFraction;
  sigc::signal1<void, const char*> signalStatusMessage;
  sigc::signal0<void> signalFullView;
  sigc::signal2<void, unsigned long, unsigned long> signalSampleSelection;
  sigc::signal1<void, bool> signalCancelEnable;
  sigc::signal1<void, const char*> signalError;
  
private:
  Toc *toc_;
  SampleManager *sampleManager_;

  char *filename_;

  TrackDataScrap *trackDataScrap_;

  bool tocDirty_;
  int  editBlocked_;

  unsigned long updateLevel_;

  class QueueJob {
  public:
    QueueJob(const char* o) { op = o; }
    ~QueueJob() {}
    std::string op;
    std::string file;
    std::string cfile;
    long pos;
    long end;
    long len;
  };

  std::list<QueueJob*> queue_;
  QueueJob* cur_;
  bool threadActive_;
  enum { TE_IDLE, TE_CONVERTING, TE_CONVERTED, TE_READING } curState_;
  FormatSupport* curConv_;

  bool curScan();
  bool curAppendTrack();
  bool curAppendFile();
  bool curInsertFile();
  int  curCreateAudioData(TrackData **);
  void curSignalConversionError(FormatSupport::Status);
  void activateQueue();
  bool queueThread();

  friend class TocEditView;
};

#endif
