/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002 Andreas Mueller <andreas@daneb.de>
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

#include "TocEdit.h"

#include <stddef.h>
#include <gnome.h>
#include <iostream>
#include <sstream>
#include <set>

#include "util.h"
#include "Toc.h"
#include "TocEditView.h"
#include "TrackData.h"
#include "TrackDataList.h"
#include "TrackDataScrap.h"

#include "guiUpdate.h"
#include "SampleManager.h"

TocEdit::TocEdit(Toc *t, const char *filename)
{
  toc_ = NULL;
  sampleManager_ = NULL;
  filename_ = NULL;
  trackDataScrap_ = NULL;
  threadActive_ = false;
  curState_ = TE_IDLE;
  curConv_ = NULL;
  cur_ = NULL;

  updateLevel_ = 0;
  editBlocked_ = false;

  if (filename == NULL)
    toc(t, "unnamed.toc");
  else
    toc(t, filename);
}

TocEdit::~TocEdit()
{
  if (toc_)
    delete toc_;

  if (sampleManager_)
    delete sampleManager_;

  if (filename_)
    delete[] filename_;

  if (trackDataScrap_)
    delete trackDataScrap_;
}

void TocEdit::toc(Toc *t, const char *filename)
{
  if (toc_)
    delete toc_;

  if (t == NULL)
    toc_ = new Toc;
  else
    toc_ = t;

  if (filename != NULL) {
    delete[] filename_;
    filename_ = strdupCC(filename);
  }

  tocDirty_ = false;
  editBlocked_ = false;

  if (sampleManager_)
    delete sampleManager_;
  sampleManager_ = new SampleManager(588);

  sampleManager_->setTocEdit(this);

  if (toc_->length().samples() > 0) {

    // First collect all filenames and queue their conversions to WAV
    std::set<std::string> set;
    toc_->collectFiles(set);
    std::set<std::string>::iterator i = set.begin();
    for (; i != set.end(); i++)
      queueConversion((*i).c_str());

    // Second, queue for toc scan.
    unsigned long maxSample = toc_->length().samples() - 1;
    queueScan(0, -1);
  }

  updateLevel_ = UPD_ALL;
}

Toc *TocEdit::toc() const
{
  return toc_;
}

SampleManager *TocEdit::sampleManager()
{
  return sampleManager_;
}

void TocEdit::tocDirty(bool f)
{
  bool old = tocDirty_;

  tocDirty_ = f;

  if (old != tocDirty_)
    updateLevel_ |= UPD_TOC_DIRTY;
}

void TocEdit::blockEdit()
{
  if (editBlocked_ == 0)
    updateLevel_ |= UPD_EDITABLE_STATE;

  editBlocked_ += 1;
}

void TocEdit::unblockEdit()
{
  if (editBlocked_ > 0) {
    editBlocked_ -= 1;

    if (editBlocked_ == 0)
      updateLevel_ |= UPD_EDITABLE_STATE;
  }
}

unsigned long TocEdit::updateLevel()
{
  unsigned long level = updateLevel_;

  updateLevel_ = 0;
  return level;
}

unsigned long TocEdit::lengthSample() const
{
  return toc_->length().samples();
}

void TocEdit::filename(const char *fname)
{
  if (fname != NULL && *fname != 0) {
    char *s = strdupCC(fname);
    delete[] filename_;
    filename_ = s;

    updateLevel_ |= UPD_TOC_DATA;
  }
}

const char *TocEdit::filename() const
{
  return filename_;
}


int TocEdit::readToc(const char *fname)
{
  if (!editable())
    return 0;

  if (fname == NULL || *fname == 0)
    return 1;

  Toc *t = Toc::read(fname);

  if (t != NULL) {

    // Check and resolve input files paths
    t->resolveFilenames(fname);

    // Sometimes length fields are ommited. Make sure we got everything.
    t->recomputeLength();

    toc(t, fname);
    return 0;
  }

  return 1;
}

