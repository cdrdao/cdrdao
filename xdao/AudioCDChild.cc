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

#include <stdio.h>
#include <fstream.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <strstream.h>

#include "xcdrdao.h"
#include "guiUpdate.h"
#include "MessageBox.h"
#include "AudioCDChild.h"
#include "SampleDisplay.h"
#include "SoundIF.h"
#include "TocEdit.h"
#include "AddFileDialog.h"
#include "AddSilenceDialog.h"
#include "MDIWindow.h"

#include "Toc.h"
#include "TrackData.h"
#include "util.h"


GtkWidget *
AudioCDChild_Creator(GnomeMDIChild *child, gpointer data)
{
  return static_cast<GtkWidget *>(data);
}

void
AudioCDChild::BuildChild()
{
// THIS FUNC SHOULD GO AWAY AND PUT THE CODE IN THE CONSTRUCTOR
// WHEN WE ARE SURE WE DON'T NEED TO CALL THIS METHOD:
// THIS IS, THE AudioCDChild_Creator works well (better).

// This should be the builder of the widgets...
//  vbox_ = new Gtk::VBox(FALSE, 5);
  vbox_aux_ = gtk_vbox_new(FALSE, 5);
  vbox_ = Gtk::wrap((GtkVBox *) vbox_aux_);

  sampleDisplay_ = new SampleDisplay;
  
  vbox_->pack_start(*sampleDisplay_, TRUE, TRUE);
  sampleDisplay_->show();

  Gtk::HScrollbar *scrollBar =
    new Gtk::HScrollbar(*(sampleDisplay_->getAdjustment()));
  vbox_->pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();
  
  Gtk::Label *label;
  Gtk::HBox *selectionInfoBox = new Gtk::HBox;

  markerPos_ = new Gtk::Entry;
  markerPos_->set_editable(true);
  markerPos_->activate.connect(slot(this, &AudioCDChild::markerSet));

  cursorPos_ = new Gtk::Entry;
  cursorPos_->set_editable(false);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
  selectionStartPos_->activate.connect(slot(this, &AudioCDChild::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
  selectionEndPos_->activate.connect(slot(this, &AudioCDChild::selectionSet));

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
  
  vbox_->pack_start(*selectionInfoBox, FALSE, FALSE);
  selectionInfoBox->show();

  Gtk::HButtonBox *buttonBox = new Gtk::HButtonBox(GTK_BUTTONBOX_START, 5);
  zoomButton_ = new Gtk::RadioButton(string("Zoom"));
  selectButton_ = new Gtk::RadioButton(string("Select"));
  selectButton_->set_group(zoomButton_->group());

  playButton_ = new Gtk::Button();
  playLabel_ = new Gtk::Label("Play");
  playButton_->add (* playLabel_);
  playLabel_->show();
  
  buttonBox->pack_start(*zoomButton_);
  zoomButton_->show();
  buttonBox->pack_start(*selectButton_);
  selectButton_->show();
  zoomButton_->set_active(true);
  setMode(ZOOM);
  buttonBox->pack_start(*playButton_);
  playButton_->show();

  Gtk::Button *button = manage(new Gtk::Button("Zoom Out"));
  buttonBox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AudioCDChild::zoomOut));

  button = manage(new Gtk::Button("Full View"));
  buttonBox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AudioCDChild::fullView));
  
  vbox_->pack_start(*buttonBox, FALSE, FALSE);
  buttonBox->show();


//llanero  add(&vbox_);
//MDI  vbox_->show();

  sampleDisplay_->markerSet.connect(slot(this,
  			&AudioCDChild::markerSetCallback));
  sampleDisplay_->selectionSet.connect(slot(this,
  			&AudioCDChild::selectionSetCallback));
  sampleDisplay_->cursorMoved.connect(slot(this,
  			&AudioCDChild::cursorMovedCallback));
  sampleDisplay_->trackMarkSelected.connect(slot(this,
  			&AudioCDChild::trackMarkSelectedCallback));
  sampleDisplay_->trackMarkMoved.connect(slot(this,
  			&AudioCDChild::trackMarkMovedCallback));

  zoomButton_->toggled.connect(bind(slot(this, &AudioCDChild::setMode), ZOOM));
  selectButton_->toggled.connect(bind(slot(this, &AudioCDChild::setMode), SELECT));
  playButton_->clicked.connect(slot(this, &AudioCDChild::play));

}


