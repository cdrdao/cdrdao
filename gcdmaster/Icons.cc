#include "Icons.h"
#include "stock/pixbufs.h"

const char* Icons::PLAY = "gcdmaster-play";
const char* Icons::STOP = "gcdmaster-stop";
const char* Icons::PAUSE = "gcdmaster-pause";
const char* Icons::GCDMASTER = "gcdmaster-gcdmaster";
const char* Icons::OPEN = "gcdmaster-open";
const char* Icons::AUDIOCD = "gcdmaster-audiocd";
const char* Icons::COPYCD = "gcdmaster-copycd";
const char* Icons::DUMPCD = "gcdmaster-dumpcd";
const char* Icons::RECORD = "gcdmaster-record";


void Icons::registerIcons()
{
    printf("Adding Icon path %s\n", GCDMASTER_ICONDIR);
    Gtk::IconTheme::get_default()->append_search_path(GCDMASTER_ICONDIR);
}
