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

#include <list>
#include <string>
#include <gtkmm.h>
#include <filesystem>

#include "CdTextItem.h"
#include "FormatConverter.h"
#include "TaskManager.h"
#include "trackdb/Toc.h"
#include "trackdb/Cddb.h"

class Toc;
class TrackData;
class TrackDataScrap;
class SampleManager;
class TocEditView;

class TocEdit : public Glib::Object
{
  public:
    TocEdit(Toc *, const std::string&);
    ~TocEdit();

    void toc(Toc *, const std::string&, bool nodata = false);
    Toc *toc() const;

    SampleManager *sampleManager();

    unsigned long lengthSample() const;

    void tocDirty(bool);
    bool tocDirty() const { return tocDirty_; }

    void blockEdit();
    void unblockEdit();
    bool editable() const { return (editBlocked_ == 0); }
    bool empty() const { return empty_; }

    // returns and resets update level
    unsigned long updateLevel();

    void filename(const std::string& f);
    const std::filesystem::path& filename() const { return filename_; }

    int readToc(const char *);
    int saveToc();
    int saveAsToc(const std::string&);

    int moveTrackMarker(int trackNr, int indexNr, long lba);
    int addTrackMarker(long lba);
    int removeTrackMarker(int trackNr, int indexNr);
    int addIndexMarker(long lba);
    int addPregap(long lba);

    // Asynchronous interface.
    void queueConversion(const char *filename);
    void queueAppendTrack(const char *filename);
    void queueAppendFile(const char *filename);
    void queueInsertFile(const char *filename, unsigned long pos);
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

    void setCdTextItem(int trackNr, CdTextItem::PackType, int blockNr, const char *);
    void setCdTextGenreItem(int blockNr, int code1, int code2, const char *description);
    void setCdTextLanguage(int blockNr, int langCode);

    void setCatalogNumber(const char *);
    void setTocType(Toc::Type);

    TaskManager& taskManager() { return tm_; }

    // Signals
    sigc::signal<void()> signalProgressPulse;
    sigc::signal<void(bool)> signalSpinner;
    sigc::signal<void(double)> signalProgressFraction;
    sigc::signal<void(const char *)> signalStatusMessage;
    sigc::signal<void()> signalFullView;
    sigc::signal<void(unsigned long, unsigned long)> signalSampleSelection;
    sigc::signal<void(bool)> signalCancelEnable;
    sigc::signal<void(const char *)> signalError;

  private:
    Toc *toc_ = nullptr;
    SampleManager *sampleManager_ = nullptr;
    TaskManager tm_;

    std::filesystem::path filename_;

    TrackDataScrap *trackDataScrap_ = nullptr;

    bool tocDirty_ = false;
    bool empty_ = true;
    bool nodata_ = false;
    int editBlocked_ = false;
    std::unique_ptr<Cddb> cddb_;
    unsigned long updateLevel_ = 0;

    class QueueJob : public Task
    {
      public:
        QueueJob(TocEdit *t, const char *o) : te(t), op(o) {}
        ~QueueJob() {}
        std::string op;
        std::string file;
        std::string cfile;
        long pos = 0;
        long end = 0;
        long len = 0;
        TocEdit *te;
        FormatSupport::Status conv_err;

        void run();
        void completed();
    };

    int outstanding;
    int completed;

    void showOutstanding();

    void taskQueueStarted();
    void taskQueueDone();

    void addJobToThreadQueue(QueueJob*);
    void threadQueueStatus(Task *, const std::string &);

    bool curScan(QueueJob *);
    bool curAppendTrack(QueueJob *);
    bool curAppendFile(QueueJob *);
    bool curInsertFile(QueueJob *);
    int curCreateAudioData(QueueJob *, TrackData **);
    void runCompletion(QueueJob *);

    friend class TocEditView;
};

#endif
