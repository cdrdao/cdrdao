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
 * Revision 1.1  2000/02/05 01:39:32  llanero
 * Initial revision
 *
 */

static char rcsid[] = "$Id: MessageBox.cc,v 1.1 2000/02/05 01:39:32 llanero Exp $";

#include <stddef.h>
#include <stdarg.h>

#include "MessageBox.h"

MessageBoxBase::MessageBoxBase(Gtk_Window * win)
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

void MessageBoxBase::init(const char *title, int askDontShow,
			  int nButtons, int defaultButton, char *buttons[],
			  va_list args)
{
  int i;
  const char *s;

  done_ = 0;
  doneDefault_ = defaultButton;

  set_title(title);

  Gtk_ObjectHandle<Gtk_HButtonBox> bbox(new Gtk_HButtonBox(GTK_BUTTONBOX_SPREAD));
  bbox->show();

  Gtk_ObjectHandle<Gtk_VBox> contents(new Gtk_VBox);
  contents->show();
  //contents->set_spacing(5);

  
  for (i = 1; i <= nButtons; i++) {
    Gtk_ObjectHandle<Gtk_Button> button(new Gtk_Button(string(buttons[i - 1])));
    button->show();
    connect_to_method(button->clicked, this, &MessageBoxBase::buttonAction, i);
    bbox->pack_start(button);
  }

  while ((s = va_arg(args, const char *)) != NULL) {
    Gtk_ObjectHandle<Gtk_HBox> lbox(new Gtk_HBox);
    lbox->show();
    Gtk_ObjectHandle<Gtk_Label> label(new Gtk_Label(s));
    label->show();
    lbox->pack_start(label, FALSE);
    contents->pack_start(lbox, FALSE);
  }

  if (askDontShow) {
    dontShowAgain_ = new Gtk_CheckButton(string("Don't show this message again"));
    dontShowAgain_->set_active(FALSE);
    dontShowAgain_->show();
    Gtk_ObjectHandle<Gtk_HBox> box(new Gtk_HBox);
    Gtk_ObjectHandle<Gtk_Label> label(new Gtk_Label(string("")));

    label->show();
    box->show();
    box->pack_end(*dontShowAgain_, FALSE);
    contents->pack_start(label, FALSE);
    contents->pack_start(box, FALSE);
  }

  Gtk_ObjectHandle<Gtk_HBox> hcontens(new Gtk_HBox);
  hcontens->show();

  hcontens->pack_start(contents, TRUE, TRUE, 10);
  get_vbox()->pack_start(hcontens, FALSE, FALSE, 10);
  get_vbox()->show();

  
  get_action_area()->pack_start(bbox);
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
  Gtk_Main *app = Gtk_Main::instance();

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


MessageBox::MessageBox(Gtk_Window *win, const char *title,
		       int askDontShow, ...) : MessageBoxBase(win)
{
  va_list args;
  char *buttons[1];

  buttons[0] = "Ok";

  va_start(args, askDontShow);

  init(title, askDontShow, 1, 1, buttons, args);

  va_end(args);
}


MessageBox::~MessageBox()
{
}

Ask2Box::Ask2Box(Gtk_Window *win, const char *title, int askDontShow,
		 int defaultButton, ...)
  : MessageBoxBase(win)

{
  va_list args;
  char *buttons[2];
  
  buttons[0] = "Yes";
  buttons[1] = "No";

  if (defaultButton < 0 || defaultButton > 2)
    defaultButton = 0;

  va_start(args, defaultButton);

  init(title, askDontShow, 2, defaultButton, buttons, args);

  va_end(args);
  
}

Ask2Box::~Ask2Box()
{
}

Ask3Box::Ask3Box(Gtk_Window *win, const char *title, int askDontShow,
		 int defaultButton, ...)
  : MessageBoxBase(win)
{
  va_list args;
  char *buttons[3];
  
  buttons[0] = "Yes";
  buttons[1] = "No";
  buttons[2] = "Cancel";

  if (defaultButton < 0 || defaultButton > 3)
    defaultButton = 0;

  va_start(args, defaultButton);

  init(title, askDontShow, 3, defaultButton, buttons, args);

  va_end(args);
  
}

Ask3Box::~Ask3Box()
{
}
