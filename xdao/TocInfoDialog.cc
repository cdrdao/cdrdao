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

#include "TocInfoDialog.h"

#include <gtkmm.h>
#include <gnome.h>

#include <stdio.h>
#include <stddef.h>
#include <ctype.h>

#include "TocEdit.h"
//#include "TocEditView.h"
#include "guiUpdate.h"
#include "Toc.h"
#include "CdTextItem.h"
#include "TextEdit.h"

#define MAX_CD_TEXT_LANGUAGE_CODES 22

struct LanguageCode {
  int code;
  const char *name;
};

static LanguageCode CD_TEXT_LANGUAGE_CODES[MAX_CD_TEXT_LANGUAGE_CODES] = {
  { -1, "Unknown" }, 
  { -1, "Undefined" },
  { 0x75, "Chinese" },
  { 0x06, "Czech" },
  { 0x07, "Danish" },
  { 0x1d, "Dutch" },
  { 0x09, "English" },
  { 0x27, "Finnish" },
  { 0x0f, "French" },
  { 0x08, "German" },
  { 0x70, "Greek" },
  { 0x1b, "Hungarian" },
  { 0x15, "Italian" },
  { 0x69, "Japanese" },
  { 0x65, "Korean" },
  { 0x1e, "Norwegian" },
  { 0x20, "Polish" },
  { 0x21, "Portuguese" },
  { 0x56, "Russian" },
  { 0x26, "Slovene" },
  { 0x0a, "Spanish" },
  { 0x28, "Swedish" }
};

#define MAX_CD_TEXT_GENRE_CODES 28

struct GenreCode {
  int code1;
  int code2;
  const char *name;
};

static GenreCode CD_TEXT_GENRE_CODES[MAX_CD_TEXT_GENRE_CODES] = {
  { -1, -1, "Unknown" },
  { -1, -1, "Not Used" },
  { 0x00, 0x01, "Not Defined" },
  { 0x00, 0x02, "Adult Contemporary" },
  { 0x00, 0x03, "Alternative Rock" },
  { 0x00, 0x04, "Children's Music" },
  { 0x00, 0x05, "Classical" },
  { 0x00, 0x07, "Country" },
  { 0x00, 0x08, "Dance" },
  { 0x00, 0x09, "Easy Listening" },
  { 0x00, 0x0a, "Erotic" },
  { 0x00, 0x0b, "Folk" },
  { 0x00, 0x0c, "Gospel" },
  { 0x00, 0x0d, "Hip Hop" },
  { 0x00, 0x0e, "Jazz" },
  { 0x00, 0x0f, "Latin" },
  { 0x00, 0x10, "Musical" },
  { 0x00, 0x11, "New Age" },
  { 0x00, 0x12, "Opera" },
  { 0x00, 0x13, "Operetta" },
  { 0x00, 0x14, "Pop Music" },
  { 0x00, 0x15, "RAP" },
  { 0x00, 0x16, "Reggae" },
  { 0x00, 0x17, "Rock Music" },
  { 0x00, 0x19, "Sound Effects" },
  { 0x00, 0x1a, "Sound Track" },
  { 0x00, 0x1b, "Spoken Word" },
  { 0x00, 0x1c, "World Music" }
};

