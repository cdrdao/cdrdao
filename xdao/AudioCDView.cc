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
#include "SampleDisplay.h"
#include "TocEdit.h"
#include "MDIWindow.h"
#include "util.h"

#include "AudioCDChild.h"
#include "AudioCDView.h"
#include <gnome.h>

AudioCDView::AudioCDView(AudioCDChild *child) 
{
  cdchild = child;
  tocEdit_ = child->tocEdit_;

  Gtk::VBox *vbox = this;

  static const GtkTargetEntry drop_types [] =
  {
    { "text/uri-list", 0, TARGET_URI_LIST }
  };

  static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);

  drag_dest_set(static_cast <GtkDestDefaults> (GTK_DEST_DEFAULT_MOTION
    | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP),
    &drop_types[0], n_drop_types, GDK_ACTION_COPY);
  
  drag_data_received.connect(slot(this, &AudioCDView::drag_data_received_cb));

  sampleDisplay_ = new SampleDisplay;
  sampleDisplay_->setTocEdit(tocEdit_);

  sampleDisplay_->set_usize(200,200);
  
  vbox->pack_start(*sampleDisplay_, TRUE, TRUE);
  sampleDisplay_->show();

  Gtk::HScrollbar *scrollBar =
    new Gtk::HScrollbar(*(sampleDisplay_->getAdjustment()));
  vbox->pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();
  
  Gtk::Label *label;
  Gtk::HBox *selectionInfoBox = new Gtk::HBox;

  markerPos_ = new Gtk::Entry;
  markerPos_->set_editable(true);
  markerPos_->activate.connect(slot(this, &AudioCDView::markerSet));

  cursorPos_ = new Gtk::Entry;
  cursorPos_->set_editable(false);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
  selectionStartPos_->activate.connect(slot(this, &AudioCDView::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
  selectionEndPos_->activate.connect(slot(this, &AudioCDView::selectionSet));

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
  
  vbox->pack_start(*selectionInfoBox, FALSE, FALSE);
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
  button->clicked.connect(slot(this, &AudioCDView::zoomOut));

  button = manage(new Gtk::Button("Full View"));
  buttonBox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AudioCDView::fullView));
  
  vbox->pack_start(*buttonBox, FALSE, FALSE);
  buttonBox->show();

  sampleDisplay_->markerSet.connect(slot(this,
  			&AudioCDView::markerSetCallback));
  sampleDisplay_->selectionSet.connect(slot(this,
  			&AudioCDView::selectionSetCallback));
  sampleDisplay_->cursorMoved.connect(slot(this,
  			&AudioCDView::cursorMovedCallback));
  sampleDisplay_->trackMarkSelected.connect(slot(this,
  			&AudioCDView::trackMarkSelectedCallback));
  sampleDisplay_->trackMarkMoved.connect(slot(cdchild,
  			&AudioCDChild::trackMarkMovedCallback));

  zoomButton_->toggled.connect(bind(slot(this, &AudioCDView::setMode), ZOOM));
  selectButton_->toggled.connect(bind(slot(this, &AudioCDView::setMode), SELECT));
  playButton_->clicked.connect(slot(this, &AudioCDView::play));

  sampleMarkerValid_ = 0;
  sampleSelectionValid_ = 0;
  trackSelectionValid_ = 0;
  indexSelectionValid_ = 0;

  sampleSelectionMin_ = 0;
  sampleSelectionMax_ = 0;
  sampleViewMin_ = 0;
  sampleViewMax_ = 0;

  updateLevel_ = 0;

  sampleViewFull();
}

