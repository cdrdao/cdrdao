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

//#include <stdio.h>
//#include <limits.h>
//#include <math.h>
//#include <assert.h>

//#include <gnome.h>

#include "RecordGenericDialog.h"
//#include "MessageBox.h"
//#include "xcdrdao.h"
//#include "Settings.h"

//#include "CdDevice.h"
//#include "guiUpdate.h"
//#include "TocEdit.h"

//#include "util.h"

#include <gnome--.h>

RecordGenericDialog::RecordGenericDialog()
{
  Gtk::VBox *main_vbox;
  Gtk::HBox *main_hbox;
  Gtk::VBox *left_vbox;
  Gtk::VSeparator *vseparator;
  Gtk::VBox *right_vbox;
  Gtk::HSeparator *hseparator;
  Gtk::HButtonBox *buttonbox;
  Gtk::Button *startButton;
  Gtk::Button *closeButton;
  Gtk::Button *helpButton;

  set_title(string("CD duplication, etc ..."));
  set_usize(0, 400);

//  main_vbox = get_vbox();
  main_vbox = new Gtk::VBox;
  add (*main_vbox);
  main_vbox->show();

  main_hbox = new Gtk::HBox;
  main_vbox->pack_start(*main_hbox);
  main_hbox->show();

  left_vbox = new Gtk::VBox;
  main_hbox->pack_start(*left_vbox);
  left_vbox->show();

  vseparator = new Gtk::VSeparator;
  main_hbox->pack_start(*vseparator);
  vseparator->show();
  
  right_vbox = new Gtk::VBox;
  main_hbox->pack_start(*right_vbox);
  right_vbox->show();

  hseparator = new Gtk::HSeparator;
  main_vbox->pack_start(*hseparator, FALSE, FALSE);
  hseparator->show();
  
  buttonbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);
  main_vbox->pack_start(*buttonbox, FALSE, FALSE, 5);
  buttonbox->show();


//  startButton = manage( new Gtk::Button ("Start"));
  startButton = Gnome::StockPixmap::pixmap_button(
  			*manage(Gnome::StockPixmap::pixmap_widget(*this,
  			GNOME_STOCK_BUTTON_YES)),
  			"Start");
  buttonbox->pack_start(*startButton);
  startButton->show();

//  closeButton = manage( new Gtk::Button ("Close"));
  closeButton = Gnome::StockPixmap::pixmap_button(
  			*manage(Gnome::StockPixmap::pixmap_widget(*this,
  			GNOME_STOCK_BUTTON_CLOSE)),
  			"Close");
  buttonbox->add (*closeButton);
  closeButton->show();

//  helpButton = manage( new Gtk::Button ("Help"));
  helpButton = Gnome::StockPixmap::pixmap_button(
  			*manage(Gnome::StockPixmap::pixmap_widget(*this,
  			GNOME_STOCK_BUTTON_HELP)),
  			"Help");
  buttonbox->add (*helpButton);
  helpButton->show();

  main_vbox->show();

/*    
  startButton->clicked.connect(SigC::slot(this,&RecordDialog::startAction));

  contents = new Gtk::VBox;
  contents->set_spacing(10);
*/

}

RecordGenericDialog::~RecordGenericDialog()
{
/*
  delete startButton_;
  startButton_ = NULL;
*/
}


void RecordGenericDialog::start()
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  show();
}

void RecordGenericDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordGenericDialog::update(unsigned long level)
{
  if (!active_)
    return;
}


gint RecordGenericDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void RecordGenericDialog::cancelAction()
{
  stop();
}

void RecordGenericDialog::startAction()
{
}
