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

#ifndef __MDI_WINDOW_H__
#define __MDI_WINDOW_H__

#include <gtk--.h>
#include <gtk/gtk.h>

#include <gnome--.h>

class TocEdit;
GtkWidget* example_creator(GnomeMDIChild *child, gpointer data);

//FIXME: Until we get gnome-- MDI support!
//class MDIWindow : public Gnome::MDI
class MDIWindow : public Gnome::App
{
public:
  MDIWindow(TocEdit *);
  void example_child();
//  GtkWidget* example_creator(GnomeMDIChild *child, gpointer data);

protected:
  void install_menus_and_toolbar();
  void app_close();
  virtual int delete_event_impl(GdkEventAny *event);
  void nothing_cb();  
  void about_cb();
  void about_destroy_cb();

private:
  TocEdit *tocEdit_; // this should be a list of TocEdit objects

  Gtk::Statusbar *statusBar_;
  class AudioCDChild *audioCdChild_;

  Gtk::FileSelection readSaveFileSelector_;
  int readSaveOperation_; // 1 for read, 2 for save

  void tocBlockedMsg(const char *);

  void readWriteFileSelectorOKCB();
  void readWriteFileSelectorCancelCB();
  void newProject();
  void readProject();
  void saveProject();
  void saveAsProject();

//llanero  void quit();
  void configureDevices();

  void recordToc2CD();
  void recordCD2HD();
  void recordCD2CD();
  void projectInfo();
  void trackInfo();

  Gnome::About *about_;

public:
  TocEdit *tocEdit() const { return tocEdit_; }

 void statusMessage(const char *fmt, ...);

  void update(unsigned long level);
//  gint delete_event_impl(GdkEventAny*);

};
#endif
