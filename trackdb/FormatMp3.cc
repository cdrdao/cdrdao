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

//  A lot of this code is taken from mpg321 :
//  mpg321 - a fully free clone of mpg123.
//  Copyright (C) 2001 Joe Drew <drew@debian.org>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "log.h"
#include "FormatMp3.h"


FormatMp3::FormatMp3()
{
  memset(&dither_, 0, sizeof(dither_));
}

FormatSupport::Status FormatMp3::convert(const char* from, const char* to)
{
  src_file_ = from;
  dst_file_ = to;

  Status err = madInit();
  if (err != FS_SUCCESS)
    return err;

  while ((err = madDecodeFrame()) == FS_IN_PROGRESS);

  madExit();

  return err;
}

FormatSupport::Status FormatMp3::convertStart(const char* from, const char* to)
{
  src_file_ = from;
  dst_file_ = to;

  return madInit();
}

FormatSupport::Status FormatMp3::convertContinue()
{
  Status err;

  for (int i = 0; i < 3; i++) {
    err = madDecodeFrame();
    if (err != FS_IN_PROGRESS)
      break;
  }

  if (err != FS_IN_PROGRESS)
    madExit();

  return err;
}

void FormatMp3::convertAbort()
{
  madExit();
}

FormatSupport::Status FormatMp3::madInit()
{
  struct stat st;

  if (stat(src_file_, &st) != 0) {
    log_message(-2, "Could not stat input file \"%s\": %s", src_file_,
		strerror(errno));
    return FS_INPUT_PROBLEM;
  }

  mapped_fd_ = open(src_file_, O_RDONLY);
  if (!mapped_fd_) {
    log_message(-2, "Could not open input file \"%s\": %s", src_file_,
            strerror(errno));
    return FS_INPUT_PROBLEM;
  }

  length_ = st.st_size;
  start_ = mmap(0, st.st_size, PROT_READ, MAP_SHARED, mapped_fd_, 0);
  if (start_ == MAP_FAILED) {
    log_message(-2, "Could not map file \"%s\" into memory: %s", src_file_,
            strerror(errno));
    return FS_INPUT_PROBLEM;
  }

  // Initialize libao for WAV output;
  ao_sample_format out_format;
  out_format.bits = 16;
  out_format.rate = 44100;
  out_format.channels = 2;
  out_format.byte_format = AO_FMT_NATIVE;

  out_ = ao_open_file(ao_driver_id("wav"), dst_file_, 1, &out_format, NULL);

  if (!out_) {
    log_message(-2, "Could not create output file \"%s\": %s", dst_file_,
            strerror(errno));
    return FS_OUTPUT_PROBLEM;
  }

  // Initialize libmad input stream.
  mad_stream_init(&stream_);
  mad_frame_init(&frame_);
  mad_synth_init(&synth_);
  mad_stream_options(&stream_, 0);
  mad_stream_buffer(&stream_, (unsigned char*)start_, length_);

  return FS_SUCCESS;
}

FormatSupport::Status FormatMp3::madDecodeFrame()
{
  if (mad_frame_decode(&frame_, &stream_) == -1) {

    if (stream_.error != MAD_ERROR_BUFLEN &&
        stream_.error != MAD_ERROR_LOSTSYNC) {
      log_message(-1, "Decoding error 0x%04x (%s) at byte offset %u",
              stream_.error, mad_stream_errorstr(&stream_),
              stream_.this_frame - (unsigned char*)start_);
    }

    if (stream_.error == MAD_ERROR_BUFLEN)
      return FS_SUCCESS;

    if (!MAD_RECOVERABLE(stream_.error))
      return FS_DECODE_ERROR;
  }

  mad_synth_frame(&synth_, &frame_);
  madOutput();
  return FS_IN_PROGRESS;
}

void FormatMp3::madExit()
{
  mad_synth_finish(&synth_);
  mad_frame_finish(&frame_);
  mad_stream_finish(&stream_);

  munmap(start_, length_);
  close(mapped_fd_);
  ao_close(out_);
}

