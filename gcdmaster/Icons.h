#ifndef ICONS_H
#define ICONS_H

#include <gtkmm.h>

class Icons
{
  public:
    static void registerIcons();

    static Glib::ustring PLAY;
    static Glib::ustring STOP;
    static Glib::ustring PAUSE;
    static Glib::ustring GCDMASTER;
    static Glib::ustring OPEN;
    static Glib::ustring AUDIOCD;
    static Glib::ustring COPYCD;
    static Glib::ustring DUMPCD;
    static Glib::ustring RECORD;
};

#endif