int TocEdit::saveToc()
{
  int ret = toc_->write(filename_);

  if (ret == 0)
    tocDirty(0);

  return ret;
}

int TocEdit::saveAsToc(const char *fname)
{
  int ret;

  if (fname != NULL && *fname != 0) {
    ret = toc_->write(fname);

    if (ret == 0) {
      filename(fname);
      tocDirty(0);
      updateLevel_ |= UPD_TOC_DATA;
    }

    return ret;
  }

  return 1;
}


int TocEdit::moveTrackMarker(int trackNr, int indexNr, long lba)
{
  if (!editable())
    return 0;

  int ret = toc_->moveTrackMarker(trackNr, indexNr, lba);

  if (ret == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::addTrackMarker(long lba)
{
  if (!editable())
    return 0;

  int ret = toc_->addTrackMarker(lba);

  if (ret == 0) {
    tocDirty(1);
//llanero: different views
//    updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::addIndexMarker(long lba)
{
  if (!editable())
    return 0;

  int ret = toc_->addIndexMarker(lba);

  if (ret == 0) {
    tocDirty(1);
//llanero: different views
//    updateLevel_ |= UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::addPregap(long lba)
{
  if (!editable())
    return 0;

  int ret = toc_->addPregap(lba);

  if (ret == 0) {
    tocDirty(1);
//llanero: different views
//    updateLevel_ |= UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::removeTrackMarker(int trackNr, int indexNr)
{
  if (!editable())
    return 0;

  int ret = toc_->removeTrackMarker(trackNr, indexNr);

  if (ret == 0) {
    tocDirty(1);
//llanero: different views
//    updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
  }

  return ret;
}

bool TocEdit::curScan()
{
  // An end position of -1 means recompute the toc length and scan to
  // the last sample position.

  if (cur_->end == -1) {
    // (Denis Leroy) The reason for this code is somewhat
    // complex. When importing a CUE file with MP3s, the length of the
    // last track is not known until the MP3 is actually converted to
    // a WAV (because, unlike TOC files, CUE files don't specify
    // explicitely the length of each track). Unlike WAV, you can't
    // guess the length of the track based on the mp3 file size
    // without scanning the whole thing, which we don't want to do
    // twice obviously. So the semantic of the "scan" job is changed
    // to integrate a length recalculation when the end is not
    // specified. It would be cleaner to create a specific job task to
    // do this.
    toc_->recomputeLength();
    cur_->end = toc_->length().samples() - 1;
    updateLevel_ |= UPD_SAMPLES;
  }
  int ret = sampleManager_->scanToc(cur_->pos, cur_->end);

  if (ret == 0)
    return true;

  if (ret == 2) {
    signalError("Unable to open or read from input audio files");
    return false;
  }

  return false;
}

bool TocEdit::curAppendTrack()
{
  TrackData* data;
  int ret = curCreateAudioData(&data);
  if (ret != 0)
    return false;

  TrackDataList list;
  long start, end;
  list.append(data);
  toc_->appendTrack(&list, &start, &end);
  tocDirty(1);
  sampleManager_->scanToc(Msf(start).samples(), Msf(end).samples() - 1);
  return true;
}

bool TocEdit::curAppendFile()
{
  TrackData* data;
  int ret = curCreateAudioData(&data);
  if (ret != 0)
    return false;

  TrackDataList list;
  long start, end;
  list.append(data);
  if (toc_->appendTrackData(&list, &start, &end) != 0) {
    delete data;
    return false;
  }
  tocDirty(1);
  sampleManager_->scanToc(Msf(start).samples(), Msf(end).samples() - 1);
  return true;
}

bool TocEdit::curInsertFile()
{
  TrackData* data;
  int ret = curCreateAudioData(&data);
  if (ret != 0)
    return false;

  TrackDataList list;
  list.append(data);
  if (toc_->insertTrackData(cur_->pos, &list) != 0) {
    signalError(_("Cannot insert file into a data track"));
    delete data;
    return false;
  }
  cur_->len = list.length();
  sampleManager_->insertSamples(cur_->pos, cur_->len, NULL);
  sampleManager_->scanToc(cur_->pos, cur_->pos + cur_->len);
  tocDirty(1);
  return true;
}

// Creates an audio data object for given filename. Errors are send to
// status line.
// data: filled with newly allocated TrackData object on success
// return: 0: OK
//         1: cannot open file
//         2: file has wrong format
int TocEdit::curCreateAudioData(TrackData **data)
{
  unsigned long len;
  std::string msg;

  switch (TrackData::checkAudioFile(cur_->cfile.c_str(), &len)) {
  case 1:
    msg = _("Could not open file \"");
    msg += cur_->cfile;
    msg += "\"";
    signalError(msg.c_str());
    return 1; // Cannot open file

  case 2:
    msg = _("Could not open file \"");
    msg += cur_->cfile;
    msg += "\" : wrong file format";
    signalError(msg.c_str());
    return 2; // File format error
  }

  *data = new TrackData(cur_->file.c_str(), 0, len);
  (*data)->effectiveFilename(cur_->cfile.c_str());

  return 0;
}

void TocEdit::curSignalConversionError(FormatSupport::Status err)
{
  std::string msg = _("Unable to decode audio file \"");
  msg += cur_->file;
  msg += "\" : ";
  switch (err) {
  case FormatSupport::FS_DISK_FULL:
    msg += _("disk is full");
    break;
  case FormatSupport::FS_OUTPUT_PROBLEM:
    msg += _("error creating output file");
    break;
  default:
    msg += _("read error or wrong file format");
  }
  signalError(msg.c_str());
}

void TocEdit::queueConversion(const char* filename)
{
  QueueJob* job = new QueueJob("convert");
  job->file = filename;
  queue_.push_back(job);

  if (!threadActive_)
    activateQueue();
}

void TocEdit::queueAppendTrack(const char* filename)
{
  QueueJob* job = new QueueJob("aptrack");
  job->op = "aptrack";
  job->file = filename;
  queue_.push_back(job);

  if (!threadActive_)
    activateQueue();
}

void TocEdit::queueAppendFile(const char* filename)
{
  QueueJob* job = new QueueJob("apfile");
  job->file = filename;
  queue_.push_back(job);

  if (!threadActive_)
    activateQueue();
}

void TocEdit::queueInsertFile(const char* filename, unsigned long pos)
{
  QueueJob* job = new QueueJob("infile");
  job->file = filename;
  job->pos = pos;
  queue_.push_back(job);

  if (!threadActive_)
    activateQueue();
}

void TocEdit::queueScan(long start, long end)
{
  QueueJob* job = new QueueJob("scan");
  job->pos = start;
  job->end = end;
  queue_.push_back(job);

  if (!threadActive_)
    activateQueue();
}

void TocEdit::activateQueue()
{
  if (!threadActive_) {
    threadActive_ = true;
    blockEdit();
    signalCancelEnable(true);
    Glib::signal_idle().connect(sigc::mem_fun(*this,
                                              &TocEdit::queueThread));
    guiUpdate();
  }
}

void TocEdit::queueAbort()
{
  if (threadActive_) {
    queue_.clear();
    curState_ = TE_IDLE;
    if (curConv_) {
      curConv_->convertAbort();
      delete curConv_;
      curConv_ = NULL;
      signalStatusMessage("");
    }
  }
}

bool TocEdit::isQueueActive()
{
  return threadActive_;
}

// The queueThread is run by the Gtk idle thread when asynchronous
// work has to be done, such as decoding an MP3 file or reading
// samples from a WAV file.
//
// Asynchronous work (i.e. CPU-intensive work that has do be done in
// the background without blocking the GUI) can be scheduled by adding
// a new QueueJob object in the queue_ (see queueXXX methods above).

bool TocEdit::queueThread()
{
  static int pulse = 0;

  // If we're idle, pop next queue entry.
  if (curState_ == TE_IDLE) {

    // Queue empty ? Stop queue thread.
    if (queue_.empty()) {
      threadActive_ = false;
      unblockEdit();
      signalProgressFraction(0.0);
      signalCancelEnable(false);
      guiUpdate();
      return false; // false means disconnect idle thread
    }

    if (cur_)
      delete cur_;
    cur_ = queue_.front();
    queue_.pop_front();

    if (cur_->op == "scan") {
      if (curScan()) {
        curState_ = TE_READING;
        signalStatusMessage("Scanning audio data");
      } else {
        curState_ = TE_IDLE;
        return true;
      }
    } else {

      if (curConv_)
        delete curConv_;

      FormatSupport::Status err;
      curConv_ = formatConverter.newConverterStart(cur_->file.c_str(),
                                                   cur_->cfile, &err);
      if (curConv_) {
        std::string msg = "Decoding audio file ";
        msg += cur_->file;
        curState_ = TE_CONVERTING;
        signalStatusMessage(msg.c_str());

      } else {

        if (err != FormatSupport::FS_SUCCESS) {
          curSignalConversionError(err);
          curState_ = TE_IDLE;
          return true;
        }
        // File is already converted, or can't be converted (it's a WAV
        // or RAW file already).
        if (cur_->cfile.empty())
          cur_->cfile = cur_->file;
        curState_ = TE_CONVERTED;
      }
    }
  }

  // ------------------ TE_CONVERTING state: do file format conversion

  if (curState_ == TE_CONVERTING) {
    // Perform incremental file conversion.
    FormatSupport::Status status = curConv_->convertContinue();
    if (pulse++ > 5) {
      signalProgressPulse();
      pulse = 0;
    }

    // Still in progress, likely exit here.
    if (status == FormatSupport::FS_IN_PROGRESS)
      return true;

    // Conversion done.
    delete curConv_;
    curConv_ = NULL;

    if (status == FormatSupport::FS_SUCCESS)
      curState_ = TE_CONVERTED;
    else {
      curSignalConversionError(status);
      // Conversion failed, move on with next queue entry.
      curState_ = TE_IDLE;
    }

    return true;
  }

  // ------------------- TE_CONVERTED state: conversion done, prepare reading

  if (curState_ == TE_CONVERTED) {

    // Sanity check: the cfile (converted filename) will be read as
    // either a WAV file or a file containing raw samples. If the
    // extension is still that of an encoded audio file, return an
    // error. Otherwise it'll be read as a RAW samples file which is
    // not was users expect.

    TrackData::FileType ctype = TrackData::audioFileType(cur_->cfile.c_str());
    if (ctype != TrackData::RAW && ctype != TrackData::WAVE) {
      std::string msg = _("Cannot decode file");
      msg += " \"";
      msg += cur_->cfile;
      msg += "\" : ";
      msg += _("unsupported audio format");
      signalError(msg.c_str());
      curState_ = TE_IDLE;
      return true;
    }

    // If all we wanted to do was format conversion, we're done.
    if (cur_->op == "convert") {
      toc_->markFileConversion(cur_->file.c_str(), cur_->cfile.c_str());
      curState_ = TE_IDLE;
      return true;
    }

    curState_ = TE_READING;
    signalProgressFraction(0.0);

    if (cur_->op == "aptrack") {
      if (!curAppendTrack()) {
        curState_ = TE_IDLE;
        return true;
      }
    } else if (cur_->op == "apfile") {
      if (!curAppendFile()) {
        curState_ = TE_IDLE;
        return true;
      }

    } else if (cur_->op == "infile") {
      if (!curInsertFile()) {
        curState_ = TE_IDLE;
        return true;
      }
    }

    std::string msg = "Scanning audio file ";
    msg += cur_->file;
    signalStatusMessage(msg.c_str());
  }

  // ------------------- TE_READING state : read/scan WAV samples

  if (curState_ == TE_READING) {

    int result = sampleManager_->readSamples();

    if (result != 0) {

      if (result < 0)
        signalError(_("An error occured while reading audio data"));

      else {
        // Post operating code here.
        if (cur_->op == "aptrack") {
          updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
          signalFullView();
          std::string msg = "Appended track ";
          msg += cur_->file;
          signalStatusMessage(msg.c_str());
          guiUpdate();
        } else if (cur_->op == "apfile") {
          updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
          signalFullView();
          std::string msg = "Appended file ";
          msg += cur_->file;
          signalStatusMessage(msg.c_str());
          guiUpdate();
        } else if (cur_->op == "infile") {
          updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
          signalFullView();
          std::string msg = "Inserted file ";
          msg += cur_->file;
          signalStatusMessage(msg.c_str());
          signalSampleSelection(cur_->pos, cur_->pos + cur_->len - 1);
          guiUpdate();
        } else if (cur_->op == "scan") {
          std::stringstream ss;
          ss << "Scanned ";
          ss << (cur_->end - cur_->pos + 1);
          ss << " samples of data";
          std::string msg = ss.str();
          signalStatusMessage(msg.c_str());
          updateLevel_ |= UPD_SAMPLES;
        }
      }
      curState_ = TE_IDLE;
    }
    return true;
  }
  return true;
}

int TocEdit::appendSilence(unsigned long length)
{
  if (!editable())
    return 0;

  if (length > 0) {
    long start, end;

    TrackData *data = new TrackData(length);
    TrackDataList list;
    list.append(data);

    if (toc_->appendTrackData(&list, &start, &end) == 0) {

      sampleManager_->scanToc(Msf(start).samples(), Msf(end).samples() - 1,
                              true);
      
      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
    }
  }

  return 0;
}

// Return: 0: OK
//         1: No modify allowed
//         2: error?
int TocEdit::insertSilence(unsigned long length, unsigned long pos)
{
  if (!editable())
    return 1;

  if (length > 0) {
    TrackData *data = new TrackData(length);
    TrackDataList list;

    list.append(data);

    if (toc_->insertTrackData(pos, &list) == 0) {
      sampleManager_->insertSamples(pos, length, NULL);
      sampleManager_->scanToc(pos, pos + length, true);

      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
      return 0;
    }
  }

  return 2;
}


void TocEdit::setTrackCopyFlag(int trackNr, int flag)
{
  if (!editable())
    return;

  Track *t = toc_->getTrack(trackNr);
  
  if (t != NULL) {
    t->copyPermitted(flag);
    tocDirty(1);
    updateLevel_ |= UPD_TRACK_DATA;
  }
}

void TocEdit::setTrackPreEmphasisFlag(int trackNr, int flag)
{
  if (!editable())
    return;

  Track *t = toc_->getTrack(trackNr);

  if (t != NULL) {
    t->preEmphasis(flag);
    tocDirty(1);
    updateLevel_ |= UPD_TRACK_DATA;
  }
}

void TocEdit::setTrackAudioType(int trackNr, int flag)
{
  if (!editable())
    return;

  Track *t = toc_->getTrack(trackNr);

  if (t != NULL) {
    t->audioType(flag);
    tocDirty(1);
    updateLevel_ |= UPD_TRACK_DATA;
  }
  
}

void TocEdit::setTrackIsrcCode(int trackNr, const char *s)
{
  if (!editable())
    return;

  Track *t = toc_->getTrack(trackNr);

  if (t != NULL) {
    if (t->isrc(s) == 0) {
      tocDirty(1);
      updateLevel_ |= UPD_TRACK_DATA;
    }
  }
}

void TocEdit::setCdTextItem(int trackNr, CdTextItem::PackType type,
			    int blockNr, const char *s)
{
  if (!editable())
    return;

  if (s != NULL) {
    CdTextItem *item = new CdTextItem(type, blockNr, s);

    toc_->addCdTextItem(trackNr, item);
  }
  else {
    toc_->removeCdTextItem(trackNr, type, blockNr);
  }

  tocDirty(1);

  updateLevel_ |= (trackNr == 0) ? UPD_TOC_DATA : UPD_TRACK_DATA;

}

void TocEdit::setCdTextGenreItem(int blockNr, int code1, int code2,
				 const char *description)
{
  if (code1 > 255 || code2 > 255)
    return;

  if (!editable())
    return;

  if (code1 < 0 || code2 < 0) {
    toc_->removeCdTextItem(0, CdTextItem::CDTEXT_GENRE, blockNr);
  }
  else {
    CdTextItem *item = new CdTextItem(blockNr, (unsigned char)code1,
				      (unsigned char)code2, description);

    toc_->addCdTextItem(0, item);
  }

  tocDirty(1);

  updateLevel_ |= UPD_TOC_DATA;
}


void TocEdit::setCdTextLanguage(int blockNr, int langCode)
{
  if (!editable())
    return;

  toc_->cdTextLanguage(blockNr, langCode);
  tocDirty(1);

  updateLevel_ |= UPD_TOC_DATA;

}

void TocEdit::setCatalogNumber(const char *s)
{
  if (!editable())
    return;

  if (toc_->catalog(s) == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TOC_DATA;
  }

}

void TocEdit::setTocType(Toc::TocType type)
{
  if (!editable())
    return;

  toc_->tocType(type);
  tocDirty(1);
  updateLevel_ |= UPD_TOC_DATA;
}


// Removes selected track data
// Return: 0: OK
//         1: no selection
//         2: selection crosses track boundaries
//         3: cannot modify data track
int TocEdit::removeTrackData(TocEditView *view)
{
  TrackDataList *list;
  unsigned long selMin, selMax;

  if (!editable())
    return 0;

  if (!view->sampleSelection(&selMin, &selMax))
    return 1;

  switch (toc_->removeTrackData(selMin, selMax, &list)) {
  case 0:
    if (list != NULL) {
      if (list->length() > 0) {
	delete trackDataScrap_;
	trackDataScrap_ = new TrackDataScrap(list);
      }
      else {
	delete list;
      }

      sampleManager_->removeSamples(selMin, selMax, trackDataScrap_);

      view->sampleSelectionClear();
      view->sampleMarker(selMin);

      tocDirty(1);
    }
    break;

  case 1:
    return 2;
    break;

  case 2:
    return 3;
    break;
  }
  return 0;
}

// Inserts track data from scrap
// Return: 0: OK
//         1: no scrap data to paste
int TocEdit::insertTrackData(TocEditView *view)
			     
{
  if (!editable())
    return 0;

  if (trackDataScrap_ == NULL)
    return 1;

  unsigned long len = trackDataScrap_->trackDataList()->length();
  unsigned long marker;

  if (view->sampleMarker(&marker) && marker < toc_->length().samples()) {
    if (toc_->insertTrackData(marker, trackDataScrap_->trackDataList())
	== 0) {

      sampleManager_->insertSamples(marker, len, trackDataScrap_);
      sampleManager_->scanToc(marker, marker, true);
      sampleManager_->scanToc(marker + len - 1, marker + len - 1, true);
      
      view->sampleSelect(marker, marker + len - 1);
    
      tocDirty(1);
//llanero: different views
//      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
    }
  }
  else {
    long start, end;

    if (toc_->appendTrackData(trackDataScrap_->trackDataList(), &start, &end)
	== 0) {

      sampleManager_->insertSamples(Msf(start).samples(), 
				    Msf(end - start).samples(),
                                    trackDataScrap_);

      sampleManager_->scanToc(Msf(start).samples(), Msf(start).samples(),
                              true);
      if (end > 0) 
	sampleManager_->scanToc(Msf(start).samples() + len,
                                Msf(end).samples() - 1, true);

      view->sampleSelect(Msf(start).samples(), Msf(end).samples() - 1);
      
      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
    }
  }
  return 0;
}
