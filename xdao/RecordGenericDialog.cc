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
#include "RecordTocSource.h"
#include "RecordCDSource.h"
#include "RecordCDTarget.h"
#include "RecordHDTarget.h"
#include "guiUpdate.h"

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

/*  Gnome::Dialog *testDialog;
  vector <string> buttons;
  Gtk::Button *button1;

//  button1 = new Gnome::Stock::Buttons::Button(GNOME_STOCK_BUTTON_OK);
//  buttons.push_back(button1);
//  button1->show();
  buttons.push_back("GNOME_STOCK_BUTTON_OK");

  testDialog = new Gnome::Dialog("title ...", buttons);

  testDialog->show();
*/

  active_ = 0;

  TOCSOURCE = new RecordTocSource;
  CDSOURCE = new RecordCDSource;
  CDTARGET = new RecordCDTarget;
  HDTARGET = new RecordHDTarget;

//  TOCSOURCE->parent = this;
//  CDSOURCE->parent = this;
  CDTARGET->parent = this;
  HDTARGET->parent = this;

  set_title(string("Record Project, CD duplication, etc ..."));
//  set_usize(0, 400);
  set_border_width(10);

//  main_vbox = get_vbox();
  main_vbox = new Gtk::VBox;
  add (*main_vbox);
  main_vbox->set_spacing(5);
  main_vbox->show();

  main_hbox = new Gtk::HBox;
  main_vbox->pack_start(*main_hbox, TRUE, TRUE, 5);
  main_hbox->set_spacing(8);
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
  
  buttonbox = new Gtk::HButtonBox(GTK_BUTTONBOX_END);
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

  left_vbox->pack_start(*TOCSOURCE, TRUE, TRUE);
  left_vbox->pack_start(*CDSOURCE, TRUE, TRUE);
  source_ = S_NONE;

  right_vbox->pack_start(*CDTARGET, TRUE, TRUE);
  right_vbox->pack_start(*HDTARGET, TRUE, TRUE);
  target_ = T_NONE;

  startButton->clicked.connect(SigC::slot(this,&RecordGenericDialog::startAction));
  closeButton->clicked.connect(SigC::slot(this,&RecordGenericDialog::cancelAction));
  helpButton->clicked.connect(SigC::slot(this,&RecordGenericDialog::help));

}

RecordGenericDialog::~RecordGenericDialog()
{
}


void RecordGenericDialog::start(TocEdit *tocEdit, enum RecordSourceType SourceType, enum RecordTargetType TargetType)
{
  if (active_) {
    get_window().raise();
//    return;
  }

  active_ = 1;

  if (SourceType != source_)
  {
    switch (source_) 
    {
	  case S_TOC:
                  TOCSOURCE->stop();
                  break;
      case S_CD:
                  CDSOURCE->stop();
                  break;
    }
    switch (SourceType)
    {
      case S_TOC:
                  TOCSOURCE->start(tocEdit);
                  break;
      case S_CD:
                  CDSOURCE->start(tocEdit);
                  break;                  
    }
    source_ = SourceType;
  }
  else
  {
    switch (source_) 
    {
	  case S_TOC:
                  TOCSOURCE->update(UPD_ALL, tocEdit);
                  break;
      case S_CD:
                  CDSOURCE->update(UPD_ALL, tocEdit);
                  break;
    }
  }

  if (TargetType != target_)
  {
    switch (target_) 
    {
	  case T_CD:
                  CDTARGET->stop();
                  break;
      case T_HD:
                  HDTARGET->stop();
                  break;
    }
    switch (TargetType)
    {
      case T_CD:
                  CDTARGET->start(tocEdit, SourceType);
                  break;
      case T_HD:
                  HDTARGET->start(tocEdit);
                  break;                  
    }
    target_ = TargetType;
  }
  else
  {
    switch (target_) 
    {
	  case T_CD:
                  CDTARGET->update(UPD_ALL, tocEdit, SourceType);
                  break;
      case T_HD:
                  HDTARGET->update(UPD_ALL, tocEdit);
                  break;
    }
  }

  show();
}

void RecordGenericDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }

  switch (source_) 
  {
    case S_TOC:
                TOCSOURCE->stop();
                break;
    case S_CD:
                CDSOURCE->stop();
                break;
  }
  source_ = S_NONE;

  switch (target_) 
  {
    case T_CD:
                CDTARGET->stop();
                break;
    case T_HD:
                HDTARGET->stop();
                break;
  }
  target_ = T_NONE;
}

void RecordGenericDialog::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  switch (source_) 
  {
	case S_TOC:
                TOCSOURCE->update(level, tocEdit);
                break;
    case S_CD:
                CDSOURCE->update(level, tocEdit);
                break;
  }

  switch (target_) 
  {
    case T_CD:
                CDTARGET->update(level, tocEdit, source_);
                break;
    case T_HD:
                HDTARGET->update(level, tocEdit);
                break;
  }
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
  switch (target_) 
  {
    case T_CD:
                CDTARGET->startAction(source_, TOCSOURCE, CDSOURCE);
                break;
    case T_HD:
                HDTARGET->startAction(source_, TOCSOURCE, CDSOURCE);
                break;
  }
}

void RecordGenericDialog::help()
{
//FIXME: Show help (gnome help browser??) depending of
// source_ and target_ values...
}
