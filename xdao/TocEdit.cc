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

  updateLevel_ = 0;
  editBlocked_ = 0;

  if (filename == NULL)
    toc(t, "unnamed.toc");
  else
    toc(t, filename);
}

TocEdit::~TocEdit()
{
  delete toc_;
  toc_ = NULL;

  delete sampleManager_;
  sampleManager_ = NULL;

  delete[] filename_;
  filename_ = NULL;

  delete trackDataScrap_;
  trackDataScrap_ = NULL;
}

int TocEdit::toc(Toc *t, const char *filename)
{
  delete toc_;

  if (t == NULL)
    toc_ = new Toc;
  else
    toc_ = t;

  if (filename != NULL) {
    delete[] filename_;
    filename_ = strdupCC(filename);
  }

  tocDirty_ = 0;
  editBlocked_ = 0;

  delete sampleManager_;
  sampleManager_ = new SampleManager(588);

  sampleManager_->setTocEdit(this);

  if (toc_->length().samples() > 0) {
    unsigned long maxSample = toc_->length().samples() - 1;
    int ret = sampleManager_->scanToc(0, maxSample);
    if (ret)
      return ret;
  }

  updateLevel_ = UPD_ALL;
  return 0;
}

Toc *TocEdit::toc() const
{
  return toc_;
}

SampleManager *TocEdit::sampleManager()
{
  return sampleManager_;
}

void TocEdit::tocDirty(int f)
{
  int old = tocDirty_;

  tocDirty_ = (f != 0) ? 1 : 0;

  if (old != tocDirty_)
    updateLevel_ |= UPD_TOC_DIRTY;
}

