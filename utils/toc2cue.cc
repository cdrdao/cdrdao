/*  toc2cue - converts cdrdao's toc-files to .cue files
 *
 *  Copyright (C) 2001 Andreas Mueller <andreas@daneb.de>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <fstream>

#include "util.h"
#include "Toc.h"

static const char *PRGNAME = "toc2cue";
static int VERBOSE = 1;

void message(int level, const char *fmt, ...)
{
  long len = strlen(fmt);
  char last = len > 0 ? fmt[len - 1] : 0;

  va_list args;
  va_start(args, fmt);

  if (level < 0) {
    switch (level) {
    case -1:
      fprintf(stderr, "WARNING: ");
      break;
    case -2:
      fprintf(stderr, "ERROR: ");
      break;
    case -3:
      fprintf(stderr, "INTERNAL ERROR: ");
      break;
    default:
      fprintf(stderr, "FATAL ERROR: ");
      break;
    }
    vfprintf(stderr, fmt, args);
    if (last != ' ' && last != '\r')
      fprintf(stderr, "\n");

    fflush(stderr);
    if (level <= -10)
      exit(1);
  }
  else if (level <= VERBOSE) {
    vfprintf(stderr, fmt, args);
    if (last != ' ' && last != '\r')
      fprintf(stderr, "\n");

    fflush(stderr);
  }

  va_end(args);
}

long sectorSize(TrackData::Mode m)
{
  long bsize = 0;

#if 1
  // force block size to 2352
  bsize = AUDIO_BLOCK_LEN;
#else
  switch (m) {
  case TrackData::AUDIO:
      bsize = AUDIO_BLOCK_LEN;
      break;
  case TrackData::MODE1:
  case TrackData::MODE1_RAW:
      bsize = MODE1_BLOCK_LEN;
      break;
  case TrackData::MODE2:
  case TrackData::MODE2_RAW:
  case TrackData::MODE2_FORM1:
  case TrackData::MODE2_FORM2:
  case TrackData::MODE2_FORM_MIX:
      bsize = MODE2_BLOCK_LEN;
      break;
  case TrackData::MODE0:
      message(-3, "Illegal mode in 'CdrDriver::blockSize()'.");
      break;
  }
#endif

  return bsize;
}

int convertBin(Toc *toc, char *oldBin, char *newBin, int byteSwap)
{
    Msf start, end;
    const Track *trun;
    int trackNr;
    TrackIterator titr(toc);
    FILE *in, *out;

    in = fopen(oldBin, "r");
    if(!in)
    {
	message(-2, "Failed to open input bin file %s", oldBin);
	return 1;
    }

    out = fopen(newBin, "w+");
    if(!out)
    {
	message(-2, "Failed to open output bin file %s", newBin);
	fclose(in);
	return 1;
    }

    for (trun = titr.first(start, end), trackNr = 1;
	 trun != NULL;
	 trun = titr.next(start, end), trackNr++)
    {
	TrackData::Mode mode = trun->type();
	int iblksize = sectorSize(mode) + TrackData::subChannelSize(trun->subChannelType());
	int oblksize = sectorSize(mode);

	if(mode == TrackData::AUDIO && byteSwap)
	{
	    printf("  byte swapping audio samples in track %d\n", trackNr);
	}

	char buf[AUDIO_BLOCK_LEN + MAX_SUBCHANNEL_LEN];
	int cnt;
	for(cnt = 0; cnt < (end.lba() - start.lba()); cnt++)
	{
	    int num_read, num_written = 0;
	    num_read = fread(buf, 1, iblksize, in);

	    if(mode == TrackData::AUDIO && byteSwap)
	    {
		swapSamples((Sample *)buf, SAMPLES_PER_BLOCK);
	    }

	    num_written = fwrite(buf, 1, oblksize, out);
	}

	printf("  wrote %d sectors\n", cnt);
    }

    fclose(in);
    fclose(out);

    return 0;
}

static void printVersion()
{
  message(1, "toc2cue version %s - (C) Andreas Mueller <andreas@daneb.de>",
	  VERSION);
  message(1, "");
}

static void printUsage()
{
  message(0, "Usage: %s [-v #] [-s] [-C output-bin-file] { -V | -h | input-toc-file output-cue-file}", PRGNAME);
}

static void printHelp()
{
  printUsage();
  message(0, "Convert a toc file to a cue file for use with other programs.\n");
  message(0, "Options:");
  message(0, "  -v    set verbosity level");
  message(0, "  -C    try to convert the bin file referenced by the toc to a format\n        compatible with cue files");
  message(0, "  -s    when -C is specified, byte swap audio data when writing new bin file");
  message(0, "\nMutually exclusive options:");
  message(0, "  -h    print this help text");
  message(0, "  -V    print the version number");
  message(0, "  none  convert the provided toc file to a cue file");
}

static int parseCommandLine(int argc, char **argv, char **tocFile,
			    char **cueFile, char **convertedBinFile, int *byteSwap)
{
  int c;
  int printVersion = 0;
  int printHelpInfo = 0;
  extern char *optarg;
  extern int optind, opterr, optopt;

  opterr = 0;

  while ((c = getopt(argc, argv, "Vhsv:C:")) != EOF) {
    switch (c) {
    case 'V':
      printVersion = 1;
      break;

    case 'v':
      if (optarg != NULL) {
	if ((VERBOSE = atoi(optarg)) < 0) {
	  message(-2, "Invalid verbose level: %s\n", optarg);
	  return 0;
	}
      }
      else {
	message(-2, "Missing verbose level after option '-v'.\n");
	return 0;
      }
      break;

    case 'h':
      printHelpInfo = 1;
      break;

    case 's':
      *byteSwap = 1;
      break;

    case 'C':
      *convertedBinFile = strdupCC(optarg);
      break;

    case '?':
      message(-2, "Invalid option: %c\n", optopt);
      return 0;
      break;

    case ':':
      message(-2, "option %c missing argument\n", optopt);
      return 0;
      break;
    }
  }

  if (printVersion) {
    printf("%s\n", VERSION);
    return 1;
  }

  if (printHelpInfo) {
    printHelp();
    return 1;
  }

  if (optind < argc) {
    *tocFile = strdupCC(argv[optind]);
    optind++;
  }
  else {
    message(-2, "Missing toc-file name.\n");
    return 0;
  }

  if (optind < argc) {
    *cueFile = strdupCC(argv[optind]);
    optind++;
  }
  else {
    message(-2, "Missing cue file name.\n");
    return 0;
  }

  if (optind != argc) {
    message(-2, "Expecting exactly two arguments.\n");
    return 0;
  }

  return 2;
}

int main(int argc, char **argv)
{
  char *tocFile, *cueFile, *convertedBinFile = NULL;
  Toc *toc;
  int byteSwapBinAudio = 0;

  switch (parseCommandLine(argc, argv, &tocFile, &cueFile, &convertedBinFile, &byteSwapBinAudio)) {
  case 0: // error
    printUsage();
    return 1;
    break;

  case 1: // return early
    return 0;
    break;
  }

  printVersion();

  if ((toc = Toc::read(tocFile)) == NULL) {
    message(-2, "Failed to read toc-file '%s'.", tocFile);
    return 1;
  }

  Msf start, end;
  const Track *trun;
  int trackNr;
  TrackIterator titr(toc);
  char *binFileName = NULL;
  int err = 0;
  int subchan_data_found = 0;

  // first make some consistency checks, surely not complete to identify
  // toc-files that can be correctly converted to cue files
  for (trun = titr.first(start, end), trackNr = 1;
       trun != NULL;
       trun = titr.next(start, end), trackNr++) {
    const SubTrack *strun;
    int stcount;
    TrackData::Type sttype1 = TrackData::DATAFILE,
        sttype2 = TrackData::DATAFILE;
    SubTrackIterator stitr(trun);

    switch (trun->type()) {
    case TrackData::MODE0:
    case TrackData::MODE2_FORM2:
      message(-2, "Cannot convert: track %d has unsupported mode.", trackNr);
      err = 1;
      break;
    default:
      break;
    }

    // Check for the presence of subchannel data
    TrackData::SubChannelMode subchan_mode = trun->subChannelType();
    if(subchan_mode) {
      subchan_data_found = 1;
    }

    for (strun = stitr.first(), stcount = 0;
	 strun != NULL;
	 strun = stitr.next(), stcount++) {

      // store types of first two sub-tracks for later evaluation
      switch (stcount) {
      case 0:
	sttype1 = strun->TrackData::type();
	break;
      case 1:
	sttype2 = strun->TrackData::type();
	break;
      }

      // check if whole toc-file just references a single bin file
      if (strun->TrackData::type() == TrackData::DATAFILE) {
	if (binFileName == NULL) {
	  binFileName = strdupCC(strun->filename());
	}
	else {
	  if (strcmp(binFileName, strun->filename()) != 0) {
	    message(-2, "Cannot convert: toc-file references multiple data files.");
	    err = 1;
	  }
	}
      }
    }

    switch (stcount) {
    case 0:
      message(-2, "Cannot convert: track %d references no data file.",
	      trackNr);
      err = 1;
      break;

    case 1:
      if (sttype1 != TrackData::DATAFILE) {
	message(-2, "Cannot convert: track %d references no data file.",
	      trackNr);
	err = 1;
      }
      break;

    case 2:
      if (sttype1 != TrackData::ZERODATA || sttype2 != TrackData::DATAFILE) {
	message(-2, "Cannot convert: track %d has unsupported layout.",
		trackNr);
	err = 1;
      }
      break;

    default:
      message(-2, "Cannot convert: track %d has unsupported layout.", trackNr);
      err = 1;
      break;
    }
  }

  if (binFileName == NULL) {
    message(-2, "Cannot convert: toc-file references no data file.");
    err = 1;
  }

  if (err) {
    message(-2, "Cannot convert toc-file '%s' to a cue file.", tocFile);
    return 1;
  }

  std::ofstream out(cueFile);

  if (!out) {
    message(-2, "Cannot open cue file \'%s\' for writing: %s", cueFile,
	    strerror(errno));
    return 1;
  }

  if(convertedBinFile) {
    // cue file needs to reference the new bin file
    out << "FILE \"" << convertedBinFile << "\" BINARY" << "\n";
  }
  else {
    out << "FILE \"" << binFileName << "\" BINARY" << "\n";
  }

  long offset = 0;

  for (trun = titr.first(start, end), trackNr = 1;
       trun != NULL;
       trun = titr.next(start, end), trackNr++) {
    char buf[20];

    sprintf(buf, "%02d ", trackNr);
    out << "  TRACK " << buf;

    switch (trun->type()) {
    case TrackData::AUDIO:
      out << "AUDIO";
      break;
    case TrackData::MODE1:
    case TrackData::MODE2_FORM1:
      out << "MODE1/2048";
      break;
    case TrackData::MODE2:
    case TrackData::MODE2_FORM_MIX:
      out << "MODE2/2336";
      break;
    case TrackData::MODE1_RAW:
      out << "MODE1/2352";
      break;
    case TrackData::MODE2_RAW:
      out << "MODE2/2352";
      break;
    default:
      break;
    }

    out << "\n";

    if ( trun->copyPermitted() ) {
      out << "    FLAGS DCP\n";
    }

    const SubTrack *strun;
    SubTrackIterator stitr(trun);
    int pregap = 0;

    for (strun = stitr.first(); strun != NULL; strun = stitr.next()) {
      if (strun->TrackData::type() == TrackData::ZERODATA) {
	out << "    PREGAP " << trun->start().str() << "\n";
	pregap = 1;
      }
      else {
	if (!pregap && trun->start().lba() != 0) {
	  out << "    INDEX 00 " << Msf(offset).str() << "\n";
	  out << "    INDEX 01 "
	      << Msf(offset + trun->start().lba()).str() << "\n";
	}
	else {
	  out << "    INDEX 01 " << Msf(offset).str() << "\n";
	}

	offset += trun->length().lba();

	if (pregap)
	  offset -= trun->start().lba();
      }
    }
  }

  out.close();

  message(1, "Converted toc-file '%s' to cue file '%s'.",
	  tocFile, cueFile);
  message(1, "");

  if(convertedBinFile) {
    message(1, "Converting bin file...");
    if(convertBin(toc, binFileName, convertedBinFile, byteSwapBinAudio)) {
      message(1, "Error converting bin file '%s' to '%s'", binFileName, convertedBinFile);
      message(1, "");
    }
    else {
      message(1, "Converted bin file '%s' to '%s'", binFileName, convertedBinFile);
      message(1, "");
    }
  }

  if(subchan_data_found && !convertedBinFile) {
    message(1, "Subchannel data detected on one or more tracks. The cue/bin");
    message(1, "pair may not be compatible with third-party software. You can");
    message(1, "use the -C option to create a bin file with subchannel data");
    message(1, "removed.");
    message(1, "");
  }

  message(1, "Please note that the resulting cue file is only valid if the");
  message(1, "toc-file was created with cdrdao using the commands 'read-toc'");
  message(1, "or 'read-cd'. For manually created or edited toc-files the");
  message(1, "cue file may not be correct. This program just checks for");
  message(1, "the most obvious toc-file features that cannot be converted to");
  message(1, "a cue file.");
  message(1, "");
  message(1, "Furthermore, if the toc-file contains audio tracks the byte");
  message(1, "order of the image file will be wrong which results in static");
  message(1, "noise when the resulting cue file is used for recording");
  message(1, "(even with cdrdao itself). This can be corrected by creating");
  message(1, "a new image file with the -C option and specifying the -s flag");
  message(1, "to byte swap the audio tracks.");

  return 0;
}
