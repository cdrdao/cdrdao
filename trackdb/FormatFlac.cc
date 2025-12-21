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

#include <samplerate.h>

#include "FormatFlac.h"
#include "log.h"
#include "util.h"

//
// FLAC support
//
// Two code paths:
//
// - if the FLAC is sampled at 44.1khz, we do need to resample and we
//   generate the output on the fly.
//
// - if we need to resample, we first load al lthe samples in memory
//   and resample in one block (or there would be data loss at the
//   frame boundaries).
//

FormatFlac::FormatFlac()
{
    started = false;
}

FormatFlac::~FormatFlac()
{
}

FormatSupport::Status FormatFlac::convertStart(std::string from, std::string to)
{
    source_file = from;
    dest_file = to;
    started = false;
    channels = 0;
    need_resampling = false;
    src_samples.clear();

    auto init_st = init(source_file);

    if (init_st != FLAC__STREAM_DECODER_INIT_STATUS_OK)
        return FS_INPUT_PROBLEM;

    if (!process_until_end_of_metadata())
        return FS_INPUT_PROBLEM;
    return FS_SUCCESS;
}

FormatSupport::Status FormatFlac::convertContinue()
{
    if (!process_single()) {
        finalize();
        return FS_OUTPUT_PROBLEM;
    }

    if (get_state() == FLAC__STREAM_DECODER_END_OF_STREAM) {
        if (need_resampling) {
            auto rstatus = resample();
            if (rstatus != FS_SUCCESS)
                return rstatus;
        }
        finalize();
        return FS_SUCCESS;
    }

    return FS_IN_PROGRESS;
}

void FormatFlac::convertAbort()
{
    finalize();
}

bool FormatFlac::setup_output()
{
    src_samples.clear();
    auto ostatus = setup_wav_output(dest_file);
    if (ostatus != FS_SUCCESS)
        return false;

    return true;
}

FormatSupport::Status FormatFlac::resample()
{
    if (need_resampling && src_samples.size() > 0) {
        double ratio = (double)TARGET_SAMPLE_RATE / sample_rate;
        size_t input_frames = src_samples.size() / channels;
        size_t output_frames = static_cast<size_t>(input_frames * ratio) + 10;
        std::vector<float> sroutput(channels * output_frames);

        SRC_DATA src_data;
        src_data.data_in = src_samples.data();
        src_data.data_out = sroutput.data();
        src_data.input_frames = input_frames;
        src_data.output_frames = output_frames;
        src_data.src_ratio = ratio;

        log_message(1, "  resampling...");
        int rs_error = src_simple(&src_data, SRC_SINC_BEST_QUALITY, channels);
        if (rs_error) {
            log_message(-2, "Resampling error: %s", src_strerror(rs_error));
            return FS_OUTPUT_PROBLEM;
        }
        log_message(1, "  %llu 16-bit samples", src_data.output_frames_gen);
        // Convert back to s16
        src_samples.clear();
        size_t num_samples = channels * src_data.output_frames_gen;
        std::vector<s16> pcm_data;
        pcm_data.resize(num_samples);
        src_float_to_short_array(sroutput.data(), pcm_data.data(), num_samples);
        auto wstatus = write_wav_output((char *)pcm_data.data(), pcm_data.size() * 2);
        return wstatus;
    }
    return FS_SUCCESS;
}

void FormatFlac::finalize()
{
    close_wav_output();
}

// ----------------------------------------------------------------
//
// Flac::Decoder callbacks
//
//

FLAC__StreamDecoderWriteStatus FormatFlac::write_callback(const ::FLAC__Frame *frame,
                                                          const FLAC__int32 *const buffer[])
{
    if (!channels)
        channels = frame->header.channels;

    if (!started) {
        if (channels < 1 || !setup_output())
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        started = true;
    }

    auto samples = frame->header.blocksize;
    std::vector<s16> pcm_data;

    if (need_resampling) {
        // Convert to float for libsamplerate
        for (size_t i = 0; i < samples; i++) {
            for (u32 ch = 0; ch < channels; ch++) {
                s32 sample = buffer[ch][i];
                src_samples.push_back((float)sample / (float)(1 << (bits_per_sample - 1)));
            }
        }
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    } else {
        pcm_data.reserve(channels * samples);
        // Convert to interleaved signed 16-bit format.
        for (size_t i = 0; i < samples; i++) {
            for (u32 ch = 0; ch < channels; ch++) {
                s32 sample = buffer[ch][i];
                // Scale to 16-bit range if needed
                if (bits_per_sample < 16) {
                    sample <<= (16 - bits_per_sample);
                } else if (bits_per_sample > 16) {
                    sample >>= (bits_per_sample - 16);
                }
                // Clamp to int16_t range
                if (sample > 32767)
                    sample = 32767;
                if (sample < -32768)
                    sample = -32768;

                pcm_data.push_back(static_cast<s16>(sample));
            }
        }
    }
    write_wav_output((char *)pcm_data.data(), pcm_data.size() * 2);

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FormatFlac::metadata_callback(const ::FLAC__StreamMetadata *metadata)
{
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        sample_rate = metadata->data.stream_info.sample_rate;
        bits_per_sample = metadata->data.stream_info.bits_per_sample;
        channels = metadata->data.stream_info.channels;
        src_samples.reserve(metadata->data.stream_info.total_samples);
        need_resampling = (sample_rate != TARGET_SAMPLE_RATE);
        log_message(1, "Reading FLAC file %s, %lu Hz, %u bps", source_file.c_str(), sample_rate,
                    bits_per_sample);
        log_message(1, "  %llu total samples", metadata->data.stream_info.total_samples);
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

FormatSupport *FormatFlacManager::newConverter(const char *extension)
{
    if (strcasecmp(extension, "flac") == 0)
        return new FormatFlac;

    return NULL;
}

int FormatFlacManager::supportedExtensions(std::list<std::string> &list)
{
    list.push_front("flac");
    list.push_front("FLAC");
    return 1;
}
