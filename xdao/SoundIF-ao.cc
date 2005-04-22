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

#include <ao/ao.h>

#include "SoundIF.h"
#include "Sample.h"
#include "util.h"

class SoundIFImpl
{
public:
  int              driverId;
  ao_device*       device;
  ao_sample_format format;
};

SoundIF::SoundIF()
{
  ao_initialize();

  impl_ = new SoundIFImpl;
  impl_->driverId = ao_default_driver_id();
  impl_->format.bits = 16;
  impl_->format.rate = 44100;
  impl_->format.channels = 2;
  impl_->format.byte_format = AO_FMT_NATIVE;  
}

SoundIF::~SoundIF()
{
  if (impl_) delete impl_;
  impl_ = NULL;
  end();
  ao_shutdown();
}

// Initializes sound interface.
// return: 0: OK
//         1: sounde device not found
//         2: cannot setup sound device
int SoundIF::init()
{
  return 0;
}

// Acquires sound device for playing.
// return 0: OK
//        1: error occured

int SoundIF::start()
{
  impl_->device = ao_open_live(impl_->driverId, &(impl_->format), NULL);

  if (!impl_->device)
    return 1;

  return 0;
}

// Playes given sample buffer.
// return: 0: OK
//         1: error occured
int SoundIF::play(Sample *sbuf, long nofSamples)
{
  if (!impl_->device)
    return 1;

  swapSamples(sbuf, nofSamples);

  int ret = ao_play(impl_->device, (char*)sbuf, nofSamples * sizeof(Sample));

  if (ret == 0)
    return 1;

  return 0;
}

unsigned long SoundIF::getDelay()
{
  // Unfortunately, ao doesn't have a getDelay() API, so let's return
  // a realistic audio buffering value.
  return 10000;
}

// Finishs playing, sound device is released.
void SoundIF::end()
{
  ao_close(impl_->device);
  impl_->device = NULL;
}