void AudioCDView::update(unsigned long level)
{
cout << "updating AudioCDView - " << cdchild->get_name() << endl;
//FIXME: we don't pass tedit now... 
// If some operation changes the tocEdit_ in the way this should catch
// it should do a guiUpdate(UPD_ALL)!
// Was this used when loading a new project??? remove it then.
/*
  if (tocEdit_ != tedit) {
    tocEdit_ = tedit;
//FIXME    sampleDisplay_->setTocEdit(tedit);
    level = UPD_ALL;
  }
*/

  level |= updateLevel_;
  updateLevel_ = 0;

  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
    cursorPos_->set_text("");
  }

  if (level & UPD_TRACK_MARK_SEL) {
    int trackNr, indexNr;

    if (trackSelection(&trackNr) && 
	indexSelection(&indexNr)) {
      sampleDisplay_->setSelectedTrackMarker(trackNr, indexNr);
    }
    else {
      sampleDisplay_->setSelectedTrackMarker(0, 0);
    }
  }

  if (level & UPD_SAMPLES) {
    unsigned long smin, smax;
    sampleView(&smin, &smax);
    sampleDisplay_->updateToc(smin, smax);
  }
  else if (level & (UPD_TRACK_DATA | UPD_TRACK_MARK_SEL)) {
    sampleDisplay_->updateTrackMarks();
  }

  if (level & UPD_SAMPLE_MARKER) {
    unsigned long marker;

    if (sampleMarker(&marker)) {
      markerPos_->set_text(string(cdchild->sample2string(marker)));
      sampleDisplay_->setMarker(marker);
    }
    else {
      markerPos_->set_text(string(""));
      sampleDisplay_->clearMarker();
    }
  }

  if (level & UPD_SAMPLE_SEL) {
    unsigned long start, end;

    if (sampleSelection(&start, &end)) {
      selectionStartPos_->set_text(string(cdchild->sample2string(start)));
      selectionEndPos_->set_text(string(cdchild->sample2string(end)));
      sampleDisplay_->setRegion(start, end);
    }
    else {
      selectionStartPos_->set_text(string(""));
      selectionEndPos_->set_text(string(""));
      sampleDisplay_->setRegion(1, 0);
    }
  }
}

void AudioCDView::zoomIn()
{
  unsigned long start, end;

  if (sampleSelection(&start, &end)) {
    sampleView(start, end);
    guiUpdate();
  }
}
 
void AudioCDView::zoomOut()
{
  unsigned long start, end, len, center;

  sampleView(&start, &end);

  len = end - start + 1;
  center = start + len / 2;

  if (center > len)
    start = center - len;
  else 
    start = 0;

  end = center + len;
  if (end >= tocEdit_->toc()->length().samples())
    end = tocEdit_->toc()->length().samples() - 1;

  sampleView(start, end);
  guiUpdate();
}

void AudioCDView::fullView()
{
  sampleViewFull();
  guiUpdate();
}

void AudioCDView::play()
{
  unsigned long start, end;

  if (!sampleDisplay_->getRegion(&start, &end))
    sampleDisplay_->getView(&start, &end);

  cdchild->play(start, end);
}

int AudioCDView::getMarker(unsigned long *sample)
{
  if (tocEdit_->lengthSample() == 0)
    return 0;

  if (sampleDisplay_->getMarker(sample) == 0) {
    MDI_WINDOW->statusMessage("Please set marker.");
    return 0;
  }

  return 1;
}

void AudioCDView::trackMarkSelectedCallback(const Track *, int trackNr,
					   int indexNr)
{
  trackSelection(trackNr);
  indexSelection(indexNr);
  guiUpdate();
}

void AudioCDView::markerSetCallback(unsigned long sample)
{
  sampleMarker(sample);
  guiUpdate();
}

void AudioCDView::selectionSetCallback(unsigned long start,
				      unsigned long end)
{
  if (mode_ == ZOOM) {
    sampleView(start, end);
  }
  else {
    sampleSelection(start, end);
  }
cout << "selectionSetCallback called" << endl;

  guiUpdate();
}

void AudioCDView::cursorMovedCallback(unsigned long pos)
{
  cursorPos_->set_text(string(cdchild->sample2string(pos)));
}

void AudioCDView::setMode(Mode m)
{
  mode_ = m;
}

void AudioCDView::markerSet()
{
  unsigned long s = cdchild->string2sample(markerPos_->get_text().c_str());

  sampleMarker(s);
  guiUpdate();
}

void AudioCDView::selectionSet()
{
  unsigned long s1 = cdchild->string2sample(selectionStartPos_->get_text().c_str());
  unsigned long s2 = cdchild->string2sample(selectionEndPos_->get_text().c_str());

  sampleSelection(s1, s2);
  guiUpdate();
}

void AudioCDChild::tocBlockedMsg(const char *op)
{
  MessageBox msg(MDI_WINDOW->get_active_window(), op, 0,
//  MessageBox msg(MDI_WINDOW, op, 0,
		 "Cannot perform requested operation because", 
		 "project is in read-only state.", NULL);
  msg.run();
}




