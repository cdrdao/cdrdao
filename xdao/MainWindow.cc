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
 * $Log: MainWindow.cc,v $
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:32  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.6  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.5  1999/03/06 13:55:18  mueller
 * Adapted to Gtk-- version 0.99.1
 *
 * Revision 1.4  1999/02/28 10:59:07  mueller
 * Adapted to changes in 'trackdb'.
 *
 * Revision 1.3  1999/01/30 19:45:43  mueller
 * Fixes for compilation with Gtk-- 0.11.1.
 *
 * Revision 1.1  1998/11/20 18:54:34  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: MainWindow.cc,v 1.2 2000/02/20 23:34:54 llanero Exp $";

#include <stdio.h>
#include <fstream.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <strstream.h>

#include "xcdrdao.h"
#include "MainWindow.h"
#include "guiUpdate.h"
#include "SampleDisplay.h"
#include "AddSilenceDialog.h"
#include "AddFileDialog.h"
#include "TrackInfoDialog.h"
#include "TocInfoDialog.h"
#include "DeviceConfDialog.h"
#include "ExtractDialog.h"
#include "RecordDialog.h"
#include "SoundIF.h"
#include "TocEdit.h"
#include "MessageBox.h"

#include "Toc.h"
#include "TrackData.h"
#include "util.h"