TocInfoDialog::TocInfoDialog(Gtk::Window* parent)
{
  int i;
  Gtk::Label *label;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox, *vbox1;
  Gtk::Frame *frame;
  Gtk::Table *table;
  Gtk::Button *button;
  Gtk::VBox *contents = manage(new Gtk::VBox);

  tocEdit_ = NULL;
  active_ = false;
  selectedTocType_ = Toc::CD_DA;

  nofTracks_ = manage(new Gtk::Label("99"));
  nofTracks_->set_alignment(Gtk::ALIGN_LEFT);
  tocLength_ = manage(new Gtk::Label("100:00:00"));
  tocLength_->set_alignment(Gtk::ALIGN_LEFT);

  catalog_ = manage(new TextEdit("1234567890123"));
  catalog_->set_max_length(13);
  catalog_->lowerCase(0);
  catalog_->upperCase(0);
  catalog_->space(0);
  catalog_->digits(1);

  Gtk::Menu *menu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  mi = manage(new Gtk::MenuItem("CD-DA"));
  mi->signal_activate().
    connect(bind(mem_fun(*this, &TocInfoDialog::setSelectedTocType), Toc::CD_DA));
  menu->append(*mi);

  mi = manage(new Gtk::MenuItem("CD-ROM"));
  mi->signal_activate().
    connect(bind(mem_fun(*this, &TocInfoDialog::setSelectedTocType),Toc::CD_ROM));
  menu->append(*mi);

  mi = manage(new Gtk::MenuItem("CD-ROM-XA"));
  mi->signal_activate().
    connect(bind(mem_fun(*this, &TocInfoDialog::setSelectedTocType),
                 Toc::CD_ROM_XA));
  menu->append(*mi);

  mi = manage(new Gtk::MenuItem("CD-I"));
  mi->signal_activate().
    connect(bind(mem_fun(*this, &TocInfoDialog::setSelectedTocType), Toc::CD_I));
  menu->append(*mi);

  tocType_ = manage(new Gtk::OptionMenu);
  tocType_->set_menu(*menu);

  contents->set_spacing(10);

  // Summary frame (time & data)
  frame = manage(new Gtk::Frame(_(" Summary ")));
  table = manage(new Gtk::Table(2, 2, false));
  table->set_border_width(5);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  label = manage(new Gtk::Label(_("Tracks:")));
  table->attach(*label, 0, 1, 0, 1, Gtk::SHRINK);
  table->attach(*nofTracks_, 1, 2, 0, 1, Gtk::FILL | Gtk::SHRINK);
  label = manage(new Gtk::Label(_("Length:")));
  table->attach(*label, 0, 1, 1, 2, Gtk::SHRINK);
  table->attach(*tocLength_, 1, 2, 1, 2, Gtk::FILL | Gtk::SHRINK);
  frame->add(*table);
  contents->pack_start(*frame, false, false);
  
  // Sub-channel frame
  frame = manage(new Gtk::Frame(_(" Sub-Channel ")));
  table = manage(new Gtk::Table(2, 2, false));
  table->set_border_width(5);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  label = manage(new Gtk::Label(_("Toc Type: ")));
  table->attach(*label, 0, 1, 0, 1, Gtk::SHRINK);
  table->attach(*tocType_, 1, 2, 0, 1);
  label = manage(new Gtk::Label("UPC/EAN: "));
  table->attach(*label, 0, 1, 1, 2, Gtk::SHRINK);
  table->attach(*catalog_, 1, 2, 1, 2);
  frame->add(*table);
  contents->pack_start(*frame, Gtk::PACK_SHRINK);

  // CD-TEXT data
  frame = manage(new Gtk::Frame(" CD-TEXT "));

  Gtk::Notebook *notebook = manage(new Gtk::Notebook);

  for (i = 0; i < 8; i++) {
    vbox = createCdTextPage(i);
    notebook->pages().push_back(Gtk::Notebook_Helpers::TabElem(*vbox, *(cdTextPages_[i].label)));
  }

  vbox1 = manage(new Gtk::VBox);
  vbox1->pack_start(*notebook, false, false, 5);
  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*vbox1, true, true, 5);
  frame->add(*hbox);

  contents->pack_start(*frame, Gtk::PACK_SHRINK);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*contents, true, true, 10);
  get_vbox()->pack_start(*hbox, true, true, 10);

  Gtk::HButtonBox *bbox = manage(new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD));

  applyButton_ = manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::APPLY)));
  bbox->pack_start(*applyButton_);
  applyButton_->signal_clicked().connect(sigc::mem_fun(*this,&TocInfoDialog::applyAction));

  button = manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::CLOSE)));
  bbox->pack_start(*button);
  button->signal_clicked().connect(sigc::mem_fun(*this,&TocInfoDialog::closeAction));

  get_action_area()->pack_start(*bbox);
  show_all_children();

  set_title(_("Project Info"));
  set_transient_for(*parent);
}

TocInfoDialog::~TocInfoDialog()
{
}

void TocInfoDialog::start(TocEdit *view)
{
  present();
  active_ = true;
  update(UPD_ALL, view);
}

void TocInfoDialog::stop()
{
  hide();
  active_ = false;
}

void TocInfoDialog::setSelectedTocType(Toc::TocType t)
{
  selectedTocType_ = t;
}