AudioCDChild::AudioCDChild() : Gnome::MDIGenericChild("Untitled AudioCD")
{
  tocEdit_ = NULL;

  AudioCDChild::BuildChild();

  playing_ = 0;
  playBurst_ = 588 * 5;
  soundInterface_ = new SoundIF;
  playBuffer_ = new Sample[playBurst_];
  soundInterface_ = NULL;

  //Quick hack until gnome-- gets a good function to bypass this.
//FIXME: MDI STUFF  AudioCDChild::set_view_creator(&AudioCDChild_Creator, vbox_aux_);

}

void AudioCDChild::play()
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
  playLabel_->set_text("Stop");

  tocEdit_->blockEdit();

  guiUpdate();

  Gtk::Main::idle.connect(slot(this, &AudioCDChild::playCallback));
}

int AudioCDChild::playCallback()
{
  long len = playLength_ > playBurst_ ? playBurst_ : playLength_;


  if (tocReader.readSamples(playBuffer_, len) != len ||
      soundInterface_->play(playBuffer_, len) != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
    playLabel_->set_text("Play");
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
    playLabel_->set_text("Play");
    sampleDisplay_->setCursor(0, 0);
    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }
  else {
    return 1; // keep idle handler
  }
}

void AudioCDChild::update(unsigned long level, TocEdit *tedit)
{
  if (tocEdit_ != tedit) {
    tocEdit_ = tedit;
    sampleDisplay_->setTocEdit(tedit);
    level = UPD_ALL;
  }

  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
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
    unsigned long smin, smax;
    tocEdit_->sampleView(&smin, &smax);
    sampleDisplay_->updateToc(smin, smax);
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

void AudioCDChild::zoomIn()
{
  unsigned long start, end;

  if (tocEdit_->sampleSelection(&start, &end)) {
    tocEdit_->sampleView(start, end);
    guiUpdate();
  }
}
 
void AudioCDChild::zoomOut()
{
  unsigned long start, end, len, center;

  tocEdit_->sampleView(&start, &end);

  len = end - start + 1;
  center = start + len / 2;

  if (center > len)
    start = center - len;
  else 
    start = 0;

  end = center + len;
  if (end >= tocEdit_->toc()->length().samples())
    end = tocEdit_->toc()->length().samples() - 1;

  tocEdit_->sampleView(start, end);
  guiUpdate();
}

void AudioCDChild::fullView()
{
  tocEdit_->sampleViewFull();
  guiUpdate();
}

void AudioCDChild::markerSetCallback(unsigned long sample)
{
  tocEdit_->sampleMarker(sample);
  guiUpdate();
}

void AudioCDChild::selectionSetCallback(unsigned long start,
				      unsigned long end)
{
  if (mode_ == ZOOM) {
    tocEdit_->sampleView(start, end);
  }
  else {
    tocEdit_->sampleSelection(start, end);
  }

  guiUpdate();
}

void AudioCDChild::cursorMovedCallback(unsigned long pos)
{
  cursorPos_->set_text(string(sample2string(pos)));
}

void AudioCDChild::setMode(Mode m)
{
  mode_ = m;
}

const char *AudioCDChild::sample2string(unsigned long sample)
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

unsigned long AudioCDChild::string2sample(const char *str)
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

int AudioCDChild::snapSampleToBlock(unsigned long sample, long *block)
{
  unsigned long rest = sample % SAMPLES_PER_BLOCK;

  *block = sample / SAMPLES_PER_BLOCK;

  if (rest == 0) 
    return 0;

  if (rest > SAMPLES_PER_BLOCK / 2)
    *block += 1;

  return 1;
}


void AudioCDChild::trackMarkSelectedCallback(const Track *, int trackNr,
					   int indexNr)
{
  tocEdit_->trackSelection(trackNr);
  tocEdit_->indexSelection(indexNr);
  guiUpdate();
}

void AudioCDChild::trackMarkMovedCallback(const Track *, int trackNr,
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
    MDI_WINDOW->statusMessage("Moved track marker to %s%s.", Msf(lba).str(),
			      snapped ? " (snapped to next block)" : "");
    break;

  case 6:
    MDI_WINDOW->statusMessage("Cannot modify a data track.");
    break;
  default:
    MDI_WINDOW->statusMessage("Illegal track marker position.");
    break;
  }

  tocEdit_->trackSelection(trackNr);
  tocEdit_->indexSelection(indexNr);
  guiUpdate();
}

int AudioCDChild::getMarker(unsigned long *sample)
{
  if (tocEdit_->lengthSample() == 0)
    return 0;

  if (sampleDisplay_->getMarker(sample) == 0) {
    MDI_WINDOW->statusMessage("Please set marker.");
    return 0;
  }

  return 1;
}

