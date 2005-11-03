#include "Icons.h"
#include "stock/pixbufs.h"

Gtk::StockID Icons::PLAY("gcdmaster-play");
Gtk::StockID Icons::STOP("gcdmaster-stop");
Gtk::StockID Icons::PAUSE("gcdmaster-pause");
Gtk::StockID Icons::GCDMASTER("gcdmaster-gcdmaster");
Gtk::StockID Icons::OPEN("gcdmaster-open");
Gtk::StockID Icons::AUDIOCD("gcdmaster-audiocd");
Gtk::StockID Icons::COPYCD("gcdmaster-copycd");
Gtk::StockID Icons::DUMPCD("gcdmaster-dumpcd");
Gtk::StockID Icons::RECORD("gcdmaster-record");

struct Icons::IconEntry Icons::iconList[] = {
  { Icons::PLAY,      play_pixbuf },
  { Icons::STOP,      stop_pixbuf },
  { Icons::PAUSE,     pause_pixbuf },
  { Icons::GCDMASTER, gcdmaster_pixbuf },
  { Icons::OPEN,      open_pixbuf },
  { Icons::AUDIOCD,   audiocd_pixbuf },
  { Icons::COPYCD,    copycd_pixbuf},
  { Icons::DUMPCD,    dumpcd_pixbuf},
  { Icons::RECORD,    record_pixbuf}
};

void Icons::registerStockIcons()
{
  Glib::RefPtr<Gtk::IconFactory> factory = Gtk::IconFactory::create();
  factory->add_default();

  for (unsigned i = 0; i < G_N_ELEMENTS(iconList); i++) {
    Gtk::IconSource* source = new Gtk::IconSource;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf =
        Gdk::Pixbuf::create_from_inline(-1, iconList[i].pixbuf);
    source->set_pixbuf(pixbuf);
    Gtk::IconSet* set = new Gtk::IconSet;
    set->add_source(*source);
    factory->add(iconList[i].name, *set);
  }
}