unsigned long FormatMp3::prng(unsigned long state)
{
  return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

signed long FormatMp3::audio_linear_dither(unsigned int bits,
                                           mad_fixed_t sample,
                                           struct audio_dither *dither)
{
  unsigned int scalebits;
  mad_fixed_t output, mask, random;

  enum {
    MIN = -MAD_F_ONE,
    MAX =  MAD_F_ONE - 1
  };

  /* noise shape */
  sample += dither->error[0] - dither->error[1] + dither->error[2];

  dither->error[2] = dither->error[1];
  dither->error[1] = dither->error[0] / 2;

  /* bias */
  output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

  scalebits = MAD_F_FRACBITS + 1 - bits;
  mask = (1L << scalebits) - 1;

  /* dither */
  random  = prng(dither->random);
  output += (random & mask) - (dither->random & mask);

  dither->random = random;

  /* clip */
  if (output > MAX) {
    output = MAX;

    if (sample > MAX)
      sample = MAX;
  }
  else if (output < MIN) {
    output = MIN;

    if (sample < MIN)
      sample = MIN;
  }

  /* quantize */
  output &= ~mask;

  /* error feedback */
  dither->error[0] = sample - output;

  /* scale */
  return output >> scalebits;
}

FormatSupport::Status FormatMp3::madOutput()
{
  struct mad_pcm* pcm = &synth_.pcm;
  register int nsamples = pcm->length;
  mad_fixed_t const *left_ch = pcm->samples[0], *right_ch = pcm->samples[1];
    
  register char* ptr = buffer_;
  register signed int sample;
  register mad_fixed_t tempsample;

  if (pcm->channels == 2) {
    while (nsamples--) {
      tempsample = (mad_fixed_t)(*left_ch++);
      sample = (signed int)audio_linear_dither(16, tempsample,&dither_);

#ifndef WORDS_BIGENDIAN
      *ptr++ = (unsigned char)(sample >> 0);
      *ptr++ = (unsigned char)(sample >> 8);
#else
      *ptr++ = (unsigned char)(sample >> 8);
      *ptr++ = (unsigned char)(sample >> 0);
#endif
            
      tempsample = (mad_fixed_t)(*right_ch++);
      sample = (signed int)audio_linear_dither(16, tempsample, &dither_);
#ifndef WORDS_BIGENDIAN
      *ptr++ = (unsigned char)(sample >> 0);
      *ptr++ = (unsigned char)(sample >> 8);
#else
      *ptr++ = (unsigned char)(sample >> 8);
      *ptr++ = (unsigned char)(sample >> 0);
#endif
    }

    if (ao_play(out_, buffer_, pcm->length * 4) == 0)
      return FS_DISK_FULL;

  } else {
    while (nsamples--) {
      tempsample = (mad_fixed_t)((*left_ch++)/MAD_F_ONE);
      sample = (signed int)audio_linear_dither(16, tempsample, &dither_);
            
      /* Just duplicate the sample across both channels. */
#ifndef WORDS_BIGENDIAN
      *ptr++ = (unsigned char)(sample >> 0);
      *ptr++ = (unsigned char)(sample >> 8);
      *ptr++ = (unsigned char)(sample >> 0);
      *ptr++ = (unsigned char)(sample >> 8);
#else
      *ptr++ = (unsigned char)(sample >> 8);
      *ptr++ = (unsigned char)(sample >> 0);
      *ptr++ = (unsigned char)(sample >> 8);
      *ptr++ = (unsigned char)(sample >> 0);
#endif
    }

    if (ao_play(out_, buffer_, pcm->length * 4) == 0)
      return FS_DISK_FULL;
  }

  return FS_SUCCESS;
}

// ----------------------------------------------------------------
//
// Manager class
//
//

FormatSupport* FormatMp3Manager::newConverter(const char* extension)
{
  if (strcmp(extension, "mp3") == 0)
    return new FormatMp3;

  return NULL;
}

int FormatMp3Manager::supportedExtensions(std::list<std::string>& list)
{
  list.push_front("mp3");
  return 1;
}

