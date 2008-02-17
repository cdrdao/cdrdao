/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2004  Denis Leroy <denis@poolshark.org>
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

#include <stdio.h>
#include <cstring>

#include "log.h"
#include "FormatOgg.h"


FormatSupport::Status FormatOgg::convert(const char* from, const char* to)
{
  src_file_ = from;
  dst_file_ = to;

  Status err = oggInit();
  if (err != FS_SUCCESS)
    return err;

  while ((err = oggDecodeFrame()) == FS_IN_PROGRESS);

  oggExit();

  return err;
}

FormatSupport::Status FormatOgg::convertStart(const char* from, const char* to)
{
  src_file_ = from;
  dst_file_ = to;

  return oggInit();
}

FormatSupport::Status FormatOgg::convertContinue()
{
  Status err;

  for (int i = 0; i < 4; i++) {
    err = oggDecodeFrame();
    if (err != FS_IN_PROGRESS)
      break;
  }

  if (err != FS_IN_PROGRESS)
    oggExit();

  return err;
}

void FormatOgg::convertAbort()
{
  oggExit();
}

FormatSupport::Status FormatOgg::oggInit()
{
  fin_ = fopen(src_file_, "r");
  if (!fin_) {
    log_message(-2, "Could not open input file \"%s\": %s", src_file_,
            strerror(errno));
    return FS_INPUT_PROBLEM;
  }

  int ovret = ov_open(fin_, &vorbisFile_, NULL, 0);
  if (ovret != 0) {
    log_message(-2, "Could not open Ogg Vorbis file \"%s\"", src_file_);
      return FS_WRONG_FORMAT;
  }

  outFormat_.bits = 16;
  outFormat_.rate = 44100;
  outFormat_.channels = 2;
  outFormat_.byte_format = AO_FMT_NATIVE;
  aoDev_ = ao_open_file(ao_driver_id("wav"), dst_file_, 1, &outFormat_, NULL);
  if (!aoDev_) {
    log_message(-2, "Could not create output file \"%s\": %s", dst_file_,
            strerror(errno));
    return FS_OUTPUT_PROBLEM;
  }
  return FS_SUCCESS;
}

FormatSupport::Status FormatOgg::oggDecodeFrame()
{
  int sec;
  int size = ov_read(&vorbisFile_, buffer_, sizeof(buffer_), 0, 2, 1, &sec);

  if (!size)
    return FS_SUCCESS;

  if (ao_play(aoDev_, buffer_, size) == 0)
    return FS_DISK_FULL;

  return FS_IN_PROGRESS;
}

FormatSupport::Status FormatOgg::oggExit()
{
  ov_clear(&vorbisFile_);
  ao_close(aoDev_);

  return FS_SUCCESS;
}

// ----------------------------------------------------------------
//
// Manager class
//
//

FormatSupport* FormatOggManager::newConverter(const char* extension)
{
  if (strcmp(extension, "ogg") == 0)
    return new FormatOgg;

  return NULL;
}

int FormatOggManager::supportedExtensions(std::list<std::string>& list)
{
  list.push_front("ogg");
  return 1;
}

