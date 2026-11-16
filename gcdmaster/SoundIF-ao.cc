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
#include <cstring>

#include "Sample.h"
#include "SoundIF.h"
#include "util.h"

class SoundIFao : public SoundIF
{
public:
    SoundIFao();
    ~SoundIFao();

    int start();
    int play(Sample*, long);
    unsigned long getDelay();
    void end();

private:
    int driverId;
    ao_device *device;
    ao_sample_format format;
};

SoundIFao::SoundIFao()
{
    ao_initialize();
    driverId = ao_default_driver_id();
    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.rate = 44100;
    format.channels = 2;
    format.byte_format = AO_FMT_NATIVE;
}

SoundIFao::~SoundIFao()
{
    end();
    ao_shutdown();
}

int SoundIFao::start()
{
    device = ao_open_live(driverId, &(format), NULL);

    if (!device)
        return 1;

    return 0;
}

int SoundIFao::play(Sample *sbuf, long nofSamples)
{
    if (!device)
        return 1;

    swapSamples(sbuf, nofSamples);

    int ret = ao_play(device, (char *)sbuf, nofSamples * sizeof(Sample));

    if (ret == 0)
        return 1;

    return 0;
}

unsigned long SoundIFao::getDelay()
{
    // Unfortunately, ao doesn't have a getDelay() API, so let's return
    // a realistic audio buffering value.
    return 10000;
}

// Finishs playing, sound device is released.
void SoundIFao::end()
{
    ao_close(device);
    device = NULL;
}

SoundIF* SoundIF::create()
{
    return new SoundIFao;
}
