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

#ifndef __RECORD_GENERIC_DIALOG_H
#define __RECORD_GENERIC_DIALOG_H

#include <gtk--.h>
#include <gtk/gtk.h>

#include <gnome--.h>

class TocEdit;
class RecordTocSource;
class RecordCDSource;
class RecordCDTarget;
class RecordHDTarget;

class RecordGenericDialog : public Gtk::Window {
public:
  enum RecordSourceType { S_NONE, S_TOC, S_CD };
  enum RecordTargetType { T_NONE, T_CD, T_HD };

  RecordGenericDialog();
  ~RecordGenericDialog();

  void start(TocEdit *, RecordSourceType SourceType,
	     RecordTargetType TargetType);
  void stop();

  void toc_to_cd(TocEdit *);
  void cd_to_cd();
  void cd_to_hd();

  void update(unsigned long level, TocEdit *);

  void cancelAction();
  void startAction();
  void help();

  gint delete_event_impl(GdkEventAny*);

private:
  int active_;

  RecordTocSource *TOCSOURCE;
  RecordCDSource *CDSOURCE;
  RecordCDTarget *CDTARGET;
  RecordHDTarget *HDTARGET;

  RecordSourceType source_;
  RecordTargetType target_;
};

#endif
