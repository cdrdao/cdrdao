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
 * $Log: TrackInfoDialog.h,v $
 * Revision 1.1.1.1  2000/02/05 01:38:55  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:27:16  mueller
 * Initial revision
 *
 */

#ifndef __TRACK_INFO_DIALOG_H__
#define __TRACK_INFO_DIALOG_H__

#include <gtk--.h>
#include <gtk/gtk.h>

class Toc;
class TocEdit;
class TextEdit;

class TrackInfoDialog : public Gtk_Dialog {
public:
  TrackInfoDialog();
  ~TrackInfoDialog();

  gint delete_event_impl(GdkEventAny*);

  void update(unsigned long, TocEdit *);

  void start(TocEdit *);
  void stop();

private:
  TocEdit *tocEdit_;
  int active_;

  int selectedTrack_;

  Gtk_Button *applyButton_;

  Gtk_Label *trackNr_;
  Gtk_Label *pregapLen_;
  Gtk_Label *trackStart_;
  Gtk_Label *trackEnd_;
  Gtk_Label *trackLen_;
  Gtk_Label *indexMarks_;

  Gtk_CheckButton *copyFlag_;
  Gtk_CheckButton *preEmphasisFlag_;

  Gtk_RadioButton *twoChannelAudio_;
  Gtk_RadioButton *fourChannelAudio_;

  TextEdit *isrcCodeCountry_;
  TextEdit *isrcCodeOwner_;
  TextEdit *isrcCodeYear_;
  TextEdit *isrcCodeSerial_;

  struct CdTextPage {
    Gtk_Label *label;
    Gtk_Entry *title;
    Gtk_Entry *performer;
    Gtk_Entry *songwriter;
    Gtk_Entry *composer;
    Gtk_Entry *arranger;
    Gtk_Entry *message;
    Gtk_Entry *isrc;
  };

  CdTextPage cdTextPages_[8];
  
  void cancelAction();
  void applyAction();

  Gtk_VBox *createCdTextPage(int);

  void clear();
  void clearCdText();

  const char *checkString(const string &);
  void importCdText(const Toc *, int);
  void importData(const Toc *, int);
  void exportCdText(TocEdit *, int);
  void exportData(TocEdit *, int);
};

#endif
