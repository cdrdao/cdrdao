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

#include "config.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "AddSilenceDialog.h"

#include "TocEdit.h"
#include "TocEditView.h"
#include "guiUpdate.h"

#include "Sample.h"

Glib::RefPtr<AddSilenceDialog> AddSilenceDialog::create(Gtk::Window* parent)
{
    return Glib::make_refptr_for_instance(new AddSilenceDialog(parent));
}

AddSilenceDialog::AddSilenceDialog(Gtk::Window* parent) :
    parent_(parent)
{
    set_modal(true);
    set_hide_on_close(true);

    auto main_vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    main_vbox->set_margin(10);
    set_child(*main_vbox);

    auto *grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(5);
    grid->set_margin(10);

    auto add_row = [&](const std::string& label_text, Gtk::Widget& entry, int row) {
        auto label = Gtk::make_managed<Gtk::Label>(label_text);
        label->set_halign(Gtk::Align::START);
        grid->attach(*label, 0, row, 1, 1);
        grid->attach(entry, 1, row, 1, 1);
        entry.set_expand(true);
    };

    add_row(_("Minutes:"), minutes_, 0);
    add_row(_("Seconds:"), seconds_, 1);
    add_row(_("Frames:"), frames_, 2);
    add_row(_("Samples:"), samples_, 3);

    auto frame = Gtk::make_managed<Gtk::Frame>(_(" Length of Silence "));
    frame->set_child(*grid);
    main_vbox->append(*frame);
    
    auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    bbox->set_halign(Gtk::Align::END);
    bbox->set_margin_top(10);

    applyButton_ = Gtk::make_managed<Gtk::Button>(_("_Apply"));
    applyButton_->add_css_class("suggested-action");
    applyButton_->signal_clicked().connect(sigc::mem_fun(*this, &AddSilenceDialog::applyAction));

    auto clearBtn = Gtk::make_managed<Gtk::Button>(_("_Clear"));
    clearBtn->signal_clicked().connect(sigc::mem_fun(*this, &AddSilenceDialog::clearAction));

    auto closeBtn = Gtk::make_managed<Gtk::Button>(_("_Close"));
    closeBtn->signal_clicked().connect(sigc::mem_fun(*this, &AddSilenceDialog::closeAction));

    bbox->append(*closeBtn);
    bbox->append(*clearBtn);
    bbox->append(*applyButton_);
    main_vbox->append(*bbox);
}

void AddSilenceDialog::mode(Mode m)
{
    mode_ = m;

    switch (mode_) {
    case M_APPEND:
        set_title(_("Append Silence"));
        break;
    case M_INSERT:
        set_title(_("Insert Silence"));
        break;
    }
}

void AddSilenceDialog::start(Mode m,TocEditView *view)
{
    mode(m);
    active_ = true;
    if (view) {
        if (parent_)
            set_transient_for(*parent_);
    }
    update(UPD_ALL, view);
    present();
    tocEditView_ = view;
}

void AddSilenceDialog::update(unsigned long level, TocEditView *view)
{
    if (!active_)
        return;

    if (view == NULL) {
        applyButton_->set_sensitive(false);
        tocEditView_ = NULL;
        return;
    }

    std::string s(view->tocEdit()->filename());
    s += " - ";
    s += APP_NAME;
    if (view->tocEdit()->tocDirty())
        s += "(*)";
    set_title(s);

    if ((level & UPD_EDITABLE_STATE) || tocEditView_ == NULL) {
        applyButton_->set_sensitive(view->tocEdit()->editable() ? true : false);
    }

    tocEditView_ = view;
}

bool AddSilenceDialog::on_close_request()
{
    stop();
    return true;
}

void AddSilenceDialog::closeAction()
{
    stop();
}

void AddSilenceDialog::clearAction()
{
    minutes_.set_text("");
    seconds_.set_text("");
    frames_.set_text("");
    samples_.set_text("");
}

void AddSilenceDialog::applyAction()
{
    char buf[20];
    long val;

    if (tocEditView_ == NULL)
        return;

    TocEdit* tocEdit = tocEditView_->tocEdit();

    if (!tocEdit->editable())
        return;

    unsigned long length = 0;
    auto parse = [&](Gtk::Entry& e) {
        auto t = e.get_text();
        return t.empty() ? 0 : std::atol(t.c_str());
    };

    length += parse(minutes_) * 60 * 75 * SAMPLES_PER_BLOCK;
    length += parse(seconds_) * 75 * SAMPLES_PER_BLOCK;
    length += parse(frames_) * SAMPLES_PER_BLOCK;
    length += parse(samples_);


    if (length > 0) {
        unsigned long pos;

        if (mode_ == M_APPEND) {
            tocEdit->appendSilence(length);
            update(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL, tocEditView_);
            signal_tocModified.emit(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL);
            signal_fullView.emit();
            signal_tocModified.emit(UPD_SAMPLES);
        } else if (mode_ ==  M_INSERT && tocEditView_->sampleMarker(&pos)) {
            if (tocEdit->insertSilence(length, pos) == 0) {
                tocEditView_->sampleSelect(pos, pos + length - 1);
                update(UPD_TOC_DATA | UPD_TRACK_DATA, tocEditView_);
                signal_tocModified.emit(UPD_TOC_DATA | UPD_TRACK_DATA);
            }
        }
        guiUpdate();
    }
}
