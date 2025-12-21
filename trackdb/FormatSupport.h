/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2025  Denis Leroy <denis@poolshark.org>
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

#ifndef __FORMATSUPPORT_H__
#define __FORMATSUPPORT_H__

#include "config.h"
#include <string>

#ifdef HAVE_AO
#include <ao/ao.h>
#endif

#include "TrackData.h"
#include "util.h"

// Quick abstract class declarations. Format converters should derive
// their own FormatSupport and FormatSupportManager.

class FormatSupport
{
  public:
    virtual ~FormatSupport()
    {
    }

    typedef enum {
        FS_SUCCESS = 0,
        FS_IN_PROGRESS,
        FS_WRONG_FORMAT,
        FS_INPUT_PROBLEM,
        FS_OUTPUT_PROBLEM,
        FS_DISK_FULL,
        FS_DECODE_ERROR,
        FS_OTHER_ERROR,
    } Status;

    static constexpr unsigned TARGET_SAMPLE_RATE = 44100;

    // Convert source file to destination WAV or RAW. This is a blocking
    // call until conversion is finished.
    virtual Status convert(const std::string from, const std::string to);

    // Same as above, but asynchronous interface. Call start, then call
    // continue in a busy loop until it no longer returns
    // FS_IN_PROGRESS.
    virtual Status convertStart(const std::string from, const std::string to) = 0;
    virtual Status convertContinue() = 0;
    virtual void convertAbort() = 0;

    // Specify what this object converts to. Should only returns either
    // TrackData::WAVE or TrackData::RAW
    virtual TrackData::FileType format() = 0;

    // Common WAV output functions
    virtual Status setup_wav_output(const std::string &wav_file);
    virtual Status write_wav_output(char *samples, u32 num_bytes);
    virtual Status close_wav_output();

  protected:
#ifdef HAVE_AO
    ao_device *ao_dev_;
    ao_sample_format ao_format_;
#endif
};

#endif
