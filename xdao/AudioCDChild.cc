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

#include <stdio.h>
#include <fstream.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <strstream.h>


#include "AudioCDChild.h"
#include "SampleDisplay.h"
#include "SoundIF.h"
#include "TocEdit.h"

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

//  fileSelector_ = new Gtk::FileSelection("");
//  fileSelector_->complete(string("*.toc"));


  sampleDisplay_ = new SampleDisplay;
  sampleDisplay_->setTocEdit(tocEdit_);

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
//llanero  connect_to_method(markerPos_->activate, this, &MainWindow::markerSet);
//  markerPos_->activate.connect(slot(this, &MainWindow::markerSet));

  cursorPos_ = new Gtk::Entry;
  cursorPos_->set_editable(false);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
//llanero  connect_to_method(selectionStartPos_->activate, this,
//		    &MainWindow::selectionSet);
//  selectionStartPos_->activate.connect(slot(this, &MainWindow::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
//llanero  connect_to_method(selectionEndPos_->activate, this,
//		    &MainWindow::selectionSet);
//  selectionEndPos_->activate.connect(slot(this, &MainWindow::selectionSet));

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
//  setMode(ZOOM);
  buttonBox->pack_start(*playButton_);
  playButton_->show();

  vbox_->pack_start(*buttonBox, FALSE, FALSE);
  buttonBox->show();


//llanero  add(&vbox_);
  vbox_->show();

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
/* 
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

*/
//llanero:
/*  connect_to_method(zoomButton_->toggled, this, &MainWindow::setMode, ZOOM);
  connect_to_method(selectButton_->toggled, this, &MainWindow::setMode, SELECT);
  connect_to_method(playButton_->clicked, this, &MainWindow::play);
*/

//  zoomButton_->toggled.connect(bind(slot(this, &MainWindow::setMode), ZOOM));
//  selectButton_->toggled.connect(bind(slot(this, &MainWindow::setMode), SELECT));
  playButton_->clicked.connect(slot(this, &AudioCDChild::play));

}


AudioCDChild::AudioCDChild(TocEdit *tedit) : Gnome::MDIGenericChild("Untitled AudioCD")
{
  tocEdit_ = tedit;

  AudioCDChild::BuildChild();




  playing_ = 0;
  playBurst_ = 588 * 5;
  soundInterface_ = new SoundIF;
  playBuffer_ = new Sample[playBurst_];
  soundInterface_ = NULL;

  //Quick hack until gnome-- gets a good function to bypass this.
  AudioCDChild::set_view_creator(&AudioCDChild_Creator, vbox_aux_);

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

  tocEdit_->blockEdit();

//FIXME: !!!
//  guiUpdate();

//llanero  connect_to_method(Gtk::Main::idle(), this, &MainWindow::playCallback);
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
    sampleDisplay_->setCursor(0, 0);
    tocEdit_->unblockEdit();
//FIXME: !!!
//llanero    guiUpdate();
    return 0; // remove idle handler
  }

  playLength_ -= len;
  playPosition_ += len;

  unsigned long delay = soundInterface_->getDelay();

//FIXME: 
//llanero
/*
  if (delay <= playPosition_) {
    sampleDisplay_->setCursor(1, playPosition_ - delay);
    cursorPos_->set_text(string(sample2string(playPosition_ - delay)));
  }
*/
  if (len == 0 || playAbort_ != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
    sampleDisplay_->setCursor(0, 0);
    tocEdit_->unblockEdit();
//FIXME: !!!
//llanero    guiUpdate();
    return 0; // remove idle handler
  }
  else {
    return 1; // keep idle handler
  }
}
