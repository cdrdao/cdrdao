#include "Icons.h"

Glib::ustring Icons::PLAY("gcdmaster-play");
Glib::ustring Icons::STOP("gcdmaster-stop");
Glib::ustring Icons::PAUSE("gcdmaster-pause");
Glib::ustring Icons::GCDMASTER("gcdmaster-gcdmaster");
Glib::ustring Icons::OPEN("gcdmaster-open");
Glib::ustring Icons::AUDIOCD("gcdmaster-audiocd");
Glib::ustring Icons::COPYCD("gcdmaster-copycd");
Glib::ustring Icons::DUMPCD("gcdmaster-dumpcd");
Glib::ustring Icons::RECORD("gcdmaster-record");

void Icons::registerIcons()
{
    auto display = Gdk::Display::get_default();
    auto icon_theme = Gtk::IconTheme::get_for_display(display);
    icon_theme->add_resource_path("/org/gnome/gcdmaster/icons");
}
