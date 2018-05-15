/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include <stddef.h>
#include <stdarg.h>

#include <gtkmm.h>
#include <gnome.h>

#include "MessageBox.h"

MessageBoxBase::MessageBoxBase(Gtk::Window * win)
{
  done_ = 0;
  doneDefault_ = 0;

  dontShowAgain_ = NULL;

  if (win != NULL) {
    set_transient_for(*win);
    set_modal(true);
  }

  set_position(Gtk::WIN_POS_CENTER);
}

MessageBoxBase::~MessageBoxBase()
{
  delete dontShowAgain_;
  dontShowAgain_ = NULL;
}

Gtk::Button *MessageBoxBase::createButton(const Gtk::BuiltinStockID id)
{
  return new Gtk::Button(Gtk::StockID(id));
}

void MessageBoxBase::init(const char *type, const char *title, int askDontShow,
			  int nButtons, int defaultButton,
                          Gtk::BuiltinStockID buttons[], va_list args)
{
  int i;
  const char *s;

  done_ = 0;
  doneDefault_ = defaultButton;

  set_title(title);

  Gtk::HButtonBox* bbox = manage(new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD));
  bbox->show();

  Gtk::VBox* contents = manage(new Gtk::VBox);
  contents->show();
  
  for (i = 1; i <= nButtons; i++) {
    Gtk::Button* button = manage(createButton(buttons[i - 1]));
    button->show();
    button->signal_clicked().connect(bind(mem_fun(*this,
                                               &MessageBoxBase::buttonAction),
                                          i));
    bbox->add(*button);
  }

  while ((s = va_arg(args, const char *)) != NULL) {
    Gtk::HBox* lbox = manage(new Gtk::HBox);
    lbox->show();
    Gtk::Label* label = manage(new Gtk::Label(s));
    label->show();
    lbox->pack_start(*label, Gtk::PACK_SHRINK);
    contents->pack_start(*lbox, Gtk::PACK_SHRINK);
  }

  if (askDontShow) {
    dontShowAgain_ = new Gtk::CheckButton(_("Don't show this message again"));
    dontShowAgain_->set_active(FALSE);
    dontShowAgain_->show();

    Gtk::HBox* box = manage(new Gtk::HBox);
    Gtk::Label* label = manage(new Gtk::Label(""));

    label->show();
    box->show();
    box->pack_end(*dontShowAgain_, Gtk::PACK_SHRINK);
    contents->pack_start(*label, Gtk::PACK_SHRINK);
    contents->pack_start(*box, Gtk::PACK_SHRINK);
  }


  Gtk::HBox* hcontens = manage(new Gtk::HBox);
  hcontens->show();

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

bool MessageBoxBase::on_delete_event(GdkEventAny*)
{
  done_ = doneDefault_;
  return 1;
}

int MessageBoxBase::run()
{
  Gtk::Main *app = Gtk::Main::instance();

  show();

  do {
    app->iteration();
  }  while (done_ == 0);

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
  Gtk::BuiltinStockID buttons[1];

  buttons[0] = Gtk::Stock::OK;

  va_start(args, askDontShow);

  init("info", title, askDontShow, 1, 1, buttons, args);

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
  Gtk::BuiltinStockID buttons[2];
  
  buttons[0] = Gtk::Stock::YES;
  buttons[1] = Gtk::Stock::NO;

  if (defaultButton < 0 || defaultButton > 2)
    defaultButton = 0;

  va_start(args, defaultButton);

  init("question", title, askDontShow, 2, defaultButton,
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
  Gtk::BuiltinStockID buttons[3];
  
  buttons[0] = Gtk::Stock::YES;
  buttons[1] = Gtk::Stock::NO;
  buttons[2] = Gtk::Stock::CANCEL;

  if (defaultButton < 0 || defaultButton > 3)
    defaultButton = 0;

  va_start(args, defaultButton);

  init("question", title, askDontShow, 3, defaultButton, buttons, args);

  va_end(args);
  
}

Ask3Box::~Ask3Box()
{
}

ErrorBox::ErrorBox(const char* msg)
	: MessageDialog(msg, false, Gtk::MESSAGE_ERROR)
{
}