void AudioCDChild::addTrackMark()
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
      MDI_WINDOW->statusMessage("Added track mark at %s%s.", Msf(lba).str(),
				snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      MDI_WINDOW->statusMessage("Cannot add track at this point.");
      break;

    case 3:
    case 4:
      MDI_WINDOW->statusMessage("Resulting track would be shorter than 4 seconds.");
      break;

    case 5:
      MDI_WINDOW->statusMessage("Cannot modify a data track.");
      break;

    default:
      MDI_WINDOW->statusMessage("Internal error in addTrackMark(), please report.");
      break;
    }
  }
}

void AudioCDChild::addIndexMark()
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
      MDI_WINDOW->statusMessage("Added index mark at %s%s.", Msf(lba).str(),
				snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      MDI_WINDOW->statusMessage("Cannot add index at this point.");
      break;

    case 3:
      MDI_WINDOW->statusMessage("Track has already 98 index marks.");
      break;

    default:
      MDI_WINDOW->statusMessage("Internal error in addIndexMark(), please report.");
      break;
    }
  }
}

void AudioCDChild::addPregap()
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
      MDI_WINDOW->statusMessage("Added pre-gap mark at %s%s.", Msf(lba).str(),
				snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      MDI_WINDOW->statusMessage("Cannot add pre-gap at this point.");
      break;

    case 3:
      MDI_WINDOW->statusMessage("Track would be shorter than 4 seconds.");
      break;

    case 4:
      MDI_WINDOW->statusMessage("Cannot modify a data track.");
      break;

    default:
      MDI_WINDOW->statusMessage("Internal error in addPregap(), please report.");
      break;
    }
  }
}

void AudioCDChild::removeTrackMark()
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
      MDI_WINDOW->statusMessage("Removed track/index marker.");
      guiUpdate();
      break;
    case 1:
      MDI_WINDOW->statusMessage("Cannot remove first track.");
      break;
    case 3:
      MDI_WINDOW->statusMessage("Cannot modify a data track.");
      break;
    default:
      MDI_WINDOW->statusMessage("Internal error in removeTrackMark(), please report.");
      break; 
    }
  }
  else {
    MDI_WINDOW->statusMessage("Please select a track/index mark.");
  }

}

void AudioCDChild::appendTrack()
{
  ADD_FILE_DIALOG->mode(AddFileDialog::M_APPEND_TRACK);
  ADD_FILE_DIALOG->start(tocEdit_);
}



void AudioCDChild::appendFile()
{
  ADD_FILE_DIALOG->mode(AddFileDialog::M_APPEND_FILE);
  ADD_FILE_DIALOG->start(tocEdit_);
}


void AudioCDChild::insertFile()
{
  ADD_FILE_DIALOG->mode(AddFileDialog::M_INSERT_FILE);
  ADD_FILE_DIALOG->start(tocEdit_);
}

void AudioCDChild::appendSilence()
{
  ADD_SILENCE_DIALOG->mode(AddSilenceDialog::M_APPEND);
  ADD_SILENCE_DIALOG->start(tocEdit_);
}

void AudioCDChild::insertSilence()
{
  ADD_SILENCE_DIALOG->mode(AddSilenceDialog::M_INSERT);
  ADD_SILENCE_DIALOG->start(tocEdit_);
}


void AudioCDChild::cutTrackData()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Cut");
    return;
  }

  switch (tocEdit_->removeTrackData()) {
  case 0:
    MDI_WINDOW->statusMessage("Removed selected samples.");
    guiUpdate();
    break;
  case 1:
    MDI_WINDOW->statusMessage("Please select samples.");
    break;
  case 2:
    MDI_WINDOW->statusMessage("Selected sample range crosses track boundaries.");
    break;
  }
}

void AudioCDChild::pasteTrackData()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Paste");
    return;
  }

  switch (tocEdit_->insertTrackData()) {
  case 0:
    MDI_WINDOW->statusMessage("Pasted samples.");
    guiUpdate();
    break;
  case 1:
    MDI_WINDOW->statusMessage("No samples in scrap.");
    break;
  }
}

void AudioCDChild::markerSet()
{
  unsigned long s = string2sample(markerPos_->get_text().c_str());

  tocEdit_->sampleMarker(s);
  guiUpdate();
}

void AudioCDChild::selectionSet()
{
  unsigned long s1 = string2sample(selectionStartPos_->get_text().c_str());
  unsigned long s2 = string2sample(selectionEndPos_->get_text().c_str());

  tocEdit_->sampleSelection(s1, s2);
  guiUpdate();
}

void AudioCDChild::tocBlockedMsg(const char *op)
{
//  MessageBox msg(MDI_WINDOW->get_active_window(), op, 0,
  MessageBox msg(MDI_WINDOW, op, 0,
		 "Cannot perform requested operation because", 
		 "project is in read-only state.", NULL);
  msg.run();
}
