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
 * $Log: RecordProgressDialog.cc,v $
 * Revision 1.6  2000/07/31 01:55:49  llanero
 * got rid of old Extract dialog and Record dialog.
 * both are using RecordProgressDialog now.
 *
 * Revision 1.5  2000/05/24 18:42:44  llanero
 * added % to progressbars
 *
 * Revision 1.4  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.3  2000/04/16 20:31:20  andreasm
 * Added missing stdio.h includes.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:36  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 */

static char rcsid[] = "$Id: RecordProgressDialog.cc,v 1.6 2000/07/31 01:55:49 llanero Exp $";

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "RecordProgressDialog.h"
#include "MessageBox.h"
#include "TocEdit.h"
#include "guiUpdate.h"
#include "CdDevice.h"


RecordProgressDialog::RecordProgressDialog(RecordProgressDialogPool *father)
{
  Gtk::Label *label;
  Gtk::HBox *hbox;
  Gtk::VBox *contents = new Gtk::VBox;
  Gtk::Table *table;
  Gtk::Alignment *align;

  poolFather_ = father;
  active_ = 0;
  device_ = NULL;
  poolNext_ = NULL;

  contents->set_spacing(5);

  statusMsg_ = new Gtk::Label(string("XXXXXXXXXXXXXXXXXXX"));
  trackProgress_ = new Gtk::ProgressBar;
  trackProgress_->set_show_text(TRUE);
//  trackProgress_->set_format_string("Starting...");    
  totalProgress_ = new Gtk::ProgressBar;
  totalProgress_->set_show_text(TRUE);
//  totalProgress_->set_format_string("Starting...");    
  bufferFillRate_ = new Gtk::ProgressBar;
  bufferFillRate_->set_show_text(TRUE);
//  bufferFillRate_->set_format_string("Starting...");    
  tocName_ = new Gtk::Label;
  abortLabel_ = new Gtk::Label(string(" Abort "));
  closeLabel_ = new Gtk::Label(string(" Dismiss "));
  closeButton_ = new Gtk::Button;
  closeButton_->add(*closeLabel_);
  closeLabel_->show();
  abortLabel_->show();
  actCloseButtonLabel_ = 2;

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Project: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  hbox->pack_start(*tocName_, FALSE);
  tocName_->show();
  contents->pack_start(*hbox, FALSE);
  hbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*statusMsg_, FALSE);
  statusMsg_->show();
  contents->pack_start(*hbox, FALSE);
  hbox->show();

  table = new Gtk::Table(3, 2, FALSE);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  contents->pack_start(*table, FALSE);
  table->show();

  label = new Gtk::Label(string("Track:"));
  align = new Gtk::Alignment(1.0, 0.0, 0.0, 0.0);
  align->add(*label);
  label->show();
  table->attach(*align, 0, 1, 0, 1, GTK_FILL);
  align->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*trackProgress_);
  trackProgress_->show();
  table->attach(*hbox, 1, 2, 0, 1);
  hbox->show();

  label = new Gtk::Label(string("Total:"));
  align = new Gtk::Alignment(1.0, 0.0, 0.0, 0.0);
  align->add(*label);
  label->show();
  table->attach(*align, 0, 1, 1, 2, GTK_FILL);
  align->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*totalProgress_);
  totalProgress_->show();
  table->attach(*hbox, 1, 2, 1, 2);
  hbox->show();

  label = new Gtk::Label(string("Buffer:"));
  align = new Gtk::Alignment(1.0, 0.0, 0.0, 0.0);
  align->add(*label);
  label->show();
  table->attach(*align, 0, 1, 2, 3, GTK_FILL);
  align->show();
  
  hbox = new Gtk::HBox;
  hbox->pack_start(*bufferFillRate_);
  bufferFillRate_->show();
  table->attach(*hbox, 1, 2, 2, 3);
  hbox->show();
  
  hbox = new Gtk::HBox;
  hbox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();
  get_vbox()->pack_start(*hbox, FALSE, FALSE, 10);
  hbox->show();

  get_vbox()->show();

  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);

  bbox->pack_start(*closeButton_);
  closeButton_->show();
  closeButton_->clicked.connect(SigC::slot(this,&RecordProgressDialog::closeAction));

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();

  set_usize(400, 0);
}

RecordProgressDialog::~RecordProgressDialog()
{
}

