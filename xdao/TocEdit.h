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
/*
 * $Log: TocEdit.h,v $
 * Revision 1.2  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.1.1.1  2000/02/05 01:38:52  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:27:39  mueller
 * Initial revision
 *
 */

#ifndef __TOC_EDIT_H__
#define __TOC_EDIT_H__

#include "Toc.h"
#include "CdTextItem.h"

class Toc;
class TrackData;
class TrackDataScrap;
class SampleManager;

class TocEdit {
public:
  TocEdit(Toc *, const char *);
  ~TocEdit();

  void toc(Toc *, const char *);
  Toc *toc() const;

  SampleManager *sampleManager();

  unsigned long lengthSample() const;

  void tocDirty(int);
  int tocDirty() const;

  void blockEdit();
  void unblockEdit();
  int editable() const;

  // returns and resets update level
  unsigned long updateLevel();

  void filename(const char *);
  const char *filename() const;

  void sampleMarker(unsigned long);
  int sampleMarker(unsigned long *) const;

  void sampleSelection(unsigned long, unsigned long);
  int sampleSelection(unsigned long *, unsigned long *) const;

  void sampleViewFull();
  void sampleViewInclude(unsigned long, unsigned long);
  void sampleView(unsigned long *, unsigned long *) const;
  void sampleView(unsigned long smin, unsigned long smax);

  void trackSelection(int);
  int trackSelection(int *) const;

  void indexSelection(int);
  int indexSelection(int *) const;

  int readToc(const char *);
  int saveToc();
  int saveAsToc(const char *);
  

  int moveTrackMarker(int trackNr, int indexNr, long lba);
  int addTrackMarker(long lba);
  int removeTrackMarker(int trackNr, int indexNr);
  int addIndexMarker(long lba);
  int addPregap(long lba);


  int appendTrack(const char *fname);
  int appendFile(const char *fname);
  int insertFile(const char *fname, unsigned long pos);
  int appendSilence(unsigned long);
  int insertSilence(unsigned long length, unsigned long pos);

  int removeTrackData();
  int insertTrackData();
  
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
  
private:
  Toc *toc_;
  SampleManager *sampleManager_;

  char *filename_;

  TrackDataScrap *trackDataScrap_;

  int tocDirty_;
  int editBlocked_;

  unsigned long updateLevel_;

  int sampleMarkerValid_;
  unsigned long sampleMarker_;

  int sampleSelectionValid_;
  unsigned long sampleSelectionMin_;
  unsigned long sampleSelectionMax_;

  unsigned long sampleViewMin_;
  unsigned long sampleViewMax_;
  
  int trackSelectionValid_;
  int trackSelection_;

  int indexSelectionValid_;
  int indexSelection_;

  int createAudioData(const char *filename, TrackData **);
  int modifyAllowed() const;
};

#endif
