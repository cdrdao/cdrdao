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

#include <cassert>
#include <string.h>

#include "FormatSupport.h"
#include "log.h"

FormatSupport::Status FormatSupport::convert(const std::string from, const std::string to)
{
    auto status = convertStart(from, to);
    if (status != FS_SUCCESS)
        return status;

    while ((status = convertContinue()) == FS_IN_PROGRESS)
        ;

    return status;
}

FormatSupport::Status FormatSupport::setup_wav_output(const std::string &wav_file)
{
    memset(&ao_format_, 0, sizeof(ao_format_));
    ao_format_.bits = 16;
    ao_format_.channels = 2;
    ao_format_.rate = TARGET_SAMPLE_RATE;
    ao_format_.byte_format = AO_FMT_LITTLE;

    ao_dev_ = ao_open_file(ao_driver_id("wav"), wav_file.c_str(), 1, &ao_format_, nullptr);
    if (!ao_dev_) {
        log_message(-2, "Unable to create output file \"%s\": %s", wav_file.c_str(),
                    strerror(errno));
        return FS_OUTPUT_PROBLEM;
    }
    return FS_SUCCESS;
}

FormatSupport::Status FormatSupport::write_wav_output(char *samples, u32 num_bytes)
{
    assert(ao_dev_);
    if (ao_play(ao_dev_, samples, num_bytes) == 0)
        return FS_DISK_FULL;

    return FS_SUCCESS;
}

FormatSupport::Status FormatSupport::close_wav_output()
{
    if (ao_dev_) {
        ao_close(ao_dev_);
        ao_dev_ = nullptr;
    }

    return FS_SUCCESS;
}