void RecordProgressDialog::start(CdDevice *device, TocEdit *tocEdit)
{
  string s;

  if (device == NULL)
    return;

  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;
  device_ = device;

  clear();

  statusMsg_->set_text(string("Initializing..."));
  tocName_->set_text(string(tocEdit->filename()));

  setCloseButtonLabel(1);

  s = device->vendor();
  s += " ";
  s += device->product();

  set_title(s);
  
  show();
}

void RecordProgressDialog::start(CdDevice *device, char *tocFileName)
{
  string s;

  if (device == NULL)
    return;

  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;
  device_ = device;

  clear();

  statusMsg_->set_text(string("Initializing..."));
  tocName_->set_text(string(tocFileName));

  setCloseButtonLabel(1);

  s = device->vendor();
  s += " ";
  s += device->product();

  set_title(s);
  
  show();
}

void RecordProgressDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
    device_ = NULL;
  }
}

gint RecordProgressDialog::delete_event_impl(GdkEventAny*)
{
  if (finished_) {
    poolFather_->stop(this);
  }
  return 1;
}

void RecordProgressDialog::closeAction()
{
  if (finished_) {
    poolFather_->stop(this);
  }
  else {
    switch (device_->action()) {
    case CdDevice::A_RECORD:
        {
        Ask2Box msg(this, "Abort Recording", 0, 2,
		  "Abort recording process?", NULL);

  	  if (msg.run() == 1) {
          if (device_ != NULL) 
            device_->abortDaoRecording();
        }
        }
        break;

    case CdDevice::A_READ:
        {        
        Ask2Box msg(this, "Abort Reading", 0, 2,
		    "Abort reading process?", NULL);

        if (msg.run() == 1) {
          if (device_ != NULL) 
	        device_->abortDaoReading();
        }
        }
		break;
	default:
	    break;
    }
  }
}


void RecordProgressDialog::clear()
{
  finished_ = 0;
  actStatus_ = 0;
  actTrack_ = 0;
  actTrackProgress_ = 0;
  actTotalProgress_ = 0;
  actBufferFill_ = 0;

  statusMsg_->set_text(string(""));
  trackProgress_->set_percentage(0.0);
  trackProgress_->set_format_string("");
  totalProgress_->set_percentage(0.0);
  totalProgress_->set_format_string("");
  bufferFillRate_->set_percentage(0.0);
  bufferFillRate_->set_format_string("");
  
  set_title(string(""));
}

void RecordProgressDialog::update(unsigned long level, TocEdit *tocEdit)
{
  int status;
  int track;
  int trackProgress;
  int totalProgress;
  int bufferFill;
  char buf[20];
  char bufProgress[30];
  string s;

  if (!active_ || device_ == NULL)
    return;

  if (finished_)
    return;

  switch (device_->action()) {
    case CdDevice::A_RECORD:
    if ((level & UPD_PROGRESS_STATUS) && device_->progressStatusChanged()) {
      device_->recordProgress(&status, &track, &totalProgress, &bufferFill);

      if (status != actStatus_) {
        actStatus_ = status;

        switch (status) {
          case 1:
            statusMsg_->set_text(string("Writing lead-in..."));
  	      break;
          case 2:
	        actTrack_ = track;

        	s = "Writing track ";
        	sprintf(buf, "%d", track);
	        s += buf;

        	statusMsg_->set_text(s);
    	    break;
          case 3:
    	    statusMsg_->set_text(string("Writing lead-out..."));
	        break;
        }
      }

      if (track != actTrack_ && status == 2) {
        actTrack_ = track;

        s = "Writing track ";
        sprintf(buf, "%d", track);
        s += buf;

        statusMsg_->set_text(s);
      }

      if (totalProgress != actTotalProgress_) {
        actTotalProgress_ = totalProgress;

        totalProgress_->set_percentage(gfloat(totalProgress) / 1000.0);
        sprintf(bufProgress, "%.2f %%%%", gfloat(totalProgress/10.0));
        totalProgress_->set_format_string(bufProgress);
      }

      if (bufferFill != actBufferFill_) {
        actBufferFill_ = bufferFill;

        bufferFillRate_->set_percentage(gfloat(bufferFill) / 100.0);
        sprintf(bufProgress, "%.2f %%%%", gfloat(bufferFill/1.0));
        bufferFillRate_->set_format_string(bufProgress);
      }
    }

    if (device_->status() != CdDevice::DEV_RECORDING) {
      switch (device_->exitStatus()) {
      case 0:
        statusMsg_->set_text(string("Recording finished successfully."));
        break;

      case 255:
        statusMsg_->set_text(string("Cannot execute cdrdao. Please check your PATH."));
        break;

      default:
        statusMsg_->set_text(string("Recording aborted with error."));
        break;
      }

      finished_ = 1;

      setCloseButtonLabel(2);
    }
    break;

  case CdDevice::A_READ:
    if ((level & UPD_PROGRESS_STATUS) && device_->progressStatusChanged()) {
      device_->readProgress(&status, &track, &trackProgress);

      if (status != actStatus_ || actTrack_ != track) {
        actStatus_ = status;

        switch (status) {
        case 1:
      	actTrack_ = track;

      	s = "Analyzing track ";
      	sprintf(buf, "%d", track);
      	s += buf;

      	statusMsg_->set_text(s);
      	break;
        case 2:
      	actTrack_ = track;

      	s = "Extracting track ";
	      sprintf(buf, "%d", track);
	      s += buf;

      	statusMsg_->set_text(s);
	      break;
        }
      }

      if (trackProgress != actTrackProgress_) {
        actTrackProgress_ = trackProgress;

        trackProgress_->set_percentage(gfloat(trackProgress) / 1000.0);
        sprintf(bufProgress, "%.2f %%%%", gfloat(trackProgress/10.0));
        trackProgress_->set_format_string(bufProgress);  
      }

    /*
    if (bufferFill != actBufferFill_) {
      actBufferFill_ = bufferFill;

      bufferFillRate_->set_percentage(gfloat(bufferFill) / 100.0);
    }
    */
    }

    if (device_->status() != CdDevice::DEV_READING) {
      switch (device_->exitStatus()) {
      case 0:
        statusMsg_->set_text(string("Reading finished successfully."));
        break;

      case 255:
        statusMsg_->set_text(string("Cannot execute cdrdao. Please check your PATH."));
        break;

      default:
        statusMsg_->set_text(string("Reading aborted with error."));
        break;
      }

      finished_ = 1;

      setCloseButtonLabel(2);
    }

    break;
  
  default:
    break;
  }
}