void AudioCDView::sampleMarker(unsigned long sample)
{
  if (sample < tocEdit_->toc()->length().samples()) {
    sampleMarker_ = sample;
    sampleMarkerValid_ = 1;
  }
  else {
    sampleMarkerValid_ = 0;
  }

  updateLevel_ |= UPD_SAMPLE_MARKER;
}

int AudioCDView::sampleMarker(unsigned long *sample) const
{
  if (sampleMarkerValid_)
    *sample = sampleMarker_;

  return sampleMarkerValid_;
}

void AudioCDView::sampleSelection(unsigned long smin, unsigned long smax)
{
  unsigned long tmp;

  if (smin > smax) {
    tmp = smin;
    smin = smax;
    smax = tmp;
  }

  if (smax < tocEdit_->toc()->length().samples()) {
    sampleSelectionMin_ = smin;
    sampleSelectionMax_ = smax;

    sampleSelectionValid_ = 1;
  }
  else {
    sampleSelectionValid_ = 0;
  }
  
  updateLevel_ |= UPD_SAMPLE_SEL;
}

int AudioCDView::sampleSelection(unsigned long *smin, unsigned long *smax) const
{
  if (sampleSelectionValid_) {
    *smin = sampleSelectionMin_;
    *smax = sampleSelectionMax_;
  }

  return sampleSelectionValid_;
}

void AudioCDView::sampleView(unsigned long smin, unsigned long smax)
{
  if (smin <= smax && smax < tocEdit_->lengthSample()) {
    sampleViewMin_ = smin;
    sampleViewMax_ = smax;
    updateLevel_ |= UPD_SAMPLES;
  }
}

void AudioCDView::sampleView(unsigned long *smin, unsigned long *smax) const
{
  *smin = sampleViewMin_;
  *smax = sampleViewMax_;
}

void AudioCDView::sampleViewFull()
{
  sampleViewMin_ = 0;

  if ((sampleViewMax_ = tocEdit_->lengthSample()) > 0)
    sampleViewMax_ -= 1;

  updateLevel_ |= UPD_SAMPLES;
}

void AudioCDView::sampleViewInclude(unsigned long smin, unsigned long smax)
{
  if (smin < sampleViewMin_) {
    sampleViewMin_ = smin;
    updateLevel_ |= UPD_SAMPLES;
  }

  if (smax < tocEdit_->lengthSample() && smax > sampleViewMax_) {
    sampleViewMax_ = smax;
    updateLevel_ |= UPD_SAMPLES;
  }
}

void AudioCDView::trackSelection(int tnum)
{
  if (tnum > 0) {
    trackSelection_ = tnum;
    trackSelectionValid_ = 1;
  }
  else {
    trackSelectionValid_ = 0;
  }

  updateLevel_ |= UPD_TRACK_MARK_SEL;

}

int AudioCDView::trackSelection(int *tnum) const
{
  if (trackSelectionValid_)
    *tnum = trackSelection_;

  return trackSelectionValid_;
}

void AudioCDView::indexSelection(int inum)
{
  if (inum >= 0) {
    indexSelection_ = inum;
    indexSelectionValid_ = 1;
  }
  else {
    indexSelectionValid_ = 0;
  }

  updateLevel_ |= UPD_TRACK_MARK_SEL;
}

int AudioCDView::indexSelection(int *inum) const
{
  if (indexSelectionValid_)
    *inum = indexSelection_;

  return indexSelectionValid_;
}

void AudioCDView::drag_data_received_cb(GdkDragContext *context,
  gint x, gint y, GtkSelectionData *selection_data, guint info, guint time)
{
  GList *names;
  char *file;
  
  switch (info) {
    case TARGET_URI_LIST:
      names = (GList *)gnome_uri_list_extract_filenames \
			((char *)selection_data->data);  

//  	tocEdit_->blockEdit();
//FIXME      while (names->data) {
      if (names->data) {
          string str = g_strdup(static_cast <char *>(names->data));
          const char *file = stripCwd(str.c_str());

        switch (tocEdit_->appendTrack(file)) {
        case 0:
	      guiUpdate();
	      MDI_WINDOW->statusMessage("Appended track with audio data from \"%s\".", file);
	      break;
        case 1:
	      MDI_WINDOW->statusMessage("Cannot open audio file \"%s\".", file);
	      break;
        case 2:
	      MDI_WINDOW->statusMessage("Audio file \"%s\" has wrong format.", file);
	      break;
	    }
	    names = g_list_remove(names, names->data);
      }
//	tocEdit_->unblockEdit();
    break;
  }
}
