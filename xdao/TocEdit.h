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
 * Revision 1.5  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.4.6.2  2004/01/19 18:17:51  poolshark
 * Support for multiple selections in add track dialog
 *
 * Revision 1.4.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.2  2003/12/12 02:49:36  denis
 * AudioCDProject and AudioCDView cleanup.
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.4  2000/11/05 12:24:41  andreasm
 * Improved handling of TocEdit views. Introduced a new class TocEditView that
 * holds all view data (displayed sample range, selected sample range,
 * selected tracks/index marks, sample marker). This class is passed now to
 * most of the update functions of the dialogs.
 *
 * Revision 1.3  2000/09/21 02:07:07  llanero
 * MDI support:
 * Splitted AudioCDChild into same and AudioCDView
 * Move Selections from TocEdit to AudioCDView to allow
 *   multiple selections.
 * Cursor animation in all the views.
 * Can load more than one from from command line
 * Track info, Toc info, Append/Insert Silence, Append/Insert Track,
 *   they all are built for every child when needed.
 * ...
 *
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

#include <string>
#include <list>

#include "Toc.h"
#include "CdTextItem.h"

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

  void tocDirty(int);
  int tocDirty() const;

  void blockEdit();
  void unblockEdit();
  bool editable() const;

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

  int appendTrack(const char* filename);
  int appendTracks(std::list<std::string>& tracks);
  int appendFile(const char* filename);
  int appendFiles(std::list<std::string>& tracks);
  int insertFile(const char *fname, unsigned long pos, unsigned long *len);
  int insertFiles(std::list<std::string>& tracks, unsigned long pos,
                  unsigned long *len);
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
  
private:
  Toc *toc_;
  SampleManager *sampleManager_;

  char *filename_;

  TrackDataScrap *trackDataScrap_;

  int tocDirty_;
  int editBlocked_;

  unsigned long updateLevel_;

  int createAudioData(const char *filename, TrackData **);
  int modifyAllowed() const;

  friend class TocEditView;
};

#endif
