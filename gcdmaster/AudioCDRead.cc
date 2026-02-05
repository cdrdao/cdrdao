#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "AudioCDRead.h"
#include "DeviceSelector.h"
#include "MessageBox.h"
#include "CdDevice.h"
#include "RecordCDSource.h"
#include "trackdb/TempFileManager.h"
#include "guiUpdate.h"
#include "trackdb/log.h"
#include "AudioCDProject.h"

Glib::RefPtr<AudioCDRead> AudioCDRead::create(Gtk::Window* parent)
{
    return Glib::make_refptr_for_instance(new AudioCDRead(parent));
}

AudioCDRead::AudioCDRead(Gtk::Window* parent)
    : parent_(parent)
{
    set_hide_on_close(true);
    set_modal(true);
    set_transient_for(*parent);
    set_default_size(400,300);
    datafilePath_ = "./datafile.bin";
    set_title(_("Read CD"));

    auto *vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    vbox->set_margin(10);
    {
	deviceSelector_ = Gtk::make_managed<DeviceSelector>(CdDevice::CD_ROM);
	deviceSelector_->import();
	deviceSelector_->signalChanged.connect(sigc::mem_fun(*this,
							    &AudioCDRead::on_device_selected));
	vbox->append(*deviceSelector_);
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
    {
	auto* expander = Gtk::make_managed<Gtk::Expander>(Glib::ustring("  ") + _("More Options"));
	CDSource_ = Gtk::make_managed<RecordCDSource>(parent);
	CDSource_->set_margin_top(10);
	expander->set_child(*CDSource_);
	vbox->append(*expander);
    }
    {
	auto *hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
	hbox->set_spacing(10);
	hbox->set_homogeneous(true);
	hbox->set_margin_start(10);
	hbox->set_margin_end(10);
	hbox->set_halign(Gtk::Align::CENTER);
	startButton_ = Gtk::make_managed<Gtk::Button>();
	auto *startbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
	startbox->set_margin_start(10);
	startbox->set_margin_end(10);
	auto *startpix = Gtk::make_managed<Gtk::Image>();
	startpix->set_from_resource("/org/gnome/gcdmaster/dumpcd.png");
	startpix->set_pixel_size(24);
	startbox->append(*startpix);
	startbox->append(*Gtk::make_managed<Gtk::Label>(_("Start")));
	startButton_->set_child(*startbox);
	startButton_->signal_clicked().connect(sigc::mem_fun(*this,
							 &AudioCDRead::execute));
	hbox->append(*startButton_);
	auto *cancelBut = Gtk::make_managed<Gtk::Button>(_("Cancel"));
	cancelBut->signal_clicked().connect(sigc::mem_fun(*this,
							  &Gtk::Window::close));
	hbox->append(*cancelBut);
	vbox->append(*hbox);
    }

    deviceSelector_->selectOne();
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

void AudioCDRead::on_device_selected()
{
    startButton_->set_sensitive(deviceSelector_->selection() != nullptr);
}

void AudioCDRead::execute()
{
    CdDevice* device;
    std::string tocfile;

    device = deviceSelector_->selection();
    if (!device) {
	MessageBox::message(*parent_,_("Please select a reader device first."));
	return;
    }
    tempFileManager.getTempFile(tocfile, "gcdmaster_temporary", "toc");
    log_message(0, "Creating %s", tocfile.c_str());
    std::filesystem::remove(tocfile);
    std::filesystem::remove(datafilePath_);

    device->extractDao(*parent_, tocfile, datafilePath_.string(), 
		       CDSource_->getCorrection(),
		       CDSource_->getSubChanReadMode());

    conn_ =
	device->signalProcessFinished.connect([this, tocfile](CdDevice* device) {
	    this->conn_.disconnect();
	    auto proj = dynamic_cast<AudioCDProject*>(parent_);
	    proj->changeToc(tocfile);
	});
    hide();
}

void AudioCDRead::update(unsigned long level)
{
    if (level & UPD_PROGRESS_STATUS) {
    }
}
