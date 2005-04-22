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

#ifndef __FORMATOGG_H__
#define __FORMATOGG_H__

#include <stdlib.h>
#include <ao/ao.h>
#include <vorbis/vorbisfile.h>

#include "FormatConverter.h"


class FormatOgg : public FormatSupport
{
 public:
  FormatOgg() {};

  Status convert(const char* from, const char* to);
  Status convertStart(const char* from, const char* to);
  Status convertContinue();
  void   convertAbort();

  TrackData::FileType format() { return TrackData::WAVE; }

 protected:
  virtual Status oggInit();
  virtual Status oggDecodeFrame();
  virtual Status oggExit();

 private:
  const char*      src_file_;
  const char*      dst_file_;
  char             buffer_[4096];
  FILE*            fin_;
  ao_device*       aoDev_;
  ao_sample_format outFormat_;
  OggVorbis_File   vorbisFile_;
};

class FormatOggManager : public FormatSupportManager
{
 public:
  FormatSupport* newConverter(const char* extension);
  int supportedExtensions(std::list<std::string>&);
};

#endif