MainWindow::MainWindow(TocEdit *tedit) : vbox_(false, 5), tocReader(NULL)
{
  tocEdit_ = tedit;

  playing_ = 0;
  playBurst_ = 588 * 5;
  soundInterface_ = new SoundIF;
  playBuffer_ = new Sample[playBurst_];
  soundInterface_ = NULL;

  fileSelector_ = new Gtk::FileSelection("");
  fileSelector_->complete(string("*.toc"));

  createMenuBar();

  set_usize(600,400);

//llanero: will use GNOME
//  vbox_.pack_start(*menuBar_, FALSE, FALSE);
//  menuBar_->show();

  sampleDisplay_ = new SampleDisplay;
  sampleDisplay_->setTocEdit(tocEdit_);

  vbox_.pack_start(*sampleDisplay_, TRUE, TRUE);
  sampleDisplay_->show();

  Gtk::HScrollbar *scrollBar =
    new Gtk::HScrollbar(*(sampleDisplay_->getAdjustment()));
  vbox_.pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();
  
  Gtk::Label *label;
  Gtk::HBox *selectionInfoBox = new Gtk::HBox;

  markerPos_ = new Gtk::Entry;
  markerPos_->set_editable(true);
//llanero  connect_to_method(markerPos_->activate, this, &MainWindow::markerSet);
  markerPos_->activate.connect(slot(this, &MainWindow::markerSet));

  cursorPos_ = new Gtk::Entry;
  cursorPos_->set_editable(false);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
//llanero  connect_to_method(selectionStartPos_->activate, this,
//		    &MainWindow::selectionSet);
  selectionStartPos_->activate.connect(slot(this, &MainWindow::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
//llanero  connect_to_method(selectionEndPos_->activate, this,
//		    &MainWindow::selectionSet);
  selectionEndPos_->activate.connect(slot(this, &MainWindow::selectionSet));

  label = new Gtk::Label(string("Cursor: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*cursorPos_);
  label->show();
  cursorPos_->show();

  label = new Gtk::Label(string("Marker: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*markerPos_);
  label->show();
  markerPos_->show();

  label = new Gtk::Label(string("Selection: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*selectionStartPos_);
  label->show();
  selectionStartPos_->show();

  label = new Gtk::Label(string(" - "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*selectionEndPos_);
  label->show();
  selectionEndPos_->show();
  
  vbox_.pack_start(*selectionInfoBox, FALSE, FALSE);
  selectionInfoBox->show();

  Gtk::HButtonBox *buttonBox = new Gtk::HButtonBox(GTK_BUTTONBOX_START, 5);
//llanero  zoomButton_ = new Gtk::RadioButton(NULL, string("Zoom"));
  zoomButton_ = new Gtk::RadioButton(string("Zoom"));
//llanero  selectButton_ = new Gtk::RadioButton(zoomButton_->group(), string("Select"));
  selectButton_ = new Gtk::RadioButton(string("Select"));
  selectButton_->set_group(zoomButton_->group());

  playButton_ = new Gtk::Button(string("Play"));

  buttonBox->pack_start(*zoomButton_);
  zoomButton_->show();
  buttonBox->pack_start(*selectButton_);
  selectButton_->show();
  zoomButton_->set_active(true);
  setMode(ZOOM);
  buttonBox->pack_start(*playButton_);
  playButton_->show();

  vbox_.pack_start(*buttonBox, FALSE, FALSE);
  buttonBox->show();

  statusBar_ = new Gtk::Statusbar;
  vbox_.pack_start(*statusBar_, FALSE, FALSE);
  lastMessageId_ = statusBar_->push(1, string(""));
  statusBar_->show();

  statusMessage("xcdrdao %s", VERSION);

//llanero  add(&vbox_);
  vbox_.show();

//llanero:
/*  connect_to_method(sampleDisplay_->markerSet, this,
                    &MainWindow::markerSetCallback);
  connect_to_method(sampleDisplay_->selectionSet, this,
		    &MainWindow::selectionSetCallback);
  connect_to_method(sampleDisplay_->cursorMoved, this,
                    &MainWindow::cursorMovedCallback);
  connect_to_method(sampleDisplay_->trackMarkSelected, this,
		    &MainWindow::trackMarkSelectedCallback);
  connect_to_method(sampleDisplay_->trackMarkMoved, this,
		    &MainWindow::trackMarkMovedCallback);
*/
  sampleDisplay_->markerSet.connect(slot(this,
  			&MainWindow::markerSetCallback));
  sampleDisplay_->selectionSet.connect(slot(this,
  			&MainWindow::selectionSetCallback));
  sampleDisplay_->cursorMoved.connect(slot(this,
  			&MainWindow::cursorMovedCallback));
  sampleDisplay_->trackMarkSelected.connect(slot(this,
  			&MainWindow::trackMarkSelectedCallback));
  sampleDisplay_->trackMarkMoved.connect(slot(this,
  			&MainWindow::trackMarkMovedCallback));

//llanero:
/*  connect_to_method(zoomButton_->toggled, this, &MainWindow::setMode, ZOOM);
  connect_to_method(selectButton_->toggled, this, &MainWindow::setMode, SELECT);
  connect_to_method(playButton_->clicked, this, &MainWindow::play);
*/
  zoomButton_->toggled.connect(bind(slot(this, &MainWindow::setMode), ZOOM));
  selectButton_->toggled.connect(bind(slot(this, &MainWindow::setMode), SELECT));
  playButton_->clicked.connect(slot(this, &MainWindow::play));

}

void MainWindow::createMenuBar()
{
//llanero: ???  Gtk::AccelGroup accelGroup;

//llanero  itemFactory_ = new Gtk::ItemFactory_MenuBar("<Main>", accelGroup);
//llanero: accelGroup!!! or -> use GNOME menus (will solve this)
//  itemFactory_ = new Gtk::ItemFactory_MenuBar("<Main>");
//  Gtk::ItemFactory &f = *itemFactory_;

// NOTE : Comented all menu stuff. This has changed so better than rewriting
//        it twice, I will use easier GNOME menus.

/*
  f.create_item("/File", NULL, "<Branch>", 0);

  f.create_item("/File/New", 0, "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::newToc));


  f.create_item("/File/Open...", "<control>O", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::readToc));

  f.create_item("/File/Save", "<control>S", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::saveToc));
  f.create_item("/File/Save As...", 0,"<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::saveAsToc));

  f.create_item("/File/", 0, "<Separator>",0);
  f.create_item("/File/Quit", "<alt>X", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::quit));

  f.create_item("/View", NULL, "<Branch>", 0);
  f.create_item("/View/Zoom to Selection", "<shift>z", "<Item>",
		ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::zoomIn));
  f.create_item("/View/Zoom out", "Z", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::zoomOut));
  f.create_item("/View/Fullview", "F", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::fullView));


  f.create_item("/Edit", NULL, "<Branch>", 0);

  f.create_item("/Edit/Cut", "<control>K", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this,
					     &MainWindow::cutTrackData));
  f.create_item("/Edit/Paste", "<control>Y", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this,
					     &MainWindow::pasteTrackData));

  f.create_item("/Edit/", 0, "<Separator>", 0);

  f.create_item("/Edit/Add Track Mark", "T", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::addTrackMark));

  f.create_item("/Edit/Add Index Mark", "I", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::addIndexMark));

  f.create_item("/Edit/Add Pre-Gap", "P", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::addPregap));

  f.create_item("/Edit/Remove Track Mark", "<control>D", "<Item>",
   ItemFactoryConnector<MainWindow, ignored>(this,
					     &MainWindow::removeTrackMark));

  f.create_item("/Tools", NULL, "<Branch>", 0);
  f.create_item("/Tools/Disk Info...", 0, "<Item>",
		ItemFactoryConnector<MainWindow, ignored>(this,
							  &MainWindow::tocInfo));
  f.create_item("/Tools/Track Info...", 0, "<Item>",
		ItemFactoryConnector<MainWindow, ignored>(this,
							  &MainWindow::trackInfo));
  f.create_item("/Tools/", 0, "<Separator>", 0);

  f.create_item("/Tools/Append Track...", "<control>T", "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::appendTrack));

  f.create_item("/Tools/Append File...", "<control>F", "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::appendFile));

  f.create_item("/Tools/Insert File...", "<control>I" , "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this, &MainWindow::insertFile));


  f.create_item("/Tools/", 0, "<Separator>", 0);

  f.create_item("/Tools/Append Silence...", 0, "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this,
					      &MainWindow::appendSilence));

  f.create_item("/Tools/Insert Silence...", 0, "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this,
					      &MainWindow::insertSilence));


  f.create_item("/Settings", 0, "<Branch>", 0);
  f.create_item("/Settings/Devices...", 0, "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this,
					      &MainWindow::configureDevices));

  //f.create_item("/Options/Settings...", 0, "<Item>", 0);
  f.create_item("/Actions", 0, "<Branch>", 0);
  f.create_item("/Actions/Extract...", 0, "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this,
					      &MainWindow::extract));
  f.create_item("/Actions/Record...", 0, "<Item>",
    ItemFactoryConnector<MainWindow, ignored>(this,
					      &MainWindow::record));
  
  //f.create_item("/Help", 0, "<LastBranch>", 0);

  add_accel_group(accelGroup);

  menuBar_ = f.get_menubar_widget("");
*/
}

gint MainWindow::delete_event_impl(GdkEventAny*)
{
  quit();
  return 1;
}


void MainWindow::quit()
{
  if (tocEdit_->tocDirty()) {
    Ask2Box msg(this, "Quit", 0, 2, "Current work not saved.", "",
		"Really Quit?", NULL);
    if (msg.run() != 1)
      return;
  }

  hide();
//llanero  Gtk::Main::instance()->quit();
  Gtk::Main::quit();
}

void MainWindow::update(unsigned long level)
{
  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
    string s(tocEdit_->filename());

    if (tocEdit_->tocDirty())
      s += "(*)";
    
    set_title(s);

    cursorPos_->set_text("");
  }

  if (level & UPD_TRACK_MARK_SEL) {
    int trackNr, indexNr;

    if (tocEdit_->trackSelection(&trackNr) && 
	tocEdit_->indexSelection(&indexNr)) {
      sampleDisplay_->setSelectedTrackMarker(trackNr, indexNr);
    }
    else {
      sampleDisplay_->setSelectedTrackMarker(0, 0);
    }
  }

  if (level & UPD_SAMPLES) {
    sampleDisplay_->updateToc();
  }
  else if (level & (UPD_TRACK_DATA | UPD_TRACK_MARK_SEL)) {
    sampleDisplay_->updateTrackMarks();
  }

  if (level & UPD_SAMPLE_MARKER) {
    unsigned long marker;

    if (tocEdit_->sampleMarker(&marker)) {
      markerPos_->set_text(string(sample2string(marker)));
      sampleDisplay_->setMarker(marker);
    }
    else {
      markerPos_->set_text(string(""));
      sampleDisplay_->clearMarker();
    }
  }

  if (level & UPD_SAMPLE_SEL) {
    unsigned long start, end;

    if (tocEdit_->sampleSelection(&start, &end)) {
      selectionStartPos_->set_text(string(sample2string(start)));
      selectionEndPos_->set_text(string(sample2string(end)));
      sampleDisplay_->setRegion(start, end);
    }
    else {
      selectionStartPos_->set_text(string(""));
      selectionEndPos_->set_text(string(""));
      sampleDisplay_->setRegion(1, 0);
    }
  }
}

void MainWindow::tocBlockedMsg(const char *op)
{
  MessageBox msg(this, op, 0,
		 "Cannot perform requested operation because", 
		 "project is in read-only state.", NULL);
  msg.run();

}

void MainWindow::newToc()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("New Toc");
    return;
  }

  if (tocEdit_->tocDirty()) {
    Ask2Box msg(this, "New", 0, 2, "Current work not saved.", "",
		"Continue?", NULL);
    if (msg.run() != 1)
      return;
  }

  Toc *toc = new Toc;
  
  tocEdit_->toc(toc, "unnamed.toc");

  guiUpdate();
}

void MainWindow::readToc()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Read Toc");
    return;
  }

  if (tocEdit_->tocDirty()) {
    Ask2Box msg(this, "Read", 0, 2, "Current work not saved.", "",
		"Continue?", NULL);
    if (msg.run() != 1)
      return;
  }

  fileSelector_->set_title("Read Toc-File");
//llanero  fileSelectorC1_ = connect_to_method(fileSelector_->get_ok_button()->clicked,
//				      this, &MainWindow::readTocCallback, 1);
//  fileSelectorC2_ =
//    connect_to_method(fileSelector_->get_cancel_button()->clicked,
//		      this, &MainWindow::readTocCallback, 0);

  fileSelectorC1_ = fileSelector_->get_ok_button()->clicked.connect(
  				bind(slot(this, &MainWindow::readTocCallback), 1));
  fileSelectorC2_ = fileSelector_->get_cancel_button()->clicked.connect(
  				bind(slot(this, &MainWindow::readTocCallback), 0));

  Gtk::Main::instance()->grab_add(*fileSelector_);
  fileSelector_->show();
}

void MainWindow::saveToc()
{
  if (tocEdit_->saveToc() == 0) {
    statusMessage("Toc saved to \"%s\".", tocEdit_->filename());
    guiUpdate();
  }
  else {
    statusMessage("Cannot save toc to \"%s\": %s", tocEdit_->filename(),
		  strerror(errno));
  }
}

void MainWindow::saveAsToc()
{
  fileSelector_->set_title("Save Toc-File");
//llanero  fileSelectorC1_ = connect_to_method(fileSelector_->get_ok_button()->clicked,
//				      this, &MainWindow::saveAsTocCallback, 1);
//  fileSelectorC2_ =
//    connect_to_method(fileSelector_->get_cancel_button()->clicked,
//		      this, &MainWindow::saveAsTocCallback, 0);

  fileSelectorC1_ = fileSelector_->get_ok_button()->clicked.connect(
  				bind(slot(this, &MainWindow::saveAsTocCallback), 1));
  fileSelectorC2_ = fileSelector_->get_cancel_button()->clicked.connect(
  				bind(slot(this, &MainWindow::saveAsTocCallback), 0));

  Gtk::Main::instance()->grab_add(*fileSelector_);
  fileSelector_->show();
}

void MainWindow::readTocCallback(int action)
{
  if (action == 0) {
    fileSelectorC1_.disconnect();
    fileSelectorC2_.disconnect();
    Gtk::Main::instance()->grab_remove(*fileSelector_);
    fileSelector_->hide();
    
  }
  else {
    const char *s = strdupCC(fileSelector_->get_filename().c_str());

    if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
      fileSelectorC1_.disconnect();
      fileSelectorC2_.disconnect();
      Gtk::Main::instance()->grab_remove(*fileSelector_);
      fileSelector_->hide();

      if (tocEdit_->readToc(stripCwd(s)) == 0) {
	sampleDisplay_->setView(0, tocEdit_->lengthSample() - 1);
	guiUpdate();
      }
    }
    delete[] s;
  }
}

void MainWindow::saveAsTocCallback(int action)
{
  if (action == 0) {
    fileSelectorC1_.disconnect();
    fileSelectorC2_.disconnect();
    Gtk::Main::instance()->grab_remove(*fileSelector_);
    fileSelector_->hide();
  }
  else {
    const char *s = strdupCC(fileSelector_->get_filename().c_str());

    if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
      fileSelectorC1_.disconnect();
      fileSelectorC2_.disconnect();
      Gtk::Main::instance()->grab_remove(*fileSelector_);
      fileSelector_->hide();

      if (tocEdit_->saveAsToc(stripCwd(s)) == 0) {
	statusMessage("Toc saved to \"%s\".", tocEdit_->filename());
	guiUpdate();
      }
      else {
	statusMessage("Cannot save toc to \"%s\": %s", s, strerror(errno));
      }
    }

    delete[] s;
  }
}

void MainWindow::zoomIn()
{
  unsigned long start, end;

  if (tocEdit_->sampleSelection(&start, &end)) {
    sampleDisplay_->setView(start, end);
  }
}
 
void MainWindow::zoomOut()
{
  unsigned long start, end, len, center;

  sampleDisplay_->getView(&start, &end);

  len = end - start + 1;
  center = start + len / 2;

  if (center > len)
    start = center - len;
  else 
    start = 0;

  end = center + len;
  if (end >= tocEdit_->toc()->length().samples())
    end = tocEdit_->toc()->length().samples() - 1;
  
  sampleDisplay_->setView(start, end);
}

void MainWindow::fullView()
{
  unsigned long len;

  if ((len = tocEdit_->toc()->length().samples()) > 0) {
    sampleDisplay_->setView(0, len - 1);
  }
}

void MainWindow::markerSetCallback(unsigned long sample)
{
  tocEdit_->sampleMarker(sample);
  guiUpdate();
}

void MainWindow::selectionSetCallback(unsigned long start,
				      unsigned long end)
{
  if (mode_ == ZOOM) {
    sampleDisplay_->setView(start, end);
  }
  else {
    tocEdit_->sampleSelection(start, end);
    guiUpdate();
  }
}

void MainWindow::cursorMovedCallback(unsigned long pos)
{
  cursorPos_->set_text(string(sample2string(pos)));
}

void MainWindow::setMode(Mode m)
{
  mode_ = m;

  /*
  if (mode_ == ZOOM && !zoomButton_->get_state()) {
    zoomButton_->set_state(true);
  }
  else if (mode_ == SELECT && !selectButton_->get_state()) {
    selectButton_->set_state(true);
  }
  */
}

const char *MainWindow::sample2string(unsigned long sample)
{
  static char buf[50];

  unsigned long min = sample / (60 * 44100);
  sample %= 60 * 44100;

  unsigned long sec = sample / 44100;
  sample %= 44100;

  unsigned long frame = sample / 588;
  sample %= 588;

  sprintf(buf, "%2lu:%02lu:%02lu.%03lu", min, sec, frame, sample);
  
  return buf;
}

unsigned long MainWindow::string2sample(const char *str)
{
  int m = 0;
  int s = 0;
  int f = 0;
  int n = 0;

  sscanf(str, "%d:%d:%d.%d", &m, &s, &f, &n);

  if (m < 0)
    m = 0;

  if (s < 0 || s > 59)
    s = 0;

  if (f < 0 || f > 74)
    f = 0;

  if (n < 0 || n > 587)
    n = 0;

  return Msf(m, s, f).samples() + n;
}

int MainWindow::snapSampleToBlock(unsigned long sample, long *block)
{
  unsigned long rest = sample % SAMPLES_PER_BLOCK;

  *block = sample / SAMPLES_PER_BLOCK;

  if (rest == 0) 
    return 0;

  if (rest > SAMPLES_PER_BLOCK / 2)
    *block += 1;

  return 1;
}

void MainWindow::statusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  strstream str;

  str.vform(fmt, args);
  str << ends;

  statusBar_->remove_message(1, lastMessageId_);

  lastMessageId_ = statusBar_->push(1, string(str.str()));

  str.freeze(0);

  va_end(args);
}

void MainWindow::trackMarkSelectedCallback(const Track *, int trackNr,
					   int indexNr)
{
  tocEdit_->trackSelection(trackNr);
  tocEdit_->indexSelection(indexNr);
  guiUpdate();
}

void MainWindow::trackMarkMovedCallback(const Track *, int trackNr,
					int indexNr, unsigned long sample)
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Move Track Marker");
    return;
  }

  long lba;
  int snapped = snapSampleToBlock(sample, &lba);

  switch (tocEdit_->moveTrackMarker(trackNr, indexNr, lba)) {
  case 0:
    statusMessage("Moved track marker to %s%s.", Msf(lba).str(),
		  snapped ? " (snapped to next block)" : "");
    break;

  case 6:
    statusMessage("Cannot modify a data track.");
    break;
  default:
    statusMessage("Illegal track marker position.");
    break;
  }

  tocEdit_->trackSelection(trackNr);
  tocEdit_->indexSelection(indexNr);
  guiUpdate();
}

