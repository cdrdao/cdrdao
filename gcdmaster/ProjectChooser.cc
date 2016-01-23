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

#include <gtkmm.h>
#include <iostream>

#include "xcdrdao.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "ProjectChooser.h"
#include "Icons.h"

#define ICON_PADDING 10
#define LABEL_PADDING 10
#define BUTTONS_RELIEF Gtk::RELIEF_NORMAL

ProjectChooser::ProjectChooser()
{
  Glib::RefPtr<Gtk::Builder> builder;
  try
  {
	  builder = Gtk::Builder::create_from_file(CDRDAO_GLADEDIR "/ProjectChooser.glade", "mainbox");
  }
  catch(const Glib::Exception& ex)
  {
    try
    {
    	builder = Gtk::Builder::create_from_file("glade/ProjectChooser.glade", "mainbox");
    }
    catch(const Glib::Exception& ex)
    {
      std::cerr << ex.what() << std::endl;
      return;
    }
  }

  Gtk::Box* pBox = 0;
  builder->get_widget("mainbox", pBox);
  if(!pBox)
    return;

  Gtk::Image* pImage = 0;
  builder->get_widget("AudioImage", pImage);
  if (pImage)
  {
    pImage->set(Icons::AUDIOCD, Gtk::ICON_SIZE_DIALOG);
  }
  builder->get_widget("CopyImage", pImage);
  if (pImage)
  {
    pImage->set(Icons::COPYCD, Gtk::ICON_SIZE_DIALOG);
  }
  builder->get_widget("DumpImage", pImage);
  if (pImage)
  {
    pImage->set(Icons::DUMPCD, Gtk::ICON_SIZE_DIALOG);
  }

	Gtk::Button* pButton = 0;
	builder->get_widget("AudioButton", pButton);
	if (pButton) {
		pButton->signal_clicked().connect(ProjectChooser::newAudioCDProject);
	}
	builder->get_widget("CopyButton", pButton);
	if (pButton) {
		pButton->signal_clicked().connect(
				ProjectChooser::newDuplicateCDProject);
	}
	builder->get_widget("DumpButton", pButton);
	if (pButton) {
		pButton->signal_clicked().connect(ProjectChooser::newDumpCDProject);
	}

  pack_start(*pBox);
}