void TocInfoDialog::setSelectedCDTextLanguage(BlockValue val)
{
  int i;
  int found;

  if (val.block < 0 || val.block >= 8)
    return;

  if (val.value == 0) {
    // cannot set to 'unknown', restore old setting
    if (cdTextPages_[val.block].selectedLanguage != 0) 
      cdTextPages_[val.block].language->set_history(cdTextPages_[val.block].selectedLanguage);

    return;
  }

  if (val.value != 1) {
    // check if same language is alread used
    found = 0;

    for (i = 0; i < 8; i++) {
      if (i != val.block && cdTextPages_[i].selectedLanguage == val.value) {
	found = 1;
	break;
      }
    }

    if (found ||
	(val.block > 0 && 
	 cdTextPages_[val.block - 1].selectedLanguage == 1)) {
      // reset to old value if the same language is already used or
      // if the language of the previous block is undefined
      cdTextPages_[val.block].language->set_history(cdTextPages_[val.block].selectedLanguage);
      return;
    }
  }
    
  cdTextPages_[val.block].selectedLanguage = val.value;
}

void TocInfoDialog::setSelectedCDTextGenre(BlockValue val)
{
  if (val.block < 0 || val.block >= 8)
    return;

  if (val.value == 0) {
    // cannot set to 'unknown', restore old setting
    if (cdTextPages_[val.block].selectedGenre != 0) 
      cdTextPages_[val.block].genre->set_history(cdTextPages_[val.block].selectedGenre);

    return;
  }

  cdTextPages_[val.block].selectedGenre = val.value;
}

void TocInfoDialog::createCdTextLanguageMenu(int n)
{
  BlockValue bval;
  int i;

  bval.block = n;

  Gtk::Menu *menu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  for (i = 0; i < MAX_CD_TEXT_LANGUAGE_CODES; i++) {
    bval.value = i;

    mi = manage(new Gtk::MenuItem(CD_TEXT_LANGUAGE_CODES[i].name));
    mi->signal_activate().connect(bind(mem_fun(*this, &TocInfoDialog::setSelectedCDTextLanguage), bval));
    mi->show();
    menu->append(*mi);
  }

  cdTextPages_[n].language = manage(new Gtk::OptionMenu);
  cdTextPages_[n].language->set_menu(*menu);
}

void TocInfoDialog::createCdTextGenreMenu(int n)
{
  BlockValue bval;
  int i;

  bval.block = n;

  Gtk::Menu *menu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  for (i = 0; i < MAX_CD_TEXT_GENRE_CODES; i++) {
    bval.value = i;

    mi = manage(new Gtk::MenuItem(CD_TEXT_GENRE_CODES[i].name));
    mi->signal_activate().connect(bind(mem_fun(*this, &TocInfoDialog::setSelectedCDTextGenre), bval));
    mi->show();
    menu->append(*mi);
  }

  cdTextPages_[n].genre = manage(new Gtk::OptionMenu);
  cdTextPages_[n].genre->set_menu(*menu);
}


