/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __EXTRACT_PROGRESS_DIALOG_H__
#define __EXTRACT_PROGRESS_DIALOG_H__

#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;
class CdDevice;

class ExtractProgressDialogPool;

class ExtractProgressDialog : public Gtk::Dialog {
public:
  ExtractProgressDialog(ExtractProgressDialogPool *father);
  ~ExtractProgressDialog();

  gint delete_event_impl(GdkEventAny*);

private:
  friend class ExtractProgressDialogPool;

  ExtractProgressDialogPool *poolFather_;

  int active_;
  CdDevice *device_;

  int finished_;
  int actStatus_;
  int actTrack_;
  int actTotalProgress_;
  int actBufferFill_;

  int actCloseButtonLabel_;

  Gtk::Button *closeButton_;
  Gtk::Label *abortLabel_;
  Gtk::Label *closeLabel_;
  Gtk::Label *tocName_;

  Gtk::Label *statusMsg_;;
  Gtk::ProgressBar *totalProgress_;
  Gtk::ProgressBar *bufferFillRate_;

  ExtractProgressDialog *poolNext_;

  void update(unsigned long, TocEdit *);
  void start(CdDevice *, char *tocFileName);
  void stop();
  void closeAction();
  void clear();
  void setCloseButtonLabel(int l);

};

class ExtractProgressDialogPool {
public:
  ExtractProgressDialogPool();
  ~ExtractProgressDialogPool();

  void update(unsigned long, TocEdit *);
  
  ExtractProgressDialog *start(CdDevice *, char *tocFileName);
  void stop(ExtractProgressDialog *);

private:
  ExtractProgressDialog *activeDialogs_;
  ExtractProgressDialog *pool_;
};


#endif
