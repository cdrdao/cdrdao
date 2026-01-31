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

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <gtkmm.h>

#include "MessageBox.h"
#include "RecordTocSource.h"
#include "config.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "TocEdit.h"
#include "guiUpdate.h"

#include "DeviceList.h"
#include "util.h"

RecordTocSource::RecordTocSource(Glib::RefPtr<TocEdit> tocEdit)
    : Gtk::Box(Gtk::Orientation::VERTICAL),
      tocEdit_(tocEdit)
{
    // device settings
    auto *infoFrame = Gtk::make_managed<Gtk::Frame>(_(" Project Information "));

    auto *grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(5);
    grid->set_margin(10);

    grid->attach(label1, 0, 0);
    grid->attach(projectLabel_, 1, 0);
    grid->attach(label2, 0, 1);
    grid->attach(tocTypeLabel_, 1, 1);
    grid->attach(label3, 0, 2);
    grid->attach(nofTracksLabel_, 1, 2);
    grid->attach(label4, 0, 3);
    grid->attach(tocLengthLabel_, 1, 3);

    infoFrame->set_child(*grid);
    append(*infoFrame);
}

void RecordTocSource::set(Glib::RefPtr<TocEdit> tocEdit)
{
    if (tocEdit == tocEdit_)
        return;

    tocEdit_ = tocEdit;
    update(UPD_ALL);
}

void RecordTocSource::update(unsigned long level)
{
    log_message(1, "[update %x] RecordTocSource", level);

    if (tocEdit_ == NULL) {
        projectLabel_.set_text("");
        tocTypeLabel_.set_text("");
        nofTracksLabel_.set_text("");
        tocLengthLabel_.set_text("");
    } else {
        if (level & UPD_TOC_DATA) {
            const Toc *toc = tocEdit_->toc();

            projectLabel_.set_text(tocEdit_->filename().filename().string());
            tocTypeLabel_.set_text(toc->tocType2String(toc->tocType()));
            nofTracksLabel_.set_text(std::to_string(toc->nofTracks()));

            {
                char buf[50];
                snprintf(buf, sizeof(buf), "%d:%02d:%02d",
                         toc->length().min(),
                         toc->length().sec(),
                         toc->length().frac());
                tocLengthLabel_.set_text(buf);
            }
        }
    }
}
