/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __PROJECT_H__
#define __PROJECT_H__

#include <libgnomeuimm.h>

class RecordTocDialog;
class TocEdit;

class Project : public Gtk::VBox
{
public:
  Project(Gtk::Window* parent);

  void         readToc(char *name);
  void         statusMessage(const char *fmt, ...);
  void         tocBlockedMsg(const char *);
  virtual bool closeProject() = 0; 
  virtual void saveProject();
  virtual void saveAsProject();
  virtual void recordToc2CD() = 0;
  int          projectNumber();
  TocEdit*     tocEdit();

  Gtk::Window* getParentWindow() { return parent_; };

  virtual void update(unsigned long level) = 0;

protected:
  int  projectNumber_;
  bool new_; // If it is a new project (not saved)

  Gtk::Window*            parent_;
  TocEdit*                tocEdit_;
  RecordTocDialog*        recordTocDialog_;
  Gnome::UI::AppBar*      statusbar_;
  Gtk::ProgressBar*       progressbar_;  
  Gtk::Button*            progressButton_;  
  Gtk::FileChooserDialog* saveFileSelector_;

  void updateWindowTitle();
  virtual void projectInfo() = 0;
};
#endif