void MainWindow::play()
{
  unsigned long start, end;

  if (playing_) {
    playAbort_ = 1;
    return;
  }

  if (tocEdit_->lengthSample() == 0)
    return;

  if (soundInterface_ == NULL) {
    soundInterface_ = new SoundIF;
    if (soundInterface_->init() != 0) {
      delete soundInterface_;
      soundInterface_ = NULL;
      return;
    }
  }

  if (soundInterface_->start() != 0)
    return;

  if (!sampleDisplay_->getRegion(&start, &end))
    sampleDisplay_->getView(&start, &end);

  tocReader.init(tocEdit_->toc());
  if (tocReader.openData() != 0) {
    tocReader.init(NULL);
    soundInterface_->end();
    return;
    }

  if (tocReader.seekSample(start) != 0) {
    tocReader.init(NULL);
    soundInterface_->end();
    return;
  }

  playLength_ = end - start + 1;
  playPosition_ = start;
  playing_ = 1;
  playAbort_ = 0;

  tocEdit_->blockEdit();
  guiUpdate();

//llanero  connect_to_method(Gtk::Main::idle(), this, &MainWindow::playCallback);
  Gtk::Main::idle.connect(slot(this, &MainWindow::playCallback));
}

int MainWindow::playCallback()
{
  long len = playLength_ > playBurst_ ? playBurst_ : playLength_;


  if (tocReader.readSamples(playBuffer_, len) != len ||
      soundInterface_->play(playBuffer_, len) != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
    sampleDisplay_->setCursor(0, 0);
    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }

  playLength_ -= len;
  playPosition_ += len;

  unsigned long delay = soundInterface_->getDelay();

  if (delay <= playPosition_) {
    sampleDisplay_->setCursor(1, playPosition_ - delay);
    cursorPos_->set_text(string(sample2string(playPosition_ - delay)));
  }

  if (len == 0 || playAbort_ != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
    sampleDisplay_->setCursor(0, 0);
    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }
  else {
    return 1; // keep idle handler
  }
}

