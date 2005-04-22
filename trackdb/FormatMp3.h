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

#ifndef __FORMATMP3_H__
#define __FORMATMP3_H__

#include <stdlib.h>
#include <ao/ao.h>
#include <mad.h>

#include "FormatConverter.h"


class FormatMp3 : public FormatSupport
{
 public:
  FormatMp3();

  Status convert(const char* from, const char* to);
  Status convertStart(const char* from, const char* to);
  Status convertContinue();
  void   convertAbort();

  TrackData::FileType format() { return TrackData::WAVE; }

protected:
  struct audio_dither {
    mad_fixed_t error[3];
    mad_fixed_t random;
  };

  static inline unsigned long prng(unsigned long state);
  static inline signed long audio_linear_dither(unsigned int bits,
                                                mad_fixed_t sample,
                                                struct audio_dither* dither);

  Status madInit();
  Status madDecodeFrame();
  void   madExit();
  
  Status madOutput();

 private:
  const char* src_file_;
  const char* dst_file_;
  // 1152 because that's what mad has as a max; *4 because there are 4
  // distinct bytes per sample (in 2 channel case).
  char        buffer_[1152*4];
  ao_device*  out_;
  int         mapped_fd_;
  void*       start_;
  unsigned    length_;

  struct audio_dither dither_;
  struct mad_stream   stream_;
  struct mad_frame    frame_;
  struct mad_synth    synth_;
};

class FormatMp3Manager : public FormatSupportManager
{
 public:
  FormatSupport* newConverter(const char* extension);
  int supportedExtensions(std::list<std::string>&);
};

#endif
