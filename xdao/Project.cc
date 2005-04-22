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

#include <errno.h>
#include <gtkmm.h>
#include <gnome.h>

#include "config.h"

#include "Project.h"
#include "util.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "gcdmaster.h"
#include "MessageBox.h"
#include "AudioCDView.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "RecordTocDialog.h"

Project::Project()
{
  new_ = true;
  saveFileSelector_ = 0;  
  viewNumber = 0;
  recordTocDialog_ = 0;
  parent_ = NULL;
  progressbar_ = NULL;
  progressButton_ = NULL;
  tocEdit_ = NULL;
  saveFileSelector_ = NULL;
}

void Project::updateWindowTitle()
{
  std::string s(tocEdit_->filename());
  s += " - ";
  s += APP_NAME;
  if (tocEdit_->tocDirty())
    s += "(*)";
//FIXME:llanero
//  set_title(s);
}

void Project::saveProject()
{
  if (new_) {
    saveAsProject();
    return;
  }

  if (tocEdit_->saveToc() == 0) {
    statusMessage(_("Project saved to \"%s\"."), tocEdit_->filename());
    guiUpdate(UPD_TOC_DIRTY);

  } else {
    std::string s(_("Cannot save toc to \""));
    s += tocEdit_->filename();
    s+= "\":";
    
    MessageBox msg(parent_, _("Save Project"), 0, s.c_str(), strerror(errno),
                   NULL);
    msg.run();
  }
}

void Project::saveAsProject()
{
  if (!saveFileSelector_) {
    saveFileSelector_ = new Gtk::FileSelection(_("Save Project"));
    saveFileSelector_->get_ok_button()->signal_clicked().
      connect(mem_fun(*this, &Project::saveFileSelectorOKCB));
    saveFileSelector_->get_cancel_button()->signal_clicked().
      connect(mem_fun(*this, &Project::saveFileSelectorCancelCB));
    saveFileSelector_->set_transient_for(*parent_);
  }

  saveFileSelector_->present();
}

void Project::saveFileSelectorCancelCB()
{
  saveFileSelector_->hide();
}

void Project::saveFileSelectorOKCB()
{
  char *s = g_strdup(saveFileSelector_->get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {

    if (tocEdit_->saveAsToc(stripCwd(s)) == 0) {
      statusMessage(_("Project saved to \"%s\"."), tocEdit_->filename());
      new_ = false; // The project is now saved
      updateWindowTitle();
      saveFileSelectorCancelCB();
      guiUpdate(UPD_TOC_DIRTY);

    } else {

      std::string m(_("Cannot save toc to \""));
      m += tocEdit_->filename();
      m += "\":";
      MessageBox msg(saveFileSelector_, _("Save Project"), 0, m.c_str(),
                     strerror(errno), NULL);
      msg.run();
    }
  }

  if (s) g_free(s);
}

gint Project::getViewNumber()
{
  return viewNumber++;
}

int Project::projectNumber()
{
  return projectNumber_;
}

TocEdit *Project::tocEdit()
{
  return tocEdit_;
}

void Project::statusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  char *s = g_strdup_vprintf(fmt, args);

  statusbar_->push(s);

  free(s);

  va_end(args);
}

void Project::tocBlockedMsg(const char *op)
{
  MessageBox msg(parent_, op, 0,
		 _("Cannot perform requested operation because " 
                   "project is in read-only state."), NULL);
  msg.run();
}
