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

#ifndef __SOUND_IF_H__
#define __SOUND_IF_H__

class Sample;

class SoundIF
{
  public:
    // Initializes sound interface.  Returnn null ptr ifsounde device
    // not found or cannot setup sound device.
    static SoundIF* create();

    // Acquires sound device for playing.
    // return 0: OK
    //        1: error occured
    virtual int start() = 0;

    // Playes given sample buffer (44.1khz, 2 channels)
     // return: 0: OK
    //         1: error occured
    virtual int play(Sample *, long) = 0;

    // Returns how long time it's going to take before the next sample
    // to be written gets played by the hardware. The delay is
    // returned in bytes
    virtual unsigned long getDelay() = 0;

    // Finishs playing, sound device is released.
    virtual void end() = 0;
};

#endif
