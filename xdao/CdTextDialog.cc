/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <mueller@daneb.ping.de>
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

#include "CdTextDialog.h"

#include <gnome--.h>

#include <stddef.h>
#include <string.h>

#include "TocEdit.h"
#include "TocEditView.h"
#include "Toc.h"
#include "util.h"

#include "guiUpdate.h"

CdTextDialog::CdTextDialog()
{
  int i;
  Gtk::VBox *contents = new Gtk::VBox;
  char buf[20];

  active_ = 0;
  tocEditView_ = NULL;
  trackEntries_ = 0;

  languages_ = new Gtk::Notebook;

  for (i = 0; i < 8; i++) {
    page_[i].table = new Gtk::Table(3, 3, FALSE);
    page_[i].table->set_row_spacings(5);
    page_[i].table->set_col_spacings(5);

    page_[i].performer = new Gtk::Entry;

    page_[i].title = new Gtk::Entry;
    
    page_[i].tabLabel = new Gtk::Label("");

    page_[i].performerButton = new Gtk::CheckButton("Enable Perfomer Entries");
    page_[i].performerButton->set_active(TRUE);
    page_[i].performerButton->toggled.connect(bind(slot(this, &CdTextDialog::activatePerformerAction), i));

    page_[i].adjust = new Gtk::Adjustment(0.0, 0.0, 0.0);

    page_[i].tracks = NULL;

    page_[i].table->attach(*(new Gtk::Label("Performer")), 1, 2, 0, 1);
    page_[i].table->attach(*(new Gtk::Label("Title")), 2, 3, 0, 1);

    {
      Gtk::HBox *hbox = new Gtk::HBox;
      hbox->pack_end(*(new Gtk::Label("Album")));

      page_[i].table->attach(*hbox, 0, 1, 1, 2, GTK_FILL);
      page_[i].table->attach(*(page_[i].title), 2, 3, 1, 2);
      page_[i].table->attach(*(page_[i].performer), 1, 2, 1, 2);
    }
    
    {
      Gtk::HBox *hbox = new Gtk::HBox;

      hbox->pack_start(*(page_[i].performerButton));
      page_[i].table->attach(*hbox, 1, 2, 2, 3);
    }

    {
      Gtk::HBox *hbox1 = new Gtk::HBox;
      Gtk::VBox *vbox1 = new Gtk::VBox;

      hbox1->pack_start(*(page_[i].table), TRUE, TRUE, 5);
      vbox1->pack_start(*hbox1, FALSE, FALSE, 5);

      Gtk::Viewport *vport = new Gtk::Viewport;
      vport->set_vadjustment(*(page_[i].adjust));
      vport->add(*vbox1);

      Gtk::HBox  *hbox2 = new Gtk::HBox;
      Gtk::VScrollbar *vbar = new Gtk::VScrollbar(*(page_[i].adjust));

      hbox2->set_spacing(2);
      hbox2->pack_start(*vport);
      hbox2->pack_start(*vbar, FALSE);

      sprintf(buf, "%d", i);
      languages_->pages().push_back(Gtk::Notebook_Helpers::TabElem(*hbox2, *(page_[i].tabLabel)));
    }
  }

  contents->pack_start(*languages_);

  {
    Gtk::HBox *hbox = new Gtk::HBox;

    hbox->pack_start(*contents, TRUE, TRUE, 10);
    get_vbox()->pack_start(*hbox, TRUE, TRUE, 10);
  }

  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);
  
  applyButton_ = new Gnome::StockButton(GNOME_STOCK_BUTTON_APPLY);
  bbox->pack_start(*applyButton_);
  applyButton_->clicked.connect(slot(this, &CdTextDialog::applyAction));
  
  Gtk::Button *fillButton = new Gtk::Button(" Fill Performer ");
  bbox->pack_start(*fillButton);
  fillButton->clicked.connect(slot(this, &CdTextDialog::fillPerformerAction));

  Gtk::Button *cancelButton = new Gnome::StockButton(GNOME_STOCK_BUTTON_CLOSE);
  bbox->pack_start(*cancelButton);
  cancelButton->clicked.connect(slot(this, &CdTextDialog::stop));

  get_action_area()->pack_start(*bbox);

  set_title(string("CD-TEXT Entry"));
  set_usize(0, 400);
}

CdTextDialog::~CdTextDialog()
{
}

gint CdTextDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void CdTextDialog::updateTabLabels()
{
  const Toc *toc = tocEditView_->tocEdit()->toc();
  int l;

  for (l = 0; l < 8; l++) {
    const char *s = CdTextContainer::languageName(toc->cdTextLanguage(l));

    if (page_[l].tabLabel->get() != s)
      page_[l].tabLabel->set(s);
  }
}

