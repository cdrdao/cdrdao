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
 * Revision 1.10  2000/10/08 16:39:41  andreasm
 * Remote progress message now always contain the track relative and total
 * progress and the total number of processed tracks.
 *
 * Revision 1.9  2000/09/21 02:07:06  llanero
 * MDI support:
 * Splitted AudioCDChild into same and AudioCDView
 * Move Selections from TocEdit to AudioCDView to allow
 *   multiple selections.
 * Cursor animation in all the views.
 * Can load more than one from from command line
 * Track info, Toc info, Append/Insert Silence, Append/Insert Track,
 *   they all are built for every child when needed.
 * ...
 *
 * Revision 1.7  2000/08/01 01:27:50  llanero
 * CD to CD copy works now.
 *
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

static char rcsid[] = "$Id: RecordProgressDialog.cc,v 1.10 2000/10/08 16:39:41 andreasm Exp $";

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include <gnome--.h>

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
  totalProgress_ = new Gtk::ProgressBar;
  totalProgress_->set_show_text(TRUE);
  bufferFillRate_ = new Gtk::ProgressBar;
  bufferFillRate_->set_show_text(TRUE);
  tocName_ = new Gtk::Label;

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

  hbox = new Gtk::HBox(TRUE, TRUE);
  label = new Gtk::Label(string("Elapsed Time: "), 1);
  hbox->pack_start(*label, FALSE);
  label->show();
  currentTime_ = new Gtk::Label(string(""), 0);
  hbox->pack_start(*currentTime_, FALSE);
  currentTime_->show();
  label = new Gtk::Label(string("Remaining Time: "), 1);
  hbox->pack_start(*label, FALSE);
//  label->show();
  remainingTime_ = new Gtk::Label(string(""), 0);
  hbox->pack_start(*remainingTime_, FALSE);