// Sets label of close button.
// l: 1: 'abort'
//    2: 'dismiss'
void RecordProgressDialog::setCloseButtonLabel(int l)
{
  if (actCloseButtonLabel_ == l)
    return;

  closeButton_->remove();

  switch (l) {
  case 1:
    closeButton_->add(*abortLabel_);
    break;
  case 2:
    closeButton_->add(*closeLabel_);
    break;
  }

  actCloseButtonLabel_ = l;
}



RecordProgressDialogPool::RecordProgressDialogPool()
{
  activeDialogs_ = NULL;
  pool_ = NULL;
}

RecordProgressDialogPool::~RecordProgressDialogPool()
{

}

void RecordProgressDialogPool::update(unsigned long status, TocEdit *tocEdit)
{
  RecordProgressDialog *run;

  for (run = activeDialogs_; run != NULL; run = run->poolNext_)
    run->update(status, tocEdit);
}
  
RecordProgressDialog *RecordProgressDialogPool::start(CdDevice *device,
						      TocEdit *tocEdit)
{
  RecordProgressDialog *dialog;

  if (pool_ == NULL) {
    dialog = new RecordProgressDialog(this);
  }
  else {
    dialog = pool_;
    pool_ = pool_->poolNext_;
  }

  dialog->poolNext_ = activeDialogs_;
  activeDialogs_ = dialog;

  dialog->start(device, tocEdit);

  return dialog;
}

RecordProgressDialog *RecordProgressDialogPool::start(CdDevice *device, char *tocFileName)
{
  RecordProgressDialog *dialog;

  if (pool_ == NULL) {
    dialog = new RecordProgressDialog(this);
  }
  else {
    dialog = pool_;
    pool_ = pool_->poolNext_;
  }

  dialog->poolNext_ = activeDialogs_;
  activeDialogs_ = dialog;

  dialog->start(device, tocFileName);

  return dialog;
}


void RecordProgressDialogPool::stop(RecordProgressDialog *dialog)
{
  RecordProgressDialog *run, *pred;

  for (pred = NULL, run = activeDialogs_; run != NULL;
       pred = run, run = run->poolNext_) {
    if (run == dialog)
      break;
  }

  if (run == NULL)
    return;

  dialog->stop();

  if (pred == NULL)
    activeDialogs_ = activeDialogs_->poolNext_;
  else
    pred->poolNext_ = run->poolNext_;

  dialog->poolNext_ = pool_;
  pool_ = dialog;
}

