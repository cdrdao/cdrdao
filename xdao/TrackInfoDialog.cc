/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: TrackInfoDialog.cc,v $
 * Revision 1.3  2000/04/16 20:31:20  andreasm
 * Added missing stdio.h includes.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:40:28  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:27:16  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: TrackInfoDialog.cc,v 1.3 2000/04/16 20:31:20 andreasm Exp $";

#include "TrackInfoDialog.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "TocEdit.h"
#include "guiUpdate.h"
#include "Toc.h"
#include "Track.h"
#include "CdTextItem.h"
#include "TextEdit.h"

TrackInfoDialog::TrackInfoDialog()
{
  int i;
  Gtk::Label *label, *label1;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox, *vbox1;
  Gtk::Frame *frame;
  Gtk::Table *table;
  Gtk::Button *button;
  Gtk::VBox *contents = new Gtk::VBox;
  Gtk::HBox *topBox = new Gtk::HBox;

  tocEdit_ = NULL;
  active_ = 0;
  trackNr_ = 0;

  trackNr_ = new Gtk::Label(string("99"));
  pregapLen_ = new Gtk::Label(string("100:00:00"));
  trackStart_ = new Gtk::Label(string("100:00:00"));
  trackEnd_ = new Gtk::Label(string("100:00:00"));
  trackLen_ = new Gtk::Label(string("100:00:00"));
  indexMarks_ = new Gtk::Label(string("99"));

  copyFlag_ = new Gtk::CheckButton(string("Copy"));
  preEmphasisFlag_ = new Gtk::CheckButton(string("Pre Emphasis"));
  
  twoChannelAudio_ = new Gtk::RadioButton(string("Two Channel Audio"));
  fourChannelAudio_ = new Gtk::RadioButton(string("Four Channel Audio"));
  fourChannelAudio_->set_group(twoChannelAudio_->group());

  isrcCodeCountry_ = new TextEdit("XX");
  isrcCodeCountry_->set_max_length(2);
  isrcCodeCountry_->lowerCase(0);
  isrcCodeCountry_->space(0);
  isrcCodeCountry_->digits(0);

  isrcCodeOwner_ = new TextEdit("XXX");
  isrcCodeOwner_->set_max_length(3);
  isrcCodeOwner_->lowerCase(0);
  isrcCodeOwner_->space(0);

  isrcCodeYear_ = new TextEdit("00");
  isrcCodeYear_->set_max_length(2);
  isrcCodeYear_->lowerCase(0);
  isrcCodeYear_->upperCase(0);
  isrcCodeYear_->space(0);

  isrcCodeSerial_ = new TextEdit("00000");
  isrcCodeSerial_->set_max_length(5);
  isrcCodeSerial_->lowerCase(0);
  isrcCodeSerial_->upperCase(0);
  isrcCodeSerial_->space(0);

  topBox->set_spacing(5);
  contents->set_spacing(10);

  hbox = new Gtk::HBox;

  label = new Gtk::Label(string("Track: "));

  hbox->pack_start(*label, FALSE, FALSE);
  label->show();
  hbox->pack_start(*trackNr_, FALSE, FALSE);
  trackNr_->show();

  contents->pack_start(*hbox, FALSE, FALSE);
  hbox->show();


  // time data
  frame = new Gtk::Frame(string("Summary"));

  table = new Gtk::Table(5, 2, FALSE);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  hbox = new Gtk::HBox;
  hbox->pack_start(*table, FALSE, FALSE, 5);
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, TRUE, TRUE, 5);
  frame->add(*vbox);
  vbox->show();
  hbox->show();
  table->show();
  
  label = new Gtk::Label(string("Pre-Gap:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 0, 1);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*pregapLen_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 0, 1);
  pregapLen_->show();
  hbox->show();

  label = new Gtk::Label(string("Start:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 1, 2);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*trackStart_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 1, 2);
  trackStart_->show();
  hbox->show();

  label = new Gtk::Label(string("End:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 2, 3);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*trackEnd_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 2, 3);
  trackEnd_->show();
  hbox->show();

  label = new Gtk::Label(string("Length:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 3, 4);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*trackLen_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 3, 4);
  trackLen_->show();
  hbox->show();

  label = new Gtk::Label(string("Index Marks:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 4, 5);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*indexMarks_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 4, 5);
  indexMarks_->show();
  hbox->show();

  topBox->pack_start(*frame, FALSE, FALSE);
  frame->show();



  // sub-channel data
  frame = new Gtk::Frame(string("Sub-Channel"));

  vbox = new Gtk::VBox;
  vbox->set_spacing(0);

  vbox->pack_start(*copyFlag_);
  copyFlag_->show();

  vbox->pack_start(*preEmphasisFlag_);
  preEmphasisFlag_->show();

  vbox->pack_start(*twoChannelAudio_);
  twoChannelAudio_->show();
  twoChannelAudio_->set_active(TRUE);

  vbox->pack_start(*fourChannelAudio_);
  fourChannelAudio_->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("ISRC: "));
  hbox->pack_start(*label, FALSE);

  hbox->pack_start(*isrcCodeCountry_, FALSE);
  label1 = new Gtk::Label(string("-"));
  hbox->pack_start(*label1, FALSE);
  isrcCodeCountry_->show();
  label1->show();

  hbox->pack_start(*isrcCodeOwner_, FALSE);
  label1 = new Gtk::Label(string("-"));
  hbox->pack_start(*label1, FALSE);
  isrcCodeOwner_->show();
  label1->show();

  hbox->pack_start(*isrcCodeYear_, FALSE);
  label1 = new Gtk::Label(string("-"));
  hbox->pack_start(*label1, FALSE);
  isrcCodeYear_->show();
  label1->show();

  hbox->pack_start(*isrcCodeSerial_, FALSE);
  isrcCodeSerial_->show();

  vbox->pack_start(*hbox);
  label->show();
  hbox->show();
  
  vbox1 = new Gtk::VBox;
  vbox1->pack_start(*vbox, TRUE, TRUE, 5);
  hbox = new Gtk::HBox;
  hbox->pack_start(*vbox1, TRUE, TRUE, 5);
  frame->add(*hbox);
  vbox->show();
  vbox1->show();
  hbox->show();

  topBox->pack_start(*frame, FALSE);
  frame->show();

  contents->pack_start(*topBox, FALSE);
  topBox->show();


  // CD-TEXT data
  frame = new Gtk::Frame(string("CD-TEXT"));

  Gtk::Notebook *notebook = new Gtk::Notebook;

  for (i = 0; i < 8; i++) {
    vbox = createCdTextPage(i);
    notebook->pages().push_back(Gtk::Notebook_Helpers::TabElem(*vbox, *(cdTextPages_[i].label)));
  }

  vbox1 = new Gtk::VBox;
  vbox1->pack_start(*notebook, TRUE, TRUE, 5);
  hbox = new Gtk::HBox;
  hbox->pack_start(*vbox1, TRUE, TRUE, 5);
  frame->add(*hbox);
  notebook->show();
  vbox1->show();
  hbox->show();

  contents->pack_start(*frame, TRUE);
  frame->show();



  hbox = new Gtk::HBox;
  hbox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();
  get_vbox()->pack_start(*hbox, TRUE, TRUE, 10);
  hbox->show();

  get_vbox()->show();

  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);

  applyButton_ = new Gtk::Button(string(" Apply "));
  bbox->pack_start(*applyButton_);
  applyButton_->show();
  applyButton_->clicked.connect(slot(this, &TrackInfoDialog::applyAction));

  button = new Gtk::Button(string(" Cancel "));
  bbox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &TrackInfoDialog::cancelAction));

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();

  set_title(string("Track Info"));
}

TrackInfoDialog::~TrackInfoDialog()
{
}

void TrackInfoDialog::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_ALL, tocEdit);
  show();
}

void TrackInfoDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}


Gtk::VBox *TrackInfoDialog::createCdTextPage(int n)
{
  char buf[20];
  Gtk::Table *table = new Gtk::Table(7, 2, FALSE);
  Gtk::VBox *vbox = new Gtk::VBox;
  Gtk::HBox *hbox;
  Gtk::Label *label;

  sprintf(buf, "%d", n);
  cdTextPages_[n].label = new Gtk::Label(string(buf));
  cdTextPages_[n].label->show();

  cdTextPages_[n].title = new Gtk::Entry;
  cdTextPages_[n].performer = new Gtk::Entry;
  cdTextPages_[n].songwriter = new Gtk::Entry;
  cdTextPages_[n].composer = new Gtk::Entry;
  cdTextPages_[n].arranger = new Gtk::Entry;
  cdTextPages_[n].message = new Gtk::Entry;
  cdTextPages_[n].isrc = new Gtk::Entry;


  table->set_row_spacings(5);
  table->set_col_spacings(5);
  table->show();

  label = new Gtk::Label(string("Title:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 0, 1, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].title), 1, 2, 0, 1);
  cdTextPages_[n].title->show();

  label = new Gtk::Label(string("Performer:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 1, 2, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].performer), 1, 2, 1, 2);
  cdTextPages_[n].performer->show();

  label = new Gtk::Label(string("Songwriter:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 2, 3, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].songwriter), 1, 2, 2, 3);
  cdTextPages_[n].songwriter->show();

  label = new Gtk::Label(string("Composer:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 3, 4, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].composer), 1, 2, 3, 4);
  cdTextPages_[n].composer->show();

  label = new Gtk::Label(string("Arranger:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 4, 5, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].arranger), 1, 2, 4, 5);
  cdTextPages_[n].arranger->show();

  label = new Gtk::Label(string("Message:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 5, 6, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].message), 1, 2, 5, 6);
  cdTextPages_[n].message->show();

  label = new Gtk::Label(string("ISRC:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE);
  table->attach(*hbox, 0, 1, 6, 7, GTK_FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].isrc), 1, 2, 6, 7);
  cdTextPages_[n].isrc->show();


  vbox->pack_start(*table, TRUE);
  vbox->show();

  return vbox;
}

gint TrackInfoDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void TrackInfoDialog::cancelAction()
{
  stop();
}


void TrackInfoDialog::clear()
{
  trackNr_->set_text(string(""));
  pregapLen_->set_text(string(""));
  trackStart_->set_text(string(""));
  trackEnd_->set_text(string(""));
  trackLen_->set_text(string(""));
  indexMarks_->set_text(string(""));
  
  isrcCodeCountry_->set_text(string(""));
  isrcCodeCountry_->set_editable(false);
  isrcCodeOwner_->set_text(string(""));
  isrcCodeOwner_->set_editable(false);
  isrcCodeYear_->set_text(string(""));
  isrcCodeYear_->set_editable(false);
  isrcCodeSerial_->set_text(string(""));
  isrcCodeSerial_->set_editable(false);

  copyFlag_->set_sensitive(false);
  preEmphasisFlag_->set_sensitive(false);
  twoChannelAudio_->set_sensitive(false);
  fourChannelAudio_->set_sensitive(false);

  clearCdText();
}

void TrackInfoDialog::update(unsigned long level, TocEdit *tocEdit)
{
  const Toc *toc;

  if (!active_)
    return;

  tocEdit_ = tocEdit;

  if (tocEdit == NULL || !tocEdit->trackSelection(&selectedTrack_)) {
    selectedTrack_ = 0;
    applyButton_->set_sensitive(FALSE);
    clear();
    return;
  }

  if (level & (UPD_TRACK_DATA | UPD_TRACK_MARK_SEL)) {
    toc = tocEdit->toc();
    importData(toc, selectedTrack_);
    applyButton_->set_sensitive(tocEdit->editable() ? TRUE : FALSE);
  }
  else if (level & UPD_EDITABLE_STATE) {
    applyButton_->set_sensitive(tocEdit->editable() ? TRUE : FALSE);
  }
}


void TrackInfoDialog::clearCdText()
{
  int l;

  for (l = 0; l < 8; l++) {
    cdTextPages_[l].title->set_text(string(""));
    cdTextPages_[l].title->set_editable(false);

    cdTextPages_[l].performer->set_text(string(""));
    cdTextPages_[l].performer->set_editable(false);

    cdTextPages_[l].songwriter->set_text(string(""));
    cdTextPages_[l].songwriter->set_editable(false);

    cdTextPages_[l].composer->set_text(string(""));
    cdTextPages_[l].composer->set_editable(false);

    cdTextPages_[l].arranger->set_text(string(""));
    cdTextPages_[l].arranger->set_editable(false);

    cdTextPages_[l].message->set_text(string(""));
    cdTextPages_[l].message->set_editable(false);

    cdTextPages_[l].isrc->set_text(string(""));
    cdTextPages_[l].isrc->set_editable(false);
  }
}


void TrackInfoDialog::applyAction()
{
  if (tocEdit_ == NULL || !tocEdit_->editable() || selectedTrack_ == 0)
    return;

  exportData(tocEdit_, selectedTrack_);

  guiUpdate(UPD_TRACK_DATA);
}

const char *TrackInfoDialog::checkString(const string &str)
{
  static char *buf = NULL;
  static long bufLen = 0;
  char *p, *s;
  long len = strlen(str.c_str());

  if (len == 0)
    return NULL;

  if (buf == NULL || len + 1 > bufLen) {
    delete[] buf;
    bufLen = len + 1;
    buf = new char[bufLen];
  }

  strcpy(buf, str.c_str());

  s = buf;
  p = buf + len - 1;

  while (*s != 0 && isspace(*s))
    s++;

  if (*s == 0)
    return NULL;

  while (p > s && isspace(*p)) {
    *p = 0;
    p--;
  }
  
  return s;
}

void TrackInfoDialog::importCdText(const Toc *toc, int trackNr)
{
  int l;
  const CdTextItem *item; 

  for (l = 0; l < 8; l++) {
    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_TITLE))
	!= NULL) 
      cdTextPages_[l].title->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].title->set_text(string(""));
    cdTextPages_[l].title->set_editable(true);

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_PERFORMER))
	!= NULL) 
      cdTextPages_[l].performer->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].performer->set_text(string(""));
    cdTextPages_[l].performer->set_editable(true);

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_SONGWRITER))
	!= NULL) 
      cdTextPages_[l].songwriter->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].songwriter->set_text(string(""));
    cdTextPages_[l].songwriter->set_editable(true);

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_COMPOSER))
	!= NULL) 
      cdTextPages_[l].composer->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].composer->set_text(string(""));
    cdTextPages_[l].composer->set_editable(true);

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_ARRANGER))
	!= NULL) 
      cdTextPages_[l].arranger->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].arranger->set_text(string(""));
    cdTextPages_[l].arranger->set_editable(true);

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_MESSAGE))
	!= NULL) 
      cdTextPages_[l].message->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].message->set_text(string(""));
    cdTextPages_[l].message->set_editable(true);

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_UPCEAN_ISRC))
	!= NULL) 
      cdTextPages_[l].isrc->set_text(string((const char*)(item->data())));
    else
      cdTextPages_[l].isrc->set_text(string(""));
    cdTextPages_[l].isrc->set_editable(true);
  }
}