Gtk::VBox *TocInfoDialog::createCdTextPage(int n)
{
  char buf[20];
  Gtk::Table *table = manage(new Gtk::Table(11, 2, false));
  Gtk::VBox *vbox1;
  Gtk::HBox *hbox;
  Gtk::Label *label;

  sprintf(buf, " %d ", n);
  cdTextPages_[n].label = manage(new Gtk::Label(buf));
  cdTextPages_[n].label->show();

  cdTextPages_[n].title = manage(new Gtk::Entry);
  cdTextPages_[n].performer = manage(new Gtk::Entry);
  cdTextPages_[n].songwriter = manage(new Gtk::Entry);
  cdTextPages_[n].composer = manage(new Gtk::Entry);
  cdTextPages_[n].arranger = manage(new Gtk::Entry);
  cdTextPages_[n].message = manage(new Gtk::Entry);
  cdTextPages_[n].catalog = manage(new Gtk::Entry);
  cdTextPages_[n].upcEan = manage(new Gtk::Entry);
  cdTextPages_[n].genreInfo = manage(new Gtk::Entry);

  createCdTextLanguageMenu(n);
  createCdTextGenreMenu(n);

  table->set_row_spacings(5);
  table->set_col_spacings(5);
  table->show();

  label = manage(new Gtk::Label(_("Language:")));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 0, 1, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].language), 1, 2, 0, 1);
  cdTextPages_[n].language->show();

  label = manage(new Gtk::Label(_("Title:")));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 1, 2, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].title), 1, 2, 1, 2);
  cdTextPages_[n].title->show();

  label = manage(new Gtk::Label(_("Performer:")));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 2, 3, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].performer), 1, 2, 2, 3);
  cdTextPages_[n].performer->show();

  label = new Gtk::Label(_("Songwriter:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 3, 4, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].songwriter), 1, 2, 3, 4);
  cdTextPages_[n].songwriter->show();

  label = new Gtk::Label(_("Composer:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 4, 5, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].composer), 1, 2, 4, 5);
  cdTextPages_[n].composer->show();

  label = new Gtk::Label(_("Arranger:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 5, 6, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].arranger), 1, 2, 5, 6);
  cdTextPages_[n].arranger->show();

  label = new Gtk::Label(_("Message:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 6, 7, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].message), 1, 2, 6, 7);
  cdTextPages_[n].message->show();

  label = new Gtk::Label(_("Catalog:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 7, 8, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].catalog), 1, 2, 7, 8);
  cdTextPages_[n].catalog->show();

  label = new Gtk::Label("UPC/EAN:");
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 8, 9, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].upcEan), 1, 2, 8, 9);
  cdTextPages_[n].upcEan->show();

  label = new Gtk::Label(_("Genre:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 9, 10, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].genre), 1, 2, 9, 10);
  cdTextPages_[n].genre->show();

  label = new Gtk::Label(_("Genre Info:"));
  hbox = manage(new Gtk::HBox);
  hbox->pack_end(*label, Gtk::PACK_SHRINK);
  table->attach(*hbox, 0, 1, 10, 11, Gtk::FILL);
  hbox->show();
  label->show();
  table->attach(*(cdTextPages_[n].genreInfo), 1, 2, 10, 11);
  cdTextPages_[n].genreInfo->show();

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*table, true, true, 3);
  table->show();

  vbox1 = manage(new Gtk::VBox);
  vbox1->pack_start(*hbox, true, true, 3);
  hbox->show();

  vbox1->show();
  
  return vbox1;
}

bool TocInfoDialog::on_delete_event(GdkEventAny*)
{
  stop();
  return 1;
}

void TocInfoDialog::closeAction()
{
  stop();
}


void TocInfoDialog::clear()
{
  nofTracks_->set_text("");
  tocLength_->set_text("");

  tocType_->set_history(0);
  selectedTocType_ = Toc::CD_DA;

  catalog_->set_text("");
  catalog_->set_editable(false);

  clearCdText();
}

void TocInfoDialog::update(unsigned long level, TocEdit *view)
{
  const Toc *toc;

  if (!active_)
    return;

  tocEdit_ = view;

  if (view == NULL) {
    clear();
    return;
  }

  std::string s(view->filename());
  s += " - ";
  s += APP_NAME;
  if (view->tocDirty())
    s += "(*)";
  set_title(s);

  if (level & UPD_TOC_DATA) {
    toc = tocEdit_->toc();
    importData(toc);
  }

  if (level & UPD_EDITABLE_STATE) {
    applyButton_->set_sensitive(tocEdit_->editable() ? true : false);
  }
}


void TocInfoDialog::clearCdText()
{
  int l;

  for (l = 0; l < 8; l++) {
    cdTextPages_[l].title->set_text("");
    cdTextPages_[l].title->set_editable(false);

    cdTextPages_[l].performer->set_text("");
    cdTextPages_[l].performer->set_editable(false);

    cdTextPages_[l].songwriter->set_text("");
    cdTextPages_[l].songwriter->set_editable(false);

    cdTextPages_[l].composer->set_text("");
    cdTextPages_[l].composer->set_editable(false);

    cdTextPages_[l].arranger->set_text("");
    cdTextPages_[l].arranger->set_editable(false);

    cdTextPages_[l].message->set_text("");
    cdTextPages_[l].message->set_editable(false);

    cdTextPages_[l].catalog->set_text("");
    cdTextPages_[l].catalog->set_editable(false);

    cdTextPages_[l].upcEan->set_text("");
    cdTextPages_[l].upcEan->set_editable(false);

    cdTextPages_[l].language->set_history(1);
    cdTextPages_[l].selectedLanguage = 1;

    cdTextPages_[l].genre->set_history(1);
    cdTextPages_[l].selectedGenre = 1;
  }
}


void TocInfoDialog::applyAction()
{
  if (tocEdit_ == NULL || !tocEdit_->editable())
    return;

  exportData(tocEdit_);

  guiUpdate(UPD_TOC_DATA);
}

const char *TocInfoDialog::checkString(const std::string &str)
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

int TocInfoDialog::getCdTextLanguageIndex(int code)
{
  int i;

  if (code < 0)
    return 1; // undefined

  for (i = 2; i < MAX_CD_TEXT_LANGUAGE_CODES; i++) {
    if (CD_TEXT_LANGUAGE_CODES[i].code == code)
      return i;
  }

  return 0; // unknown
}

int TocInfoDialog::getCdTextGenreIndex(int code1, int code2)
{
  int i;

  for (i = 2; i < MAX_CD_TEXT_GENRE_CODES; i++) {
    if (CD_TEXT_GENRE_CODES[i].code1 == code1 &&
	CD_TEXT_GENRE_CODES[i].code2 == code2)
      return i;
  }

  return 0;
}

void TocInfoDialog::importCdText(const Toc *toc)
{
  int l;
  const CdTextItem *item; 

  for (l = 0; l < 8; l++) {
    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_TITLE))
	!= NULL) 
      cdTextPages_[l].title->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].title->set_text("");
    cdTextPages_[l].title->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_PERFORMER))
	!= NULL) 
      cdTextPages_[l].performer->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].performer->set_text("");
    cdTextPages_[l].performer->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_SONGWRITER))
	!= NULL) 
      cdTextPages_[l].songwriter->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].songwriter->set_text("");
    cdTextPages_[l].songwriter->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_COMPOSER))
	!= NULL) 
      cdTextPages_[l].composer->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].composer->set_text("");
    cdTextPages_[l].composer->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_ARRANGER))
	!= NULL) 
      cdTextPages_[l].arranger->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].arranger->set_text("");
    cdTextPages_[l].arranger->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_MESSAGE))
	!= NULL) 
      cdTextPages_[l].message->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].message->set_text("");
    cdTextPages_[l].message->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_DISK_ID))
	!= NULL) 
      cdTextPages_[l].catalog->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].catalog->set_text("");
    cdTextPages_[l].catalog->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_UPCEAN_ISRC))
	!= NULL) 
      cdTextPages_[l].upcEan->set_text((const char*)(item->data()));
    else
      cdTextPages_[l].upcEan->set_text("");
    cdTextPages_[l].upcEan->set_editable(true);

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_GENRE)) != NULL) {
      if (item->dataLen() >= 2) {
	cdTextPages_[l].selectedGenre = getCdTextGenreIndex(item->data()[0],
							    item->data()[1]);
      }
      else {
	cdTextPages_[l].selectedGenre = 0; // Unknwon
      }

      if (item->dataLen() > 2) {
	// Copy the supplementary genre information from the CD-TEXT item.
	// Carefully handle the case that the terminating 0 is missing.
	int len = item->dataLen() - 2 + 1;
	char *s = new char[len];
	memcpy(s, item->data() + 2, len - 1);
	s[len - 1] = 0;

	cdTextPages_[l].genreInfo->set_text(s);

	delete[] s;
      }
      else {
	cdTextPages_[l].genreInfo->set_text("");
      }
    }
    else {
      cdTextPages_[l].selectedGenre = 1; // not used
      cdTextPages_[l].genreInfo->set_text("");
    }
    cdTextPages_[l].genre->set_history(cdTextPages_[l].selectedGenre);

    cdTextPages_[l].selectedLanguage = getCdTextLanguageIndex(toc->cdTextLanguage(l));
    cdTextPages_[l].language->set_history(cdTextPages_[l].selectedLanguage);
  }
}