void CdTextDialog::adjustTableEntries(int n)
{
  int i, l;
  char buf[20];

  if (trackEntries_ == n)
    return;

  for (l = 0; l < 8; l++) {
    if (n < trackEntries_) {
      page_[l].table->resize(3 + n, 3);

      for (i = n; i < trackEntries_; i++) {
	delete page_[l].tracks[i].performer;
	delete page_[l].tracks[i].title;
	delete page_[l].tracks[i].hbox;
	delete page_[l].tracks[i].label;
      }
    }
    else {
      int performerActive = page_[l].performerButton->get_active();

      TableEntry *newTracks = new TableEntry[n];

      for (i = 0; i < trackEntries_; i++)
	newTracks[i] = page_[l].tracks[i];

      delete[] page_[l].tracks;
      page_[l].tracks = newTracks;

      page_[l].table->resize(3 + n, 3);

      for (i = trackEntries_; i < n; i++) {
	sprintf(buf, "Track %02d", i + 1);
	
	page_[l].tracks[i].performer = new Gtk::Entry;
	page_[l].tracks[i].performer->set_sensitive(performerActive);
	page_[l].tracks[i].title = new Gtk::Entry;
	page_[l].tracks[i].label = new Gtk::Label(buf);
	page_[l].tracks[i].hbox = new Gtk::HBox;

	page_[l].tracks[i].hbox->pack_end(*(page_[l].tracks[i].label), FALSE);

	page_[l].table->attach(*(page_[l].tracks[i].hbox),
			       0, 1, i + 3, i + 4, GTK_FILL);
	page_[l].table->attach(*(page_[l].tracks[i].title),
			       2, 3, i + 3, i + 4);
	page_[l].table->attach(*(page_[l].tracks[i].performer),
			       1, 2, i + 3, i + 4);
      }

      page_[l].table->show_all();
    }
  }

  trackEntries_ = n;
}

void CdTextDialog::update(unsigned long level, TocEditView *view)
{
  if (view != tocEditView_) {
    tocEditView_ = view;
    level = UPD_ALL;
  }

  if (level & UPD_TOC_DATA) {
    updateTabLabels();
  }
  
  if ((level & UPD_TOC_DATA) ||
      (level & UPD_TRACK_DATA)) {
    importData();
  }

  if (level & UPD_EDITABLE_STATE) {
    applyButton_->set_sensitive(view->tocEdit()->editable() ? TRUE : FALSE);
  }
}

void CdTextDialog::start(TocEditView *view)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_ALL, view);
  show_all();
}

void CdTextDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void CdTextDialog::applyAction()
{
  if (tocEditView_ == NULL || !tocEditView_->tocEdit()->editable())
    return;

  exportData();

  guiUpdate();
}

void CdTextDialog::fillPerformerAction()
{
  int l = languages_->get_current_page_num();

  if (l >= 0 && l <= 7) {
    int i;
    const char *s = checkString(page_[l].performer->get_text());

    if (s == NULL)
      return;

    char *performer = strdupCC(s);

    for (i = 0; i < trackEntries_; i++) {
      if (checkString(page_[l].tracks[i].performer->get_text()) == NULL)
	page_[l].tracks[i].performer->set_text(performer);
    }

    delete[] performer;
  }
}

void CdTextDialog::activatePerformerAction(int l)
{
  int i;
  int val = page_[l].performerButton->get_active();

  for (i = 0; i < trackEntries_; i++) {
    page_[l].tracks[i].performer->set_sensitive(val);
  }
}

void CdTextDialog::importData()
{
  const CdTextItem *item; 
  const Toc *toc = tocEditView_->tocEdit()->toc();
  int i, l;
  int n = toc->nofTracks();

  adjustTableEntries(n);

  for (l = 0; l < 8; l++) {
    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_TITLE)) != NULL)
      page_[l].title->set_text((const char*)item->data());
    else
      page_[l].title->set_text("");

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_PERFORMER))
	!= NULL)
      page_[l].performer->set_text((const char*)item->data());
    else
      page_[l].performer->set_text("");

    for (i = 0; i < n; i++) {
      if ((item = toc->getCdTextItem(i + 1, l, CdTextItem::CDTEXT_TITLE))
	  != NULL)
	page_[l].tracks[i].title->set_text((const char*)item->data());
      else
	page_[l].tracks[i].title->set_text("");

      if ((item = toc->getCdTextItem(i + 1, l, CdTextItem::CDTEXT_PERFORMER))
	  != NULL)
	page_[l].tracks[i].performer->set_text((const char*)item->data());
      else
	page_[l].tracks[i].performer->set_text("");
    }
  }
}

void CdTextDialog::exportData()
{
  int i, l;

  for (l = 0; l < 8; l++) {
    setCdTextItem(CdTextItem::CDTEXT_TITLE, 0, l,
		  checkString(page_[l].title->get_text()));
    setCdTextItem(CdTextItem::CDTEXT_PERFORMER, 0, l,
		  checkString(page_[l].performer->get_text()));

    for (i = 0; i < trackEntries_; i++) {
      setCdTextItem(CdTextItem::CDTEXT_TITLE, i + 1, l,
		    checkString(page_[l].tracks[i].title->get_text()));
      setCdTextItem(CdTextItem::CDTEXT_PERFORMER, i + 1, l,
		    checkString(page_[l].tracks[i].performer->get_text()));
    }
  }
}

void CdTextDialog::setCdTextItem(CdTextItem::PackType type, int trackNr,
				 int l, const char *s)
{
  const CdTextItem *item; 
  TocEdit *tocEdit = tocEditView_->tocEdit();
  const Toc *toc = tocEdit->toc();
  CdTextItem *newItem;
  
  if (s != NULL)
    newItem = new CdTextItem(type, l, s);
  else
    newItem = NULL;

  if ((item = toc->getCdTextItem(trackNr, l, type)) != NULL) {
    if (newItem == NULL)
      tocEdit->setCdTextItem(trackNr, type, l, NULL);
    else if (*newItem != *item) 
      tocEdit->setCdTextItem(trackNr, type, l, s);
  }
  else if (newItem != NULL) {
    tocEdit->setCdTextItem(trackNr, type, l, s);
  }

  delete newItem;
}

const char *CdTextDialog::checkString(const string &str)
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

