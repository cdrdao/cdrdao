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

#include <string.h>

#include "log.h"
#include "FormatFlac.h"


FormatFlac::FormatFlac()
{
    src_state = NULL;
}

FormatFlac::~FormatFlac()
{
    if (out_)
	ao_close(out_);
}

FormatSupport::Status FormatFlac::convert(const char* from, const char* to)
{
    Status err;
    
    convertStart(from, to);

    while (convertContinue() == FS_IN_PROGRESS);

    return FS_SUCCESS;
}

FormatSupport::Status FormatFlac::convertStart(const char* from, const char* to)
{
    source_file = from;
    dest_file = to;
    out_ = NULL;

    auto init_st = init(source_file);

    if (init_st != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	return FS_INPUT_PROBLEM;

    if (process_until_end_of_stream())
	return FS_SUCCESS;
    else
	return FS_INPUT_PROBLEM;
}

FormatSupport::Status FormatFlac::convertContinue()
{
  return FS_SUCCESS;
}

void FormatFlac::convertAbort()
{
}

TrackData::FileType FormatFlac::format()
{
    return TrackData::WAVE;
}

bool FormatFlac::setupOutput()
{
    // Setup libao for WAV output
    ao_sample_format out_format;
    memset(&out_format, 0, sizeof(out_format));
    out_format.bits = 16;
    out_format.rate = 44100;
    out_format.channels = 2;
    out_format.byte_format = AO_FMT_NATIVE;

    out_ = ao_open_file(ao_driver_id("wav"), dest_file.c_str(), 1,
			&out_format, NULL);
    if (!out_)
	return false;

    // Setup resampling
    int error;
    src_state = src_new(SRC_SINC_MEDIUM_QUALITY, channels, &error);
    if (!src_state)
	return false;

    src_ratio = 44100.0 / (double)sample_rate;

    log_message(1, "Input: %u Hz, %u-bit, target 44100Hz, 16-bit",
		sample_rate, bits_per_sample);

    return true;
}


// ----------------------------------------------------------------
//
// Flac::Decoder callbacks
//
//

FLAC__StreamDecoderWriteStatus
FormatFlac::write_callback(const ::FLAC__Frame *frame,
			   const FLAC__int32 * const buffer[])
{
    channels = frame->header.channels;
    blocksize = frame->header.blocksize;

    if (!out_) {
	if (!setupOutput())
	    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    std::vector<float> inputBuffer(blocksize * channels);
    double scale = (frame->header.bits_per_sample == 24 ? 8388608.0 : 32768.0);
    for (unsigned i = 0; i < blocksize; i++) {
	for (unsigned c = 0; c < channels; c++) {
	    inputBuffer[i * channels + c] = (float)(buffer[c][i] / scale);
	}
    }

    std::vector<float> outputBuffer((size_t)(blocksize * src_ratio + 10)
				    * channels);
    SRC_DATA srcData;
    srcData.data_in = inputBuffer.data();
    srcData.input_frames = blocksize;
    srcData.data_out = outputBuffer.data();
    srcData.output_frames = outputBuffer.size() / channels;
    srcData.src_ratio = src_ratio;
    srcData.end_of_input = 0; // standard processing

    int error = src_process(src_state, &srcData);
    if (error) {
	log_message(-2, "Resampler Error: %s", src_strerror(error));
	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    for (auto f : outputBuffer) {
	int16_t sample = (int16_t)(f * 32767.0f);
	ao_play(out_, (char*)&sample, 2);
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FormatFlac::metadata_callback(const ::FLAC__StreamMetadata *metadata)
{
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
	sample_rate = metadata->data.stream_info.sample_rate;
	bits_per_sample = metadata->data.stream_info.bits_per_sample;
	channels = metadata->data.stream_info.channels;
	samples.resize(metadata->data.stream_info.total_samples);
	log_message(1, "Reading FLAC file %s, sample reate %lu",
		    source_file.c_str(), sample_rate);
    }
    
}

void FormatFlac::error_callback(FLAC__StreamDecoderErrorStatus status)
{
    log_message(-2, "FLAC error callback %d", status);
}


// ----------------------------------------------------------------
//
// Manager class
//
//

FormatSupport* FormatFlacManager::newConverter(const char* extension)
{
  if (strcasecmp(extension, "flac") == 0)
    return new FormatFlac;

  return NULL;
}

int FormatFlacManager::supportedExtensions(std::list<std::string>& list)
{
  list.push_front("flac");
  list.push_front("FLAC");
  return 1;
}

