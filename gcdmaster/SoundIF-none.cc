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
 * $Log: SoundIF-none.cc,v $
 * Revision 1.2  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
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
 * Revision 1.1  1999/05/24 18:07:11  mueller
 * Initial revision
 *
 */

/*
 * Dummy sound interface for platforms that are not supported, yet.
 * May serve as template for new implementations.
 */

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

  // ...
  
  return 0;
}

unsigned long SoundIF::getDelay()
{
  if (impl_->dspFd_ < 0)
    return 1;

  // ...

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

  // ...

  return 1;
}
    
void SoundIFImpl::closeDevice()
{
  if (dspFd_ >= 0) {
    // ...
    dspFd_ = -1;
  }
}

int SoundIFImpl::setupDevice()
{
  if (dspFd_ < 0)
    return 1;
  
  // ...

  return 0;
}