int TocEdit::tocDirty() const
{
  return tocDirty_;
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

bool TocEdit::editable() const
{
  return (editBlocked_ == 0);
}

int TocEdit::modifyAllowed() const
{
  return (editBlocked_ == 0) ? 1 : 0;
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
  if (!modifyAllowed())
    return 0;

  if (fname == NULL || *fname == 0)
    return 1;

  Toc *t = Toc::read(fname);

  if (t != NULL) {
    return toc(t, fname);
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
  if (!modifyAllowed())
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
  if (!modifyAllowed())
    return 0;

  int ret = toc_->addTrackMarker(lba);

  if (ret == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::addIndexMarker(long lba)
{
  if (!modifyAllowed())
    return 0;

  int ret = toc_->addIndexMarker(lba);

  if (ret == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::addPregap(long lba)
{
  if (!modifyAllowed())
    return 0;

  int ret = toc_->addPregap(lba);

  if (ret == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TRACK_DATA;
  }

  return ret;
}

int TocEdit::removeTrackMarker(int trackNr, int indexNr)
{
  if (!modifyAllowed())
    return 0;

  int ret = toc_->removeTrackMarker(trackNr, indexNr);

  if (ret == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
  }

  return ret;
}


// Creates an audio data object for given filename. Errors are send to
// status line.
// data: filled with newly allocated TrackData object on success
// return: 0: OK
//         1: cannot open file
//         2: file has wrong format
int TocEdit::createAudioData(const char *filename, TrackData **data)
{
  unsigned long len;

  switch (TrackData::checkAudioFile(filename, &len)) {
  case 1:
    return 1; // Cannot open file

  case 2:
    return 2; // File format error
  }

  *data = new TrackData(filename, 0, len);

  return 0;
}

int TocEdit::appendTrack(const char* filename)
{
  std::list<std::string> flist;
  flist.push_front(filename);
  return appendTracks(flist);
}

int TocEdit::appendTracks(std::list<std::string>& tracks)
{
  long glstart = 0, glend = 0;

  if (!modifyAllowed())
    return 0;

  for (std::list<std::string>::iterator i = tracks.begin(); i != tracks.end();
       i++) {

    TrackData *data;
    int ret;

    if ((ret = createAudioData((*i).c_str(), &data)) == 0) {
      TrackDataList list;
      long start, end;
      list.append(data);

      toc_->appendTrack(&list, &start, &end);
      if (!glend) {
        glstart = start;
        glend = end;
      } else {
        glstart = (start < glstart ? start : glstart);
        glend = (end > glend ? end : glend);
      }
      tocDirty(1);

    } else {
      return ret;
    }
  }

  sampleManager_->scanToc(Msf(glstart).samples(), Msf(glend).samples() - 1);
  updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;

  return 0;
}

int TocEdit::appendFile(const char* filename)
{
  using namespace std;
  list<string> flist;
  flist.push_front(filename);
  return appendFiles(flist);
}

int TocEdit::appendFiles(std::list<std::string>& tracks)
{
  long glstart = 0, glend = 0;

  if (!modifyAllowed())
    return 0;

  for (std::list<std::string>::iterator i = tracks.begin(); i != tracks.end();
       i++) {

    TrackData *data;
    int ret;

    if ((ret = createAudioData((*i).c_str(), &data)) == 0) {
      TrackDataList list;
      long start, end;
      list.append(data);

      if (toc_->appendTrackData(&list, &start, &end) != 0)
        break;

      if (!glend) {
        glstart = start;
        glend = end;
      } else {
        glstart = (start < glstart ? start : glstart);
        glend = (end > glend ? end : glend);
      }

    } else {
      return ret;
    }
  }

  sampleManager_->scanToc(Msf(glstart).samples(), Msf(glend).samples() - 1);
  tocDirty(1);
  updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA;
  return 0;
}

// Inserts contents of specified file at given position.
// Return: 0: OK
//         1: cannot open file
//         2: file has invalid format
int TocEdit::insertFile(const char *fname, unsigned long pos, unsigned long *len)
{
  int ret;
  TrackData *data;

  if (!modifyAllowed())
    return 0;

  if ((ret = createAudioData(fname, &data)) == 0) {
    TrackDataList list;
    list.append(data);

    if (toc_->insertTrackData(pos, &list) == 0) {
//      unsigned long len = list.length();
      *len = list.length();

      sampleManager_->insertSamples(pos, *len, NULL);
      sampleManager_->scanToc(pos, pos + *len);
      
      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
    }
  }

  return ret;
}

int TocEdit::insertFiles(std::list<std::string>& tracks, unsigned long pos,
                         unsigned long *len)
{
  if (!modifyAllowed())
    return 0;

  using namespace std;

  int ret;
  int ipos = pos;
  *len = 0;

  for (list<string>::iterator i = tracks.begin(); i != tracks.end(); i++) {

    int ilen;
    TrackData* data;
    ret = createAudioData((*i).c_str(), &data);
    if (ret == 0) {
      TrackDataList list;
      list.append(data);

      if (toc_->insertTrackData(ipos, &list) == 0) {
        ilen = list.length();

        sampleManager_->insertSamples(ipos, ilen, NULL);
        ipos = ipos + ilen;
        *len += ilen;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (*len > 0) {
    sampleManager_->scanToc(pos, pos + *len);
    tocDirty(1);
    updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
  }

  return ret;
}

int TocEdit::appendSilence(unsigned long length)
{
  if (!modifyAllowed())
    return 0;

  if (length > 0) {
    long start, end;

    TrackData *data = new TrackData(TrackData::AUDIO, TrackData::SUBCHAN_NONE,
				    length);
    TrackDataList list;
    list.append(data);

    if (toc_->appendTrackData(&list, &start, &end) == 0) {

      sampleManager_->scanToc(Msf(start).samples(), Msf(end).samples() - 1);
      
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
  if (!modifyAllowed())
    return 1;

  if (length > 0) {
    TrackData *data = new TrackData(TrackData::AUDIO, TrackData::SUBCHAN_NONE,
				    length);
    TrackDataList list;

    list.append(data);

    if (toc_->insertTrackData(pos, &list) == 0) {
      sampleManager_->insertSamples(pos, length, NULL);
      sampleManager_->scanToc(pos, pos + length);

      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
      return 0;
    }
  }

  return 2;
}


void TocEdit::setTrackCopyFlag(int trackNr, int flag)
{
  if (!modifyAllowed())
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
  if (!modifyAllowed())
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
  if (!modifyAllowed())
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
  if (!modifyAllowed())
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
  if (!modifyAllowed())
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

  if (!modifyAllowed())
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
  if (!modifyAllowed())
    return;

  toc_->cdTextLanguage(blockNr, langCode);
  tocDirty(1);

  updateLevel_ |= UPD_TOC_DATA;

}

void TocEdit::setCatalogNumber(const char *s)
{
  if (!modifyAllowed())
    return;

  if (toc_->catalog(s) == 0) {
    tocDirty(1);
    updateLevel_ |= UPD_TOC_DATA;
  }

}

void TocEdit::setTocType(Toc::TocType type)
{
  if (!modifyAllowed())
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

  if (!modifyAllowed())
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
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL | UPD_SAMPLE_MARKER | UPD_SAMPLES ;
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
  if (!modifyAllowed())
    return 0;

  if (trackDataScrap_ == NULL)
    return 1;

  unsigned long len = trackDataScrap_->trackDataList()->length();
  unsigned long marker;

  if (view->sampleMarker(&marker) && marker < toc_->length().samples()) {
    if (toc_->insertTrackData(marker, trackDataScrap_->trackDataList())
	== 0) {

      sampleManager_->insertSamples(marker, len, trackDataScrap_);
      sampleManager_->scanToc(marker, marker);
      sampleManager_->scanToc(marker + len - 1,
			      marker + len - 1);
      
      view->sampleSelection(marker, marker + len - 1);
    
      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
    }
  }
  else {
    long start, end;

    if (toc_->appendTrackData(trackDataScrap_->trackDataList(), &start, &end)
	== 0) {

      sampleManager_->insertSamples(Msf(start).samples(), 
				    Msf(end - start).samples(), trackDataScrap_);

      sampleManager_->scanToc(Msf(start).samples(), Msf(start).samples());
      if (end > 0) 
	sampleManager_->scanToc(Msf(start).samples() + len, Msf(end).samples() - 1);

      view->sampleSelection(Msf(start).samples(), Msf(end).samples() - 1);
      
      tocDirty(1);
      updateLevel_ |= UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL;
    }
  }
  return 0;
}
