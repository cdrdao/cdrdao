#ifndef ICONS_H
#define ICONS_H

#include <gtkmm.h>

class Icons
{
 public:
  static const char* PLAY;
  static const char* STOP;
  static const char* PAUSE;
  static const char* GCDMASTER;
  static const char* OPEN;
  static const char* AUDIOCD;
  static const char* COPYCD;
  static const char* DUMPCD;
  static const char* RECORD;

  static void registerIcons();
};


#endif