void TrackInfoDialog::importData(const Toc *toc, int trackNr)
{
  char buf[50];
  TrackIterator itr(toc);
  Msf start, end;

  const Track *track = itr.find(trackNr, start, end);
  
  if (track == NULL) {
    clear();
    return;
  }

  sprintf(buf, "%d", trackNr);
  trackNr_->set_text(string(buf));

  sprintf(buf, "%3d:%02d:%02d", track->start().min(), track->start().sec(),
	  track->start().frac());
  pregapLen_->set_text(string(buf));

  sprintf(buf, "%3d:%02d:%02d", start.min(), start.sec(), start.frac());
  trackStart_->set_text(string(buf));

  sprintf(buf, "%3d:%02d:%02d", end.min(), end.sec(), end.frac());
  trackEnd_->set_text(string(buf));

  Msf len(track->length() - track->start());
  sprintf(buf, "%3d:%02d:%02d", len.min(), len.sec(), len.frac());
  trackLen_->set_text(string(buf));

  sprintf(buf, "%3d", track->nofIndices());
  indexMarks_->set_text(string(buf));

  copyFlag_->set_sensitive(true);
  preEmphasisFlag_->set_sensitive(true);
  twoChannelAudio_->set_sensitive(true);
  fourChannelAudio_->set_sensitive(true);

  copyFlag_->set_active(track->copyPermitted());
  preEmphasisFlag_->set_active(track->preEmphasis());
  
  if (track->audioType() == 0)
    twoChannelAudio_->set_active(true);
  else
    fourChannelAudio_->set_active(true);
    
  if (track->isrcValid()) {
    sprintf(buf, "%c%c", track->isrcCountry(0), track->isrcCountry(1));
    isrcCodeCountry_->set_text(string(buf));
    
    sprintf(buf, "%c%c%c", track->isrcOwner(0), track->isrcOwner(1),
	    track->isrcOwner(2));
    isrcCodeOwner_->set_text(string(buf));

    sprintf(buf, "%c%c", '0' + track->isrcYear(0), '0' + track->isrcYear(1));
    isrcCodeYear_->set_text(string(buf));
    
    sprintf(buf, "%c%c%c%c%c", '0' + track->isrcSerial(0),
	    '0' + track->isrcSerial(1), '0' + track->isrcSerial(2),
	    '0' + track->isrcSerial(3), '0' + track->isrcSerial(4));
    isrcCodeSerial_->set_text(string(buf));
  }
  else {
    isrcCodeCountry_->set_text(string(""));
    isrcCodeOwner_->set_text(string(""));
    isrcCodeYear_->set_text(string(""));
    isrcCodeSerial_->set_text(string(""));
  }

  isrcCodeCountry_->set_editable(true);
  isrcCodeOwner_->set_editable(true);
  isrcCodeYear_->set_editable(true);
  isrcCodeSerial_->set_editable(true);

  importCdText(toc, trackNr);
}

