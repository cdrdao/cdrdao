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

#include <gnome--.h>

#include "RecordGenericDialog.h"

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

  active_ = 0;

  CDTARGET = new RecordCDTarget;

  set_title(string("Record Project, CD duplication, etc ..."));
//  set_usize(0, 400);
  set_border_width(5);

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
  main_hbox->pack_start(*vseparator, FALSE, FALSE);
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


//  startButton = manage( new Gtk::Button ("Accept"));
  startButton = new Gnome::Stock::Buttons::Button(GNOME_STOCK_BUTTON_OK);
  buttonbox->pack_start(*startButton);
  startButton->show();

//  closeButton = manage( new Gtk::Button ("Cancel"));
  closeButton = new Gnome::Stock::Buttons::Button(GNOME_STOCK_BUTTON_CLOSE);
  buttonbox->add (*closeButton);
  closeButton->show();

//  helpButton = manage( new Gtk::Button ("Help"));
  helpButton = new Gnome::Stock::Buttons::Button(GNOME_STOCK_BUTTON_HELP);
  buttonbox->add (*helpButton);
  helpButton->show();

  main_vbox->show();

  right_vbox->pack_start(*CDTARGET, TRUE, TRUE);
  CDTARGET->show();

  startButton->clicked.connect(SigC::slot(this,&RecordGenericDialog::startAction));
  closeButton->clicked.connect(SigC::slot(this,&RecordGenericDialog::cancelAction));
  helpButton->clicked.connect(SigC::slot(this,&RecordGenericDialog::help));

}

RecordGenericDialog::~RecordGenericDialog()
{
}


void RecordGenericDialog::start(TocEdit *tocEdit, enum RecordTargetType TargetType)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

// case or if stament
  CDTARGET->start(tocEdit);

  show();
}

void RecordGenericDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
// case or if stament
  CDTARGET->stop();
}

void RecordGenericDialog::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

// case or if stament
  CDTARGET->update(level, tocEdit);
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
// case or if stament
  CDTARGET->startAction();
}

void RecordGenericDialog::help()
{
}
