/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998,1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: CdTextTable.cc,v $
 * Revision 1.1  2000/04/23 09:02:04  andreasm
 * Table entry dialog for CD-TEXT title and performer data.
 *
 * Revision 1.1  1999/12/15 20:44:50  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: CdTextTable.cc,v 1.1 2000/04/23 09:02:04 andreasm Exp $";

#include "CdTextTable.h"

#include <stddef.h>
#include <string.h>

#include "TocEdit.h"
#include "Toc.h"
#include "util.h"

CdTextTable::CdTextTable(TocEdit *tocEdit, int language)
  : adjust_(0.0, 0.0, 0.0)
{
  int i;
  int n = tocEdit->toc()->nofTracks();
  Gtk::VBox *contents = manage(new Gtk::VBox);
  Gtk::Table *table = manage(new Gtk::Table(n + 3, 3, FALSE));
  char buf[50];

  tocEdit_ = tocEdit;
  language_ = language;

  if (n > 0)
    tracks_ = new TableEntry[n];
  else
    tracks_ = NULL;

  table->set_row_spacings(5);
  table->set_col_spacings(5);

  {
    Gtk::Label *label1 = manage(new Gtk::Label("Performer"));
    Gtk::Label *label2 = manage(new Gtk::Label("Title"));

    table->attach(*label1, 1, 2, 0, 1);
    label1->show();
    table->attach(*label2, 2, 3, 0, 1);
    label2->show();
  }
  
  {
    Gtk::HBox *hbox = manage(new Gtk::HBox);
    Gtk::Label *label = manage(new Gtk::Label("Album"));
    performer_ = new Gtk::Entry;
    title_ = new Gtk::Entry;

    hbox->pack_end(*label, FALSE);
    label->show();
    table->attach(*hbox, 0, 1, 1, 2, GTK_FILL);
    hbox->show();
    table->attach(*title_, 2, 3, 1, 2);
    title_->show();
    table->attach(*performer_, 1, 2, 1, 2);
    performer_->show();
  }

  {
    performerButton_ = new Gtk::CheckButton("Enable Perfomer Entries");
    performerButton_->set_active(TRUE);
    //performerButton_->toggled.connect(slot(this, &CdTextTable::activatePerformerAction));

    Gtk::HBox *hbox = manage(new Gtk::HBox);


    hbox->pack_start(*performerButton_, FALSE);
    performerButton_->show();

    table->attach(*hbox, 1, 2, 2, 3);
    hbox->show();
  }

  for (i = 0; i < n; i++) {
    sprintf(buf, "Track %02d", i + 1);

    Gtk::HBox *hbox(new Gtk::HBox);
    Gtk::Label *label(new Gtk::Label(buf));
    tracks_[i].performer = new Gtk::Entry;
    tracks_[i].title = new Gtk::Entry;

    hbox->pack_end(*label, FALSE);
    label->show();

    table->attach(*hbox, 0, 1, i + 3, i + 4, GTK_FILL);
    hbox->show();
    table->attach(*(tracks_[i].title), 2, 3, i + 3, i + 4);
    tracks_[i].title->show();
    table->attach(*(tracks_[i].performer), 1, 2, i + 3, i + 4);
    tracks_[i].performer->show();
  }

  {
    Gtk::HBox *hbox1 = manage(new Gtk::HBox);
    Gtk::VBox *vbox1 = manage(new Gtk::VBox);

    hbox1->pack_start(*table, TRUE, TRUE, 5);
    table->show();
    vbox1->pack_start(*hbox1, FALSE, FALSE, 5);
    hbox1->show();

    Gtk::Viewport *vport = manage(new Gtk::Viewport);
    vport->set_vadjustment(adjust_);
    vport->add(*vbox1);
    vbox1->show();

    Gtk::HBox  *hbox2 = manage(new Gtk::HBox);
    Gtk::VScrollbar *vbar = manage(new Gtk::VScrollbar(adjust_));

    hbox2->set_spacing(2);
    hbox2->pack_start(*vport);
    vport->show();
    hbox2->pack_start(*vbar, FALSE);
    vbar->show();
    
    contents->pack_start(*hbox2);
    hbox2->show();
  }


  Gtk::HBox *chbox = manage(new Gtk::HBox);
  
  chbox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();
  get_vbox()->pack_start(*chbox, TRUE, TRUE, 10);
  chbox->show();

  get_vbox()->show();

  Gtk::HButtonBox *bbox = manage(new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD));

  Gtk::Button *okButton = manage(new Gtk::Button(string(" Ok ")));
  //okButton->clicked.connect(slot(this, &CdTextTable::okAction));

  bbox->pack_start(*okButton);
  okButton->show();

  Gtk::Button *fillButton = manage(new Gtk::Button(string(" Fill Performer ")));
  //fillButton->clicked.connect(slot(this, &CdTextTable::fillPerformerAction));

  bbox->pack_start(*fillButton);
  fillButton->show();

  Gtk::Button *cancelButton = manage(new Gtk::Button(string(" Cancel ")));
  //cancelButton->clicked.connect(slot(this, &CdTextTable::cancelAction));

  bbox->pack_start(*cancelButton);
  cancelButton->show();

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();

  set_title(string("CD TEXT"));
  set_usize(0, 400);
}