void TrackInfoDialog::exportData(TocEdit *tocEdit, int trackNr)
{
  char buf[13];
  const char *s;
  Toc *toc = tocEdit->toc();
  Track *t = toc->getTrack(trackNr);
  int flag;

  if (t == NULL)
    return;

  flag = copyFlag_->get_active() ? 1 : 0;
  if (t->copyPermitted() != flag)
    tocEdit->setTrackCopyFlag(trackNr, flag);

  if (t->type() == TrackData::AUDIO) {
    flag = preEmphasisFlag_->get_active() ? 1 : 0;
    if (t->preEmphasis() != flag)
      tocEdit->setTrackPreEmphasisFlag(trackNr, flag);

    flag = twoChannelAudio_->get_active() ? 0 : 1;
    if (t->audioType() != flag)
      tocEdit->setTrackAudioType(trackNr, flag);

    buf[0] = 0;
    if ((s = checkString(isrcCodeCountry_->get_text())) != NULL &&
	strlen(s) == 2) {
      strcat(buf, s);
    }
    if ((s = checkString(isrcCodeOwner_->get_text())) != NULL &&
	strlen(s) == 3) {
      strcat(buf, s);
    }
    if ((s = checkString(isrcCodeYear_->get_text())) != NULL &&
	strlen(s) == 2) {
      strcat(buf, s);
    }
    if ((s = checkString(isrcCodeSerial_->get_text())) != NULL &&
	strlen(s) == 5) {
      strcat(buf, s);
    }

    if (strlen(buf) == 0) {
      if (t->isrcValid())
	tocEdit->setTrackIsrcCode(trackNr, NULL);
    }
    else if (strlen(buf) == 12) {
      if ((s = t->isrc()) == NULL || strcmp(s, buf) != 0)
	tocEdit->setTrackIsrcCode(trackNr, buf);
    }
  }

  exportCdText(tocEdit, trackNr);
}

