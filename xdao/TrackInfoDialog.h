/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __TRACK_INFO_DIALOG_H__
#define __TRACK_INFO_DIALOG_H__

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <string>

class Toc;
class TocEdit;
class TocEditView;
class TextEdit;

class TrackInfoDialog : public Gtk::Dialog {
public:
  TrackInfoDialog();
  ~TrackInfoDialog();

  bool on_delete_event(GdkEventAny*);

  void update(unsigned long, TocEditView *);

  void start(TocEditView *);
  void stop();

private:
  TocEditView *tocEditView_;
  int active_;

  int selectedTrack_;

  Gtk::Button *applyButton_;

  Gtk::Label *trackNr_;
  Gtk::Label *pregapLen_;
  Gtk::Label *trackStart_;
  Gtk::Label *trackEnd_;
  Gtk::Label *trackLen_;
  Gtk::Label *indexMarks_;

  Gtk::CheckButton *copyFlag_;
  Gtk::CheckButton *preEmphasisFlag_;

  Gtk::RadioButton *twoChannelAudio_;
  Gtk::RadioButton *fourChannelAudio_;

  TextEdit *isrcCodeCountry_;
  TextEdit *isrcCodeOwner_;
  TextEdit *isrcCodeYear_;
  TextEdit *isrcCodeSerial_;

  struct CdTextPage {
    Gtk::Label *label;
    Gtk::Entry *title;
    Gtk::Entry *performer;
    Gtk::Entry *songwriter;
    Gtk::Entry *composer;
    Gtk::Entry *arranger;
    Gtk::Entry *message;
    Gtk::Entry *isrc;
  };

  CdTextPage cdTextPages_[8];
  
  void closeAction();
  void applyAction();

  Gtk::VBox *createCdTextPage(int);

  void clear();
  void clearCdText();

  const char *checkString(const std::string &);
  void importCdText(const Toc *, int);
  void importData(const Toc *, int);
  void exportCdText(TocEdit *, int);
  void exportData(TocEdit *, int);
};

#endif