int MainWindow::getMarker(unsigned long *sample)
{
  if (tocEdit_->lengthSample() == 0)
    return 0;

  if (sampleDisplay_->getMarker(sample) == 0) {
    statusMessage("Please set marker.");
    return 0;
  }

  return 1;
}

void MainWindow::addTrackMark()
{
  unsigned long sample;

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Add Track Mark");
    return;
  }

  if (getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (tocEdit_->addTrackMarker(lba)) {
    case 0:
      statusMessage("Added track mark at %s%s.", Msf(lba).str(),
		    snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      statusMessage("Cannot add track at this point.");
      break;

    case 3:
    case 4:
      statusMessage("Resulting track would be shorter than 4 seconds.");
      break;

    case 5:
      statusMessage("Cannot modify a data track.");
      break;

    default:
      statusMessage("Internal error in addTrackMark(), please report.");
      break;
    }
  }
}

void MainWindow::addIndexMark()
{
  unsigned long sample;

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Add Index Mark");
    return;
  }

  if (getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (tocEdit_->addIndexMarker(lba)) {
    case 0:
      statusMessage("Added index mark at %s%s.", Msf(lba).str(),
		    snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      statusMessage("Cannot add index at this point.");
      break;

    case 3:
      statusMessage("Track has already 98 index marks.");
      break;

    default:
      statusMessage("Internal error in addIndexMark(), please report.");
      break;
    }
  }
}

