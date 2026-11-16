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

#ifndef __CD_TEXT_SUB_CHANNEL_ENCODER_H__
#define __CD_TEXT_SUB_CHANNEL_ENCODER_H__

#include "CdTextEncoder.h"

class PWSubChannel96;

// Extends CdTextEncoder with the R-W sub-channel data needed to write the
// CD-TEXT packs into the lead-in.
class CdTextSubChannelEncoder : public CdTextEncoder
{
  public:
    CdTextSubChannelEncoder(const Toc *);
    ~CdTextSubChannelEncoder() override;

    int encode() override;

    const PWSubChannel96 **getSubChannels(long *subChannelCount);

  private:
    PWSubChannel96 **subChannels_;
    long subChannelCount_;

    void freeSubChannels();
    void buildSubChannels();
};

#endif