void TocInfoDialog::importData(const Toc *toc)
{
  char buf[50];
  int i;

  sprintf(buf, "%3d:%02d:%02d", toc->length().min(), toc->length().sec(),
	  toc->length().frac());
  tocLength_->set_text(buf);

  sprintf(buf, "%3d", toc->nofTracks());
  nofTracks_->set_text(buf);

  if (toc->catalogValid()) {
    for (i = 0; i < 13; i++)
      buf[i] = toc->catalog(i) + '0';

    buf[13] = 0;

    catalog_->set_text(buf);
  }
  else {
    catalog_->set_text("");
  }

  catalog_->set_editable(true);

  switch (toc->tocType()) {
  case Toc::CD_DA:
    tocType_->set_history(0);
    break;
  case Toc::CD_ROM:
    tocType_->set_history(1);
    break;
  case Toc::CD_ROM_XA:
    tocType_->set_history(2);
    break;
  case Toc::CD_I:
    tocType_->set_history(3);
    break;
  }
  selectedTocType_ = toc->tocType();

  importCdText(toc);
}

void TocInfoDialog::exportData(TocEdit *tocEdit)
{
  const char *s, *s1;
  Toc *toc = tocEdit->toc();
  
  if (toc->tocType() != selectedTocType_) {
    tocEdit->setTocType(selectedTocType_);
  }

  s = checkString(catalog_->get_text());

  if (s == NULL) {
    if (toc->catalogValid())
      tocEdit->setCatalogNumber(NULL);
  }
  else if (strlen(s) == 13) {
    if ((s1 = toc->catalog()) == NULL || strcmp(s1, s) != 0)
      tocEdit->setCatalogNumber(s);
  }

  exportCdText(tocEdit);
}