void MainWindow::addPregap()
{
  unsigned long sample;

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Add Pre-Gap");
    return;
  }

  if (getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (tocEdit_->addPregap(lba)) {
    case 0:
      statusMessage("Added pre-gap mark at %s%s.", Msf(lba).str(),
		    snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      statusMessage("Cannot add pre-gap at this point.");
      break;

    case 3:
      statusMessage("Track would be shorter than 4 seconds.");
      break;

    case 4:
      statusMessage("Cannot modify a data track.");
      break;

    default:
      statusMessage("Internal error in addPregap(), please report.");
      break;
    }
  }
}

void MainWindow::removeTrackMark()
{
  int trackNr;
  int indexNr;

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Remove Track Mark");
    return;
  }

  if (tocEdit_->trackSelection(&trackNr) &&
      tocEdit_->indexSelection(&indexNr)) {
    switch (tocEdit_->removeTrackMarker(trackNr, indexNr)) {
    case 0:
      statusMessage("Removed track/index marker.");
      guiUpdate();
      break;
    case 1:
      statusMessage("Cannot remove first track.");
      break;
    case 3:
      statusMessage("Cannot modify a data track.");
      break;
    default:
      statusMessage("Internal error in removeTrackMark(), please report.");
      break; 
    }
  }
  else {
    statusMessage("Please select a track/index mark.");
  }

}

