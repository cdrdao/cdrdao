#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "AudioCDRead.h"
#include "DeviceSelector.h"
#include "CdDevice.h"

Glib::RefPtr<AudioCDRead> AudioCDRead::create(Gtk::Window* parent)
{
    return Glib::make_refptr_for_instance(new AudioCDRead(parent));
}

AudioCDRead::AudioCDRead(Gtk::Window* parent)
{
    set_hide_on_close(true);
    set_modal(true);
    set_transient_for(*parent);
    set_default_size(400,300);
    datafilePath_ = "./datafile.bin";

    auto *vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    vbox->set_margin(10);
    {
	auto *selector = Gtk::make_managed<DeviceSelector>(CdDevice::CD_ROM);
	selector->import();
	selector->selectOne();
	vbox->append(*selector);
    }
    {
	auto *frame = Gtk::make_managed<Gtk::Frame>(_(" Select destination data file:"));
	auto *hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
	hbox->set_margin(5);
	hbox->set_halign(Gtk::Align::CENTER);
	datafileButton_ = Gtk::make_managed<Gtk::Button>(datafilePath_.string());
	datafileButton_->signal_clicked().connect(
	    sigc::mem_fun(*this, &AudioCDRead::on_datafile_button_clicked));
	hbox->append(*datafileButton_);
	frame->set_child(*hbox);
	vbox->append(*frame);
    }

    set_child(*vbox);
}

void AudioCDRead::on_datafile_button_clicked()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_modal(true);
    dialog->set_title(_("Choose the destination data file"));
    if (!datafilePath_.empty()) {
	dialog->set_initial_folder(Gio::File::create_for_path(datafilePath_.parent_path()));
	dialog->set_initial_name(datafilePath_.filename());
    }

    dialog->save([this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
	try {
	    auto file = dialog->save_finish(result);
	    if (file) {
		this->datafilePath_ = file->get_path();
		datafileButton_->set_label(file->get_path());
	    }
	} catch (...) {
	}
    });
}