void TrackInfoDialog::exportCdText(TocEdit *tocEdit, int trackNr)
{
  int l;
  const char *s;
  const Toc *toc = tocEdit->toc();

  const CdTextItem *item; 
  CdTextItem *newItem;

  for (l = 0; l < 8; l++) {
    // Title
    if ((s = checkString(cdTextPages_[l].title->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_TITLE, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_TITLE))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_TITLE, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_TITLE, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_TITLE, l, s);
    }

    delete newItem;


    // Performer
    if ((s = checkString(cdTextPages_[l].performer->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_PERFORMER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_PERFORMER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_PERFORMER, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_PERFORMER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_PERFORMER, l, s);
    }

    delete newItem;


    // Songwriter
    if ((s = checkString(cdTextPages_[l].songwriter->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_SONGWRITER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_SONGWRITER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_SONGWRITER, l,
			       NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_SONGWRITER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_SONGWRITER, l, s);
    }

    delete newItem;


    // Composer
    if ((s = checkString(cdTextPages_[l].composer->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_COMPOSER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_COMPOSER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_COMPOSER, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_COMPOSER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_COMPOSER, l, s);
    }

    delete newItem;


    // Arranger
    if ((s = checkString(cdTextPages_[l].arranger->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_ARRANGER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_ARRANGER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_ARRANGER, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_ARRANGER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_ARRANGER, l, s);
    }

    delete newItem;


    // Message
    if ((s = checkString(cdTextPages_[l].message->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_MESSAGE, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_MESSAGE))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_MESSAGE, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_MESSAGE, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_MESSAGE, l, s);
    }

    delete newItem;


    // Isrc
    if ((s = checkString(cdTextPages_[l].isrc->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_UPCEAN_ISRC, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::CDTEXT_UPCEAN_ISRC))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_UPCEAN_ISRC, l,
			       NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_UPCEAN_ISRC, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(trackNr, CdTextItem::CDTEXT_UPCEAN_ISRC, l, s);
    }

    delete newItem;
  }
}