CdTextTable::~CdTextTable()
{
  delete[] tracks_;
  tracks_ = NULL;
}

int CdTextTable::run()
{
  Gtk::Main *app = Gtk::Main::instance();

  importData();

  done_ = 0;

  show();

  app->grab_add(*this);

  do {
    app->iteration();
  }  while (done_ == 0);

  app->grab_remove(*this);

  hide();

  return (done_ == 1 ? 0 : 1);
}

gint CdTextTable::delete_event_impl(GdkEventAny*)
{
  cancelAction();
  return 1;
}

void CdTextTable::cancelAction()
{
  done_ = 1;
}

void CdTextTable::okAction()
{
  done_ = 2;

  if (tocEdit_ == NULL || !tocEdit_->editable())
    return;

  exportData();
}

void CdTextTable::fillPerformerAction()
{
  const char *s = checkString(performer_->get_text());
  int i;
  int n = tocEdit_->toc()->nofTracks();

  if (s == NULL)
    return;

  char *performer = strdupCC(s);

  for (i = 0; i < n; i++) {
    if (checkString(tracks_[i].performer->get_text()) == NULL)
      tracks_[i].performer->set_text(performer);
  }

  delete[] performer;
}

void CdTextTable::activatePerformerAction()
{
  int i;
  int n = tocEdit_->toc()->nofTracks();
  int val = performerButton_->get_active();

  for (i = 0; i < n; i++) {
    tracks_[i].performer->set_sensitive(val);
  }
}

void CdTextTable::importData()
{
  const CdTextItem *item; 
  const Toc *toc = tocEdit_->toc();
  int i;
  int n = tocEdit_->toc()->nofTracks();

  if ((item = toc->getCdTextItem(0, language_, CdTextItem::CDTEXT_TITLE))
      != NULL)
    title_->set_text((const char*)item->data());
  else
    title_->set_text("");

  if ((item = toc->getCdTextItem(0, language_, CdTextItem::CDTEXT_PERFORMER))
      != NULL)
    performer_->set_text((const char*)item->data());
  else
    performer_->set_text("");

  for (i = 0; i < n; i++) {
    if ((item = toc->getCdTextItem(i + 1, language_, CdTextItem::CDTEXT_TITLE))
	!= NULL)
      tracks_[i].title->set_text((const char*)item->data());
    else
      tracks_[i].title->set_text("");

    if ((item = toc->getCdTextItem(i + 1, language_, CdTextItem::CDTEXT_PERFORMER))
	!= NULL)
      tracks_[i].performer->set_text((const char*)item->data());
    else
      tracks_[i].performer->set_text("");
  }
}

void CdTextTable::exportData()
{
  int i;
  int n = tocEdit_->toc()->nofTracks();

  setCdTextItem(CdTextItem::CDTEXT_TITLE, 0, checkString(title_->get_text()));
  setCdTextItem(CdTextItem::CDTEXT_PERFORMER, 0,
		checkString(performer_->get_text()));

  for (i = 0; i < n; i++) {
    setCdTextItem(CdTextItem::CDTEXT_TITLE, i + 1,
		  checkString(tracks_[i].title->get_text()));
    setCdTextItem(CdTextItem::CDTEXT_PERFORMER, i + 1,
		  checkString(tracks_[i].performer->get_text()));
  }
}

void CdTextTable::setCdTextItem(CdTextItem::PackType type, int trackNr,
				const char *s)
{
  const CdTextItem *item; 
  const Toc *toc = tocEdit_->toc();
  CdTextItem *newItem;
  
  if (s != NULL)
    newItem = new CdTextItem(type, language_, s);
  else
    newItem = NULL;

  if ((item = toc->getCdTextItem(trackNr, language_, type)) != NULL) {
    if (newItem == NULL)
      tocEdit_->setCdTextItem(trackNr, type, language_, NULL);
    else if (*newItem != *item) 
      tocEdit_->setCdTextItem(trackNr, type, language_, s);
  }
  else if (newItem != NULL) {
    tocEdit_->setCdTextItem(trackNr, type, language_, s);
  }

  delete newItem;
}

const char *CdTextTable::checkString(const string &str)
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

