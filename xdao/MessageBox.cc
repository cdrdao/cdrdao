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
 * $Log: MessageBox.cc,v $
 * Revision 1.4  2000/05/01 18:15:00  andreasm
 * Switch to gnome-config settings.
 * Adapted Message Box to Gnome look, unfortunately the Gnome::MessageBox is
 * not implemented in gnome--, yet.
 *
 * Revision 1.3  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:32  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 */

static char rcsid[] = "$Id: MessageBox.cc,v 1.4 2000/05/01 18:15:00 andreasm Exp $";

#include <stddef.h>
#include <stdarg.h>

#include <gnome--.h>

#include "MessageBox.h"

MessageBoxBase::MessageBoxBase(Gtk::Window * win)
{
  done_ = 0;
  doneDefault_ = 0;

  dontShowAgain_ = NULL;

  if (win != NULL)
    set_transient_for(*win);

  set_position(GTK_WIN_POS_CENTER);
}

MessageBoxBase::~MessageBoxBase()
{
  delete dontShowAgain_;
  dontShowAgain_ = NULL;
}

Gtk::Button *MessageBoxBase::createButton(const char *name)
{
  Gnome::Stock *pixmap = NULL;
  const char *text;

  if (strcmp(name, GNOME_STOCK_BUTTON_OK) == 0) {
    pixmap = manage(Gnome::Stock::pixmap_widget(*this, name));
    text = "Ok";
  }
  else if (strcmp(name, GNOME_STOCK_BUTTON_CANCEL) == 0) {
    pixmap = manage(Gnome::Stock::pixmap_widget(*this, name));
    text = "Cancel";
  }
  else if (strcmp(name, GNOME_STOCK_BUTTON_YES) == 0) {
    pixmap = manage(Gnome::Stock::pixmap_widget(*this, name));
    text = "Yes";
  }
  else if (strcmp(name, GNOME_STOCK_BUTTON_NO) == 0) {
    pixmap = manage(Gnome::Stock::pixmap_widget(*this, name));
    text = "No";
  }

  if (pixmap != NULL)
    return Gnome::Stock::pixmap_button(*pixmap, text);
  else 
    return new Gtk::Button(name);
}

void MessageBoxBase::init(const char *type, const char *title, int askDontShow,
			  int nButtons, int defaultButton, char *buttons[],
			  va_list args)
{
  int i;
  const char *s;

  done_ = 0;
  doneDefault_ = defaultButton;

  set_title(title);

  Gtk::HButtonBox* bbox = manage(new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD));
  bbox->show();

  Gtk::VBox* contents = manage(new Gtk::VBox);
  contents->show();
  //contents->set_spacing(5);

  
  for (i = 1; i <= nButtons; i++) {
    //Gtk::Button* button = manage(new Gtk::Button(string(buttons[i - 1])));
    Gtk::Button* button = manage(createButton(buttons[i - 1]));
    button->show();
    button->clicked.connect(bind(slot(this,&MessageBoxBase::buttonAction),i));
    bbox->add(*button);
  }

  while ((s = va_arg(args, const char *)) != NULL) {
    Gtk::HBox* lbox = manage(new Gtk::HBox);
    lbox->show();
    Gtk::Label* label = manage(new Gtk::Label(s));
    label->show();
    lbox->pack_start(*label, FALSE);
    contents->pack_start(*lbox, FALSE);
  }

  if (askDontShow) {
    dontShowAgain_ = new Gtk::CheckButton(string("Don't show this message again"));
    dontShowAgain_->set_active(FALSE);
    dontShowAgain_->show();

    Gtk::HBox* box = manage(new Gtk::HBox);
    Gtk::Label* label = manage(new Gtk::Label(string("")));

    label->show();
    box->show();
    box->pack_end(*dontShowAgain_, FALSE);
    contents->pack_start(*label, FALSE);
    contents->pack_start(*box, FALSE);
  }


  Gtk::HBox* hcontens = manage(new Gtk::HBox);
  hcontens->show();

  /*
  Gnome::Pixmap *typePixmap = manage(new Gnome::Pixmap("gnome-info.png"));
  if (typePixmap != NULL) {
    hcontens->pack_start(*typePixmap, FALSE, FALSE, 10);
    typePixmap->show();
  }
  */

  hcontens->pack_start(*contents, TRUE, TRUE, 10);
  get_vbox()->pack_start(*hcontens, FALSE, FALSE, 10);
  get_vbox()->show();

  
  get_action_area()->pack_start(*bbox);
  get_action_area()->show();
    
}

void MessageBoxBase::buttonAction(int act)
{
  done_ = act;
}

gint MessageBoxBase::delete_event_impl(GdkEventAny*)
{
  done_ = doneDefault_;
  return 1;
}

int MessageBoxBase::run()
{
  Gtk::Main *app = Gtk::Main::instance();

  show();

  app->grab_add(*this);

  do {
    app->iteration();
  }  while (done_ == 0);

  app->grab_remove(*this);

  hide();

  return done_;
}

int MessageBoxBase::dontShowAgain() const
{
  if (dontShowAgain_ != NULL)
    return dontShowAgain_->get_active() ? 1 : 0;
  else
    return 0;
}


MessageBox::MessageBox(Gtk::Window *win, const char *title,
		       int askDontShow, ...) : MessageBoxBase(win)
{
  va_list args;
  char *buttons[1];

  buttons[0] = GNOME_STOCK_BUTTON_OK;

  va_start(args, askDontShow);

  init(GNOME_MESSAGE_BOX_INFO, title, askDontShow, 1, 1, buttons, args);

  va_end(args);
}


MessageBox::~MessageBox()
{
}

Ask2Box::Ask2Box(Gtk::Window *win, const char *title, int askDontShow,
		 int defaultButton, ...)
  : MessageBoxBase(win)

{
  va_list args;
  char *buttons[2];
  
  buttons[0] = GNOME_STOCK_BUTTON_YES;
  buttons[1] = GNOME_STOCK_BUTTON_NO;

  if (defaultButton < 0 || defaultButton > 2)
    defaultButton = 0;

  va_start(args, defaultButton);

  init(GNOME_MESSAGE_BOX_QUESTION, title, askDontShow, 2, defaultButton,
       buttons, args);

  va_end(args);
  
}

Ask2Box::~Ask2Box()
{
}

Ask3Box::Ask3Box(Gtk::Window *win, const char *title, int askDontShow,
		 int defaultButton, ...)
  : MessageBoxBase(win)
{
  va_list args;
  char *buttons[3];
  
  buttons[0] = GNOME_STOCK_BUTTON_YES;
  buttons[1] = GNOME_STOCK_BUTTON_NO;
  buttons[2] = GNOME_STOCK_BUTTON_CANCEL;

  if (defaultButton < 0 || defaultButton > 3)
    defaultButton = 0;

  va_start(args, defaultButton);

  init(GNOME_MESSAGE_BOX_QUESTION, title, askDontShow, 3, defaultButton, buttons, args);

  va_end(args);
  
}

Ask3Box::~Ask3Box()
{
}