void TocInfoDialog::exportCdText(TocEdit *tocEdit)
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

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_TITLE))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_TITLE, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_TITLE, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_TITLE, l, s);
    }

    delete newItem;


    // Performer
    if ((s = checkString(cdTextPages_[l].performer->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_PERFORMER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_PERFORMER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_PERFORMER, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_PERFORMER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_PERFORMER, l, s);
    }

    delete newItem;


    // Songwriter
    if ((s = checkString(cdTextPages_[l].songwriter->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_SONGWRITER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_SONGWRITER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_SONGWRITER, l,
			       NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_SONGWRITER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_SONGWRITER, l, s);
    }

    delete newItem;


    // Composer
    if ((s = checkString(cdTextPages_[l].composer->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_COMPOSER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_COMPOSER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_COMPOSER, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_COMPOSER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_COMPOSER, l, s);
    }

    delete newItem;


    // Arranger
    if ((s = checkString(cdTextPages_[l].arranger->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_ARRANGER, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_ARRANGER))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_ARRANGER, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_ARRANGER, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_ARRANGER, l, s);
    }

    delete newItem;


    // Message
    if ((s = checkString(cdTextPages_[l].message->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_MESSAGE, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_MESSAGE))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_MESSAGE, l, NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_MESSAGE, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_MESSAGE, l, s);
    }

    delete newItem;

    // Catalog
    if ((s = checkString(cdTextPages_[l].catalog->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_DISK_ID, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_DISK_ID))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_DISK_ID, l,
			       NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_DISK_ID, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_DISK_ID, l, s);
    }

    delete newItem;

    // Upc/Ean
    if ((s = checkString(cdTextPages_[l].upcEan->get_text())) != NULL)
      newItem = new CdTextItem(CdTextItem::CDTEXT_UPCEAN_ISRC, l, s);
    else
      newItem = NULL;

    if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_UPCEAN_ISRC))
	!= NULL) {
      if (newItem == NULL)
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_UPCEAN_ISRC, l,
			       NULL);
      else if (*newItem != *item) 
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_UPCEAN_ISRC, l, s);
    }
    else if (newItem != NULL) {
	tocEdit->setCdTextItem(0, CdTextItem::CDTEXT_UPCEAN_ISRC, l, s);
    }

    delete newItem;

    // Genre
    if (cdTextPages_[l].selectedGenre != 0) {
      int code1 = CD_TEXT_GENRE_CODES[cdTextPages_[l].selectedGenre].code1;
      int code2 = CD_TEXT_GENRE_CODES[cdTextPages_[l].selectedGenre].code2;
      s = checkString(cdTextPages_[l].genreInfo->get_text());

      if (cdTextPages_[l].selectedGenre > 1)
	newItem = new CdTextItem(l, code1, code2, s);
      else
	newItem = NULL;

      if ((item = toc->getCdTextItem(0, l, CdTextItem::CDTEXT_GENRE))
	  != NULL) {
	if (newItem == NULL)
	  tocEdit->setCdTextGenreItem(l, -1, -1, NULL);
	else if (*newItem != *item) 
	  tocEdit->setCdTextGenreItem(l, code1, code2, s);
      }
      else if (newItem != NULL) {
	tocEdit->setCdTextGenreItem(l, code1, code2, s);
      }

      delete newItem;
    }

    // language
    if (cdTextPages_[l].selectedLanguage != 0) {
      int langCode = CD_TEXT_LANGUAGE_CODES[cdTextPages_[l].selectedLanguage].code;

      if (langCode != toc->cdTextLanguage(l)) 
	tocEdit->setCdTextLanguage(l, langCode);
    }
  }
}
