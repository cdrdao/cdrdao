/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
/*
 * $Log: SoundIF-solaris.cc,v $
 * Revision 1.3  2007/12/29 12:31:54  poolshark
 * Moved log code into own file, renamed message call
 *
 * Revision 1.2  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.1.1.1.6.2  2004/01/12 20:50:26  poolshark
 * Added _( and N_( intl macros
 *
 * Revision 1.1.1.1.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.1.1.1  2000/02/05 01:40:00  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/05/24 18:07:37  mueller
 * Initial revision
 *
 */

/*
 * Sound interface for Solaris. Thanks to Tobias Oetiker <oetiker@ee.ethz.ch>.
 */

#include <sys/audioio.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <gnome.h>

#include "SoundIF.h"

#include "Sample.h"
#include "util.h"

class SoundIFImpl {
public:
  SoundIFImpl() { dspFd_ = -1; }

  int setupDevice();
  int openDevice();
  void closeDevice();

  int dspFd_; // sound device
};

SoundIF::SoundIF()
{
  impl_ = new SoundIFImpl;
}

SoundIF::~SoundIF()
{
  delete impl_;
  impl_ = NULL;
}

// Initializes sound interface.
// return: 0: OK
//         1: sounde device not found
//         2: cannot setup sound device
int SoundIF::init()
{
  if (impl_->openDevice() != 0)
    return 1;

  if (impl_->setupDevice() != 0) {
    impl_->closeDevice();
    return 2;
  }

  impl_->closeDevice();
  return 0;
}

// Acquires sound device for playing.
// return 0: OK
//        1: error occured
int SoundIF::start()
{
  if (impl_->dspFd_ >= 0)
    return 0; // already opened

  if (impl_->openDevice() != 0)
    return 1;

  if (impl_->setupDevice() != 0) {
    impl_->closeDevice();
    return 1;
  }

  return 0;
}

// Playes given sample buffer.
// return: 0: OK
//         1: error occured
int SoundIF::play(Sample *sbuf, long nofSamples)
{
  if (impl_->dspFd_ < 0)
    return 1;

  long ret;
  long len = nofSamples * sizeof(Sample);
  long nwritten = 0;
  char *buf = (char *)sbuf;

  while (len > 0) {
    ret = write(impl_->dspFd_, buf + nwritten, len);

    if (ret <= 0)
      return 1;

    nwritten += ret;
    len -= ret;
  }

  return 0;
}

unsigned long SoundIF::getDelay()
{
  return 0;
}

// Finishs playing, sound device is released.
void SoundIF::end()
{
  impl_->closeDevice();
}


int SoundIFImpl::openDevice()
{
  if (dspFd_ >= 0)
    return 0; // already open

  if ((dspFd_ = open("/dev/audio", O_WRONLY | O_NONBLOCK)) < 0) {
    log_message(-1, _("Cannot open \"/dev/audio\": %s"), strerror(errno));
    return 1;
  }
  /* Clear the non-blocking flag */
  (void) fcntl(dspFd_, F_SETFL,
	       (fcntl(dspFd_, F_GETFL, 0) & ~(O_NDELAY | O_NONBLOCK)));

  return 0;
}
    
void SoundIFImpl::closeDevice()
{
  if (dspFd_ >= 0) {
    close(dspFd_);
    dspFd_ = -1;
  }
}

int SoundIFImpl::setupDevice()
{
  struct audio_info auinf;

  if (dspFd_ < 0)
    return 1;
  
  if (ioctl(dspFd_, AUDIO_GETINFO, &auinf) < 0) {
    log_message(-1, _("Cannot get state of audio interface: %s"), strerror(errno));
    return 1;
  }
  auinf.play.sample_rate=44100;
  auinf.play.channels=2;
  auinf.play.precision=16;
  auinf.play.encoding=AUDIO_ENCODING_LINEAR;

  if (ioctl(dspFd_, AUDIO_SETINFO, &auinf) < 0) {
    log_message(-1, _("Cannot setup audio interface: %s"), strerror(errno));
    return 1;
  }

  return 0;
}
