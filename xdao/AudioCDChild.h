/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __AUDIO_CD_CHILD_H__
#define __AUDIO_CD_CHILD_H__

#include <gtkmm.h>
#include <libgnomeuimm.h>

#include "Toc.h"
#include "GenericChild.h"

class Toc;
class Track;
class SoundIF;
class Sample;
class TrackData;
class TocEdit;
class AudioCDView;
class AudioCDProject;

class AudioCDChild : public GenericChild
{
public:
  AudioCDChild(AudioCDProject *project);
  ~AudioCDChild();

  AudioCDView* view();
  bool closeProject();
  void update(unsigned long level);

  const char *sample2string(unsigned long sample);
  unsigned long string2sample(const char *s);

private:
  AudioCDProject* project_;
  AudioCDView*    view_;

  void tocBlockedMsg(const char *);
};
#endif