void MainWindow::appendTrack()
{
  ADD_FILE_DIALOG->mode(AddFileDialog::M_APPEND_TRACK);
  ADD_FILE_DIALOG->start(tocEdit_);
}



void MainWindow::appendFile()
{
  ADD_FILE_DIALOG->mode(AddFileDialog::M_APPEND_FILE);
  ADD_FILE_DIALOG->start(tocEdit_);
}


void MainWindow::insertFile()
{
  ADD_FILE_DIALOG->mode(AddFileDialog::M_INSERT_FILE);
  ADD_FILE_DIALOG->start(tocEdit_);
}

void MainWindow::appendSilence()
{
  ADD_SILENCE_DIALOG->mode(AddSilenceDialog::M_APPEND);
  ADD_SILENCE_DIALOG->start(tocEdit_);
}

void MainWindow::insertSilence()
{
  ADD_SILENCE_DIALOG->mode(AddSilenceDialog::M_INSERT);
  ADD_SILENCE_DIALOG->start(tocEdit_);
}

void MainWindow::trackInfo()
{
  TRACK_INFO_DIALOG->start(tocEdit_);
}

void MainWindow::tocInfo()
{
  TOC_INFO_DIALOG->start(tocEdit_);
}

void MainWindow::cutTrackData()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Cut");
    return;
  }

  switch (tocEdit_->removeTrackData()) {
  case 0:
    statusMessage("Removed selected samples.");
    guiUpdate();
    break;
  case 1:
    statusMessage("Please select samples.");
    break;
  case 2:
    statusMessage("Selected sample range crosses track boundaries.");
    break;
  }
}

void MainWindow::pasteTrackData()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Paste");
    return;
  }

  switch (tocEdit_->insertTrackData()) {
  case 0:
    statusMessage("Pasted samples.");
    guiUpdate();
    break;
  case 1:
    statusMessage("No samples in scrap.");
    break;
  }
}

void MainWindow::markerSet()
{
  unsigned long s = string2sample(markerPos_->get_text().c_str());

  tocEdit_->sampleMarker(s);
  guiUpdate();
}

void MainWindow::selectionSet()
{
  unsigned long s1 = string2sample(selectionStartPos_->get_text().c_str());
  unsigned long s2 = string2sample(selectionEndPos_->get_text().c_str());

  tocEdit_->sampleSelection(s1, s2);
  guiUpdate();
}

void MainWindow::configureDevices()
{
  DEVICE_CONF_DIALOG->start(tocEdit_);
}

void MainWindow::extract()
{
  EXTRACT_DIALOG->start(tocEdit_);
}

void MainWindow::record()
{
  RECORD_DIALOG->start(tocEdit_);
}
