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

#ifndef __PROGRESS_DIALOG_H__
#define __PROGRESS_DIALOG_H__

#include <sys/time.h>
#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;
class CdDevice;

class ProgressDialogPool;

class ProgressDialog : public Gtk::Dialog {
public:
  ProgressDialog(ProgressDialogPool *father);
  ~ProgressDialog();

  gint delete_event_impl(GdkEventAny*);

private:
  friend class ProgressDialogPool;

  ProgressDialogPool *poolFather_;

  int active_;
  CdDevice *device_;

  int finished_;
  int actStatus_;
  int actTrack_;
  int actTrackProgress_;
  int actTotalProgress_;
  int actBufferFill_;

  int actCloseButtonLabel_;

  Gtk::Label *currentTime_;
  Gtk::Label *remainingTime_;

  long leadTime_;
  bool leadTimeFilled_;

  struct timeval time_;
  gint ProgressDialog::time(gint timer_nr);

  Gtk::Button *cancelButton_;
  Gtk::Button *closeButton_;
  Gtk::Label *tocName_;

  Gtk::Label *statusMsg_;;
  Gtk::ProgressBar *trackProgress_;
  Gtk::ProgressBar *totalProgress_;
  Gtk::ProgressBar *bufferFillRate_;

  ProgressDialog *poolNext_;

  void update(unsigned long);
  void start(CdDevice *, const char *tocFileName);
  void stop();
  void closeAction();
  void clear();
  void setCloseButtonLabel(int l);

};

class ProgressDialogPool {
public:
  ProgressDialogPool();
  ~ProgressDialogPool();

  void update(unsigned long);
  
  ProgressDialog *start(CdDevice *, TocEdit *);
  ProgressDialog *start(CdDevice *, const char *tocFileName);
  void stop(ProgressDialog *);

private:
  ProgressDialog *activeDialogs_;
  ProgressDialog *pool_;
};


#endif
