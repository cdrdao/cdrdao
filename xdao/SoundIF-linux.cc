/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: SoundIF-linux.cc,v $
 * Revision 1.2  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.1.1.1.6.3  2004/01/12 20:50:26  poolshark
 * Added _( and N_( intl macros
 *
 * Revision 1.1.1.1.6.2  2004/01/06 19:03:36  poolshark
 * Missing end in destructor
 *
 * Revision 1.1.1.1.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.1.1.1  2000/02/05 01:39:57  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.3  1999/08/07 16:27:28  mueller
 * Applied patch from Yves Bastide:
 * * prefixing member function names with their class name in connect_to_method
 * * explicitly `const_cast'ing a cast to const
 *
 * Revision 1.2  1999/05/24 18:09:53  mueller
 * Added check for existance of 'SNDCTL_DSP_GETODELAY'.
 *
 * Revision 1.1  1998/11/20 18:57:34  mueller
 * Initial revision
 *
 */

#include <linux/soundcard.h>

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
  end();
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

  swapSamples(sbuf, nofSamples);

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
  if (impl_->dspFd_ < 0)
    return 1;

#ifdef SNDCTL_DSP_GETODELAY
  int delay;

  if (ioctl(impl_->dspFd_, SNDCTL_DSP_GETODELAY, &delay) == 0) {
    return delay / 4;
  }
#endif

  return 0;
}

// Finishs playing, sound device is released.
void SoundIF::end()
{
  impl_->closeDevice();
}


int SoundIFImpl::openDevice()
{
  int flags;

  if (dspFd_ >= 0)
    return 0; // already open

  if ((dspFd_ = open("/dev/dsp", O_WRONLY | O_NONBLOCK)) < 0) {
    message(-1, _("Cannot open \"/dev/dsp\": %s"), strerror(errno));
    return 1;
  }

  if ((flags = fcntl(dspFd_, F_GETFL)) >= 0) {
    flags &= ~O_NONBLOCK;
    fcntl(dspFd_, F_SETFL, flags);
  }

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
  if (dspFd_ < 0)
    return 1;
  
  int val = 44100;
  if (ioctl(dspFd_, SNDCTL_DSP_SPEED, &val) < 0) {
    message(-1, _("Cannot set sample rate to 44100: %s"), strerror(errno));
    return 1;
  }

  val = 2;
  if (ioctl(dspFd_, SNDCTL_DSP_CHANNELS, &val) < 0) {
    message(-1, _("Cannot setup 2 channels: %s"), strerror(errno));
    return 1;
  }

  val = AFMT_S16_LE;
  if (ioctl(dspFd_, SNDCTL_DSP_SETFMT, &val) < 0) {
    message(-1, _("Cannot setup sound format: %s"), strerror(errno));
    return 1;
  }

  if (val != AFMT_S16_LE) {
    message(-1, _("Sound device does not support "
                  "little endian signed 16 bit samples."));
    return 1;
  }
  
  return 0;
}