//  remainingTime_->show();
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

  cancelButton_ = new Gnome::StockButton(GNOME_STOCK_BUTTON_CANCEL);
  bbox->pack_start(*cancelButton_);

  closeButton_ = new Gnome::StockButton(GNOME_STOCK_BUTTON_CLOSE);
  bbox->pack_start(*closeButton_);

  cancelButton_->show();
  actCloseButtonLabel_ = 2;

  cancelButton_->clicked.connect(SigC::slot(this,&RecordProgressDialog::closeAction));
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
  gint m_t_nr;

  if (device == NULL)
    return;

  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;
  device_ = device;

  clear();

  SigC::Slot0<gint> my_slot = bind(slot(this,&RecordProgressDialog::time),m_t_nr);
  Gtk::Connection conn = Gtk::Main::timeout.connect(my_slot, 1000);

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
  gint m_t_nr;

  if (device == NULL)
    return;

  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;
  device_ = device;

  clear();

  SigC::Slot0<gint> my_slot = bind(slot(this,&RecordProgressDialog::time),m_t_nr);
  Gtk::Connection conn = Gtk::Main::timeout.connect(my_slot, 1000);

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
    case CdDevice::A_DUPLICATE:
        {        
        Ask2Box msg(this, "Abort Process", 0, 2,
		    "Abort duplicating process?", NULL);

        if (msg.run() == 1) {
          if (device_ != NULL) 
	        device_->abortDaoDuplication();
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

  gettimeofday(&time_, NULL);
  currentTime_->set(string("0:00:00"));
  remainingTime_->set(string(""));
  leadTimeFilled_ = FALSE;
  statusMsg_->set_text(string(""));
  trackProgress_->set_percentage(0.0);
  trackProgress_->set_format_string("");
  totalProgress_->set_percentage(0.0);
  totalProgress_->set_format_string("");
  bufferFillRate_->set_percentage(0.0);
  bufferFillRate_->set_format_string("");
  
  set_title(string(""));
}

void RecordProgressDialog::update(unsigned long level)
{
  int status;
  int totalTracks;
  int track;
  int trackProgress;
  int totalProgress;
  int bufferFill;
  char buf[40];
  char bufProgress[30];
  string s;

  if (!active_ || device_ == NULL)
    return;
  if (finished_)
    return;

  switch (device_->action()) {
    case CdDevice::A_RECORD:
    if ((level & UPD_PROGRESS_STATUS) && device_->progressStatusChanged()) {
      device_->recordProgress(&status, &totalTracks, &track, &trackProgress,
			      &totalProgress, &bufferFill);

      if (status != actStatus_) {
        actStatus_ = status;

        switch (status) {
          case 1:
            statusMsg_->set_text(string("Writing lead-in..."));
  	      break;
          case 2:
	        actTrack_ = track;

        	s = "Writing track ";
        	sprintf(buf, "%d of %d", track, totalTracks);
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
        sprintf(buf, "%d of %d", track, totalTracks);
        s += buf;

        statusMsg_->set_text(s);
      }

      if (trackProgress != actTrackProgress_) {
        actTrackProgress_ = trackProgress;

        trackProgress_->set_percentage(gfloat(trackProgress) / 1000.0);
        sprintf(bufProgress, "%.2f %%%%", gfloat(trackProgress/10.0));
        trackProgress_->set_format_string(bufProgress);  
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
      device_->readProgress(&status, &totalTracks, &track, &trackProgress,
			    &totalProgress);

      if (status != actStatus_ || actTrack_ != track) {
        actStatus_ = status;

        switch (status) {
        case 1:
      	actTrack_ = track;

      	s = "Analyzing track ";
      	sprintf(buf, "%d of %d", track, totalTracks);
      	s += buf;

      	statusMsg_->set_text(s);
      	break;
        case 2:
      	actTrack_ = track;

      	s = "Extracting track ";
	      sprintf(buf, "%d of %d", track, totalTracks);
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

      if (totalProgress != actTotalProgress_) {
        actTotalProgress_ = totalProgress;

        totalProgress_->set_percentage(gfloat(totalProgress) / 1000.0);
        sprintf(bufProgress, "%.2f %%%%", gfloat(totalProgress/10.0));
        totalProgress_->set_format_string(bufProgress);
      }
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
  
  case CdDevice::A_DUPLICATE:
    if ((level & UPD_PROGRESS_STATUS) && device_->progressStatusChanged()) {
      device_->recordProgress(&status, &totalTracks, &track, &trackProgress,
			      &totalProgress, &bufferFill);

//g_print("Message received!\n");

      if (status != actStatus_) {
        actStatus_ = status;

        switch (status) {
          case 1:
            statusMsg_->set_text(string("Writing lead-in..."));
  	      break;
          case 2:
	        actTrack_ = track;

        	s = "Writing track ";
        	sprintf(buf, "%d of %d", track, totalTracks);
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
        sprintf(buf, "%d of %d", track, totalTracks);
        s += buf;

        statusMsg_->set_text(s);
      }

      if (trackProgress != actTrackProgress_) {
        actTrackProgress_ = trackProgress;

        trackProgress_->set_percentage(gfloat(trackProgress) / 1000.0);
        sprintf(bufProgress, "%.2f %%%%", gfloat(trackProgress/10.0));
        trackProgress_->set_format_string(bufProgress);  
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
  default:
    break;
  }
}

// Sets label of close button.
// l: 1: 'abort'	--> CANCEL gnome stock button (i18n)
//    2: 'dismiss'  --> CLOSE  gnome stock button (i18n)
void RecordProgressDialog::setCloseButtonLabel(int l)
{
  if (actCloseButtonLabel_ == l)
    return;

  switch (l) {
  case 1:
	  closeButton_->hide();
	  cancelButton_->show();
    break;
  case 2:
	  cancelButton_->hide();
	  closeButton_->show();
    break;
  }

  actCloseButtonLabel_ = l;
}

gint RecordProgressDialog::time(gint timer_nr)
{
  char buf[50];
  struct timeval timenow;
  long time, time_remain, hours, mins, secs;

  gettimeofday(&timenow, NULL);

  time = timenow.tv_sec - time_.tv_sec;

  hours = time / 3600;
  mins = (time - (hours * 3600)) / 60;
  secs = time - ((hours * 3600) + (mins * 60));

  sprintf(buf, "%ld:%02ld:%02ld", hours, mins, secs);
  currentTime_->set(string(buf));


  if (actTotalProgress_ > 10)
  {
//Hack!
    gfloat aux1, aux2, aux3;

    if (!leadTimeFilled_)
    {
      leadTime_ = time;
      leadTimeFilled_ = TRUE;
    }
//    time_remain = (long)((float)((float)(time + 5 - leadTime_) / actTotalProgress_) * (1000 - actTotalProgress_));
//    time_remain = (time + 5 - leadTime_) * ((1000 - actTotalProgress_) / actTotalProgress_);
    aux1 = (gfloat)actTotalProgress_;
    aux2 = (1000 - aux1);
    aux3 = (aux2 * (time + 20 - leadTime_)) / aux1;
	time_remain = (long)aux3;

    hours = time_remain / 3600;
    mins = (time_remain - (hours * 3600)) / 60;
    secs = time_remain - ((hours * 3600) + (mins * 60));

    sprintf(buf, "%ld:%02ld:%02ld", hours, mins, secs);
    remainingTime_->set(string(buf));
  }

  if (finished_) 
  {
    return 0;
  }
  else
  {
    return 1;
  }
}



RecordProgressDialogPool::RecordProgressDialogPool()
{
  activeDialogs_ = NULL;
  pool_ = NULL;
}

RecordProgressDialogPool::~RecordProgressDialogPool()
{

}

void RecordProgressDialogPool::update(unsigned long status)
{
  RecordProgressDialog *run;

  for (run = activeDialogs_; run != NULL; run = run->poolNext_)
    run->update(status);
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
