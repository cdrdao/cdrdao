#ifndef ICONS_H
#define ICONS_H

#include <gtkmm.h>

class Icons
{
 public:
  static void registerStockIcons();

  static Gtk::StockID PLAY;
  static Gtk::StockID STOP;
  static Gtk::StockID PAUSE;
  static Gtk::StockID GCDMASTER;
  static Gtk::StockID OPEN;
  static Gtk::StockID AUDIOCD;
  static Gtk::StockID COPYCD;
  static Gtk::StockID DUMPCD;
  static Gtk::StockID RECORD;

 private:
  static struct IconEntry {
    Gtk::StockID& name;
    const guint8* pixbuf;
  } iconList[];
};


#endif
