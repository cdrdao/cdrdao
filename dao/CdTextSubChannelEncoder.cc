/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001 Andreas Mueller <mueller@daneb.ping.de>
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

#include "CdTextSubChannelEncoder.h"

#include <string.h>

#include "PWSubChannel96.h"

CdTextSubChannelEncoder::CdTextSubChannelEncoder(const Toc *toc) : CdTextEncoder(toc)
{
    subChannels_ = NULL;
    subChannelCount_ = 0;
}

CdTextSubChannelEncoder::~CdTextSubChannelEncoder()
{
    freeSubChannels();
}

void CdTextSubChannelEncoder::freeSubChannels()
{
    if (subChannels_ != NULL) {
        for (long i = 0; i < subChannelCount_; i++)
            delete subChannels_[i];

        delete[] subChannels_;
        subChannels_ = NULL;
    }

    subChannelCount_ = 0;
}

int CdTextSubChannelEncoder::encode()
{
    int ret = CdTextEncoder::encode();

    if (ret == 0)
        buildSubChannels();

    return ret;
}

const PWSubChannel96 **CdTextSubChannelEncoder::getSubChannels(long *subChannelCount)
{
    *subChannelCount = subChannelCount_;

    return (const PWSubChannel96 **)subChannels_;
}

// Builds the R-W sub-channel data for all CD-TEXT packs. The sub-channel
// data of each sector can hold 4 CD-TEXT packs. The packs are repeated
// cyclically so that each sub-channel is completely filled and the
// sequence of sub-channels can be repeated seamlessly.
void CdTextSubChannelEncoder::buildSubChannels()
{
    std::vector<CdTextPack> packs = getPacks();
    long packCount = packs.size();
    unsigned char buf[72];

    freeSubChannels();

    switch (packCount % 4) {
    case 0:
        subChannelCount_ = packCount / 4;
        break;
    case 2:
        subChannelCount_ = packCount / 2;
        break;
    default:
        subChannelCount_ = packCount;
        break;
    }

    if (subChannelCount_ == 0)
        return;

    subChannels_ = new PWSubChannel96 *[subChannelCount_];

    long p = 0;

    for (long i = 0; i < subChannelCount_; i++) {
        subChannels_[i] = new PWSubChannel96;

        for (int j = 0; j < 4; j++) {
            memcpy(buf + j * 18, &packs[p], 18);
            p = (p + 1) % packCount;
        }

        subChannels_[i]->setRawRWdata(buf);
    }
}
