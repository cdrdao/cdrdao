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

#ifndef __FORMATFLAC_H__
#define __FORMATFLAC_H__

#include <stdlib.h>
#include <vector>

#include <FLAC++/decoder.h>

#include "FormatConverter.h"

class FormatFlac : public FormatSupport, FLAC::Decoder::File
{
  public:
    FormatFlac();
    virtual ~FormatFlac();

    Status convertStart(std::string from, std::string to);
    Status convertContinue();
    void convertAbort();

    TrackData::FileType format()
    {
        return TrackData::WAVE;
    }

    FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame,
                                                  const FLAC__int32 *const buffer[]);
    void metadata_callback(const ::FLAC__StreamMetadata *metadata);
    void error_callback(FLAC__StreamDecoderErrorStatus status);

  private:
    std::string source_file, dest_file;
    bool started;

    unsigned sample_rate;
    unsigned channels;
    unsigned bits_per_sample;

    bool need_resampling;
    std::vector<float> src_samples;
    double src_ratio;

    Status resample();
    bool setup_output();
    void finalize();
};

class FormatFlacManager : public FormatSupportManager
{
  public:
    FormatSupport *newConverter(const char *extension);
    int supportedExtensions(std::list<std::string> &);
};

#endif
