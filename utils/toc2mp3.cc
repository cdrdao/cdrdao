/*  toc2mp3 - encodes a audio CD disk image to mp3 files for each track
 *
 *  Copyright (C) 2002 Andreas Mueller <andreas@daneb.de>
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
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <lame/lame.h>

#include "util.h"
#include "Toc.h"
#include "CdTextItem.h"

// set desired default bit rate for encoding here:
#define DEFAULT_ENCODER_BITRATE 192

static const char *PRGNAME = NULL;
static int VERBOSE = 1;
static int CREATE_ALBUM_DIRECTORY = 0;
static std::string TARGET_DIRECTORY;


void message_args(int level, int addNewLine, const char *fmt, va_list args)
{
  long len = strlen(fmt);
  char last = len > 0 ? fmt[len - 1] : 0;

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
    if (addNewLine) {
      if (last != ' ' && last != '\r')
	fprintf(stderr, "\n");
    }

    fflush(stderr);
    if (level <= -10)
      exit(1);
  }
  else if (level <= VERBOSE) {
    vfprintf(stderr, fmt, args);

    if (addNewLine) {
      if (last != ' ' && last != '\r')
	fprintf(stderr, "\n");
    }

    fflush(stderr);
  }
}

void message(int level, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  message_args(level, 1, fmt, args);

  va_end(args);
}

void lame_message(const char *fmt, va_list args)
{
  message_args(1, 0, fmt, args);
}

void lame_error_message(const char *fmt, va_list args)
{
  message_args(-2, 0, fmt, args);
}


static void printVersion()
{
  message(1, "toc2mp3 version %s - (C) Andreas Mueller <andreas@daneb.de>",
	  VERSION);
  message(1, "");
}

static void printUsage()
{
  message(0, "Usage: %s [-v #] [-d target-dir ] [-c] { -V | toc-file }", PRGNAME);
  message(0, "\nConverts an audio CD disk image (.toc file) to mp3 files.");
  message(0, "Each track will be written to a separate mp3 file.");
  message(0, "Special care is taken that the mp3 files can be played in sequence");
  message(0, "without having unwanted noise at the transition points.");
  message(0, "CD-TEXT information (if available) is used to set ID3 (v2) tags and to");
  message(0, "construct the name of the mp3 files.\n");
  message(0, "Options:");
  message(0, "  -h               Shows this help.");
  message(0, "  -v <n>           Sets verbose level to <n> (0..2).");
  message(0, "  -d <target-dir>  Specifies directory the mp3 files will be");
  message(0, "                   written to.");
  message(0, "  -c               Adds a sub-directory composed out of CD title");
  message(0, "                   and author to <target-dir> specified with -d.");
  message(0, "  -b <bit rate>    Sets bit rate used for encoding (default %d kbit/s).",
	  DEFAULT_ENCODER_BITRATE);
  message(0, "                   See below for supported bit rates.");

  message(0, "");

  message(0, "LAME encoder version: %s", get_lame_version());
  message(0, "Supported bit rates: ");
  for (int i = 0; i < 16 && bitrate_table[1][i] >= 0; i++) {
    message(0, "%d ", bitrate_table[1][i]);
  }
  message(0, "");
}

static int parseCommandLine(int argc, char **argv, char **tocFile, int *bitrate)
{
  int c;
  int printVersion = 0;
  extern char *optarg;
  extern int optind, opterr, optopt;

  opterr = 0;

  while ((c = getopt(argc, argv, "Vhcv:d:b:")) != EOF) {
    switch (c) {
    case 'V':
      printVersion = 1;
      break;

    case 'v':
      if (optarg != NULL) {
	if ((VERBOSE = atoi(optarg)) < 0) {
	  message(-2, "Invalid verbose level: %s", optarg);
	  return 0;
	}
      }
      else {
	message(-2, "Missing verbose level after option '-v'.");
	return 0;
      }
      break;

    case 'b':
      if (optarg != NULL) {
	*bitrate = atoi(optarg);
      }
      else {
	message(-2, "Missing bit rate value after option '-b'.");
	return 0;
      }
      break;

    case 'c':
      CREATE_ALBUM_DIRECTORY = 1;
      break;

    case 'h':
      return 0;
      break;

    case 'd':
      if (optarg != NULL) {
	TARGET_DIRECTORY = optarg;
      }
      else {
	message(-2, "Missing target directory after option '-d'.");
	return 0;
      }
      break;

    case '?':
      message(-2, "Invalid option: %c", optopt);
      return 0;
      break;
    }
  }

  if (printVersion) {
    return 1;
  }

  if (optind < argc) {
    *tocFile = strdupCC(argv[optind]);
    optind++;
  }
  else {
    message(-2, "Missing toc-file name.");
    return 0;
  }

  if (optind != argc) {
    message(-2, "More arguments than expected.");
    return 0;
  }

  return 2;
}

lame_global_flags *init_encoder(int bitrate)
{
  lame_global_flags *lf;
  int bitrateOk = 0;

  for (int i = 0; bitrate_table[1][i] >= 0 && !bitrateOk; i++) {
    if (bitrate == bitrate_table[1][i])
      bitrateOk = 1;
  }

  if (!bitrateOk) {
    message(-2, "Invalid bit rate: %d kbit/s", bitrate);
    return NULL;
  }

  if ((lf = lame_init()) == NULL) {
    return NULL;
  }

  lame_set_msgf(lf, lame_message);
  lame_set_debugf(lf, lame_message);
  lame_set_errorf(lf, lame_error_message);

  lame_set_in_samplerate(lf, 44100);

  lame_set_num_channels(lf, 2);

  lame_set_quality(lf, 2);

  lame_set_mode(lf, STEREO);

  lame_set_brate(lf, bitrate);
  
  //lame_set_VBR(lf, vbr_abr);

  //lame_set_VBR(lf, vbr_mtrh);
  //lame_set_VBR_q(lf, 2);

  //lame_set_VBR_mean_bitrate_kbps(lf, bitrate);
  //lame_set_VBR_min_bitrate_kbps(lf, 112);
  //lame_set_VBR_hard_min(lf, 1);

  //lame_set_bWriteVbrTag(lf, 1);

  //lame_set_asm_optimizations(lf, AMD_3DNOW, 1);

  return lf;
}

void set_id3_tags(lame_global_flags *lf, int tracknr, const std::string &title,
		  const std::string &artist, const std::string &album)
{
  char buf[100];

  id3tag_init(lf);

  id3tag_add_v2(lf);

  if (!title.empty())
    id3tag_set_title(lf, title.c_str());

  if (!artist.empty())
    id3tag_set_artist(lf, artist.c_str());

  if (!album.empty())
    id3tag_set_album(lf, album.c_str());

  if (tracknr > 0 && tracknr <= 255) {
    sprintf(buf, "%d", tracknr);
    id3tag_set_track(lf, buf);
  }
}

int encode_track(lame_global_flags *lf, const Toc *toc, 
		 const std::string &fileName, long startLba,
		 long len)
{
  int fd;
  int ret = 1;
  TocReader reader(toc);
  Sample audioData[SAMPLES_PER_BLOCK];
  short int leftSamples[SAMPLES_PER_BLOCK];
  short int rightSamples[SAMPLES_PER_BLOCK];
  unsigned char mp3buffer[LAME_MAXMP3BUFFER];

  if (reader.openData() != 0) {
    message(-2, "Cannot open audio data.");
    return 0;
  }

  if (reader.seekSample(Msf(startLba).samples()) != 0) {
    message(-2, "Cannot seek to start sample of track.");
    return 0;
  }

  if ((fd = open(fileName.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
    message(-2, "Cannot open \"%s\" for writing: %s", fileName.c_str(),
	    strerror(errno));
    return 0;
  }

  while (len > 0) {
    if (reader.readSamples(audioData, SAMPLES_PER_BLOCK) != SAMPLES_PER_BLOCK) {
      message(-2, "Cannot read audio data.");
      ret = 0;
      break;
    }

    for (int i = 0; i < SAMPLES_PER_BLOCK; i++) {
      leftSamples[i] = audioData[i].left();
      rightSamples[i] = audioData[i].right();
    }

    int count = lame_encode_buffer(lf, leftSamples, rightSamples,
				   SAMPLES_PER_BLOCK, mp3buffer,
				   sizeof(mp3buffer));

    if (count < 0) {
      message(-2, "Lame encoder failed: %d", count);
      ret = 0;
      break;
    }

    if (count > 0) {
      if (fullWrite(fd, mp3buffer, count) != count) {
	message(-2, "Failed to write encoded data: %s", strerror(errno));
	ret = 0;
	break;
      }
    }

    len--;
  }
    
  if (ret != 0) {
    int count = lame_encode_flush_nogap(lf, mp3buffer, sizeof(mp3buffer));

    if (count > 0) {
      if (fullWrite(fd, mp3buffer, count) != count) {
	message(-2, "Failed to write encoded data: %s", strerror(errno));
	ret = 0;
      }
    }
  }

  if (close(fd) != 0) {
    message(-2, "Failed to close encoded data file: %s", strerror(errno));
    ret = 0;
  }

  if (ret != 0) {
    FILE *fp = fopen(fileName.c_str(), "a+");

    if (fp != NULL) {
      lame_mp3_tags_fid(lf, fp);
      fclose(fp);
    }
    else {
      message(-2, "Cannot reopen output file for adding headers: %s",
	      strerror(errno));
      ret = 0;
    }
  }

  return ret;
}

std::string &clean_string(std::string &s)
{
  int i = 0;
  int len = s.length();
  char c;

  while (i < len && (c = s[i]) != 0) {
    if (c == '_') {
      s[i] = ' ';
      i++;
    }
    else if (isalnum(c) || c == ' ' || c == '.' || c== '-' || c == '(' ||
	     c == ')' || c == ')' || c == ',' || c == ':' || c == ';' ||
	     c == '"' || c == '!' || c == '?' || c == '\'' || c == '$')  {
      i++;
    }
    else {
      s.erase(i, 1);
      len = s.length();
    }
  }

  len = s.length();

  for (i = 0; i < len && s[i] != 0 && isspace(s[i]); i++) ;

  if (i >= len) {
    // string just contains space
    s = "";
  }

  return s;
}

int main(int argc, char **argv)
{
  char *tocFile;
  int bitrate = DEFAULT_ENCODER_BITRATE;
  Toc *toc;
  lame_global_flags *lf;
  std::string album, albumPerformer, title, performer;
  int cdTextLanguage = 0;
  const CdTextItem *cdTextItem;
  char *tocfileBaseName, *p;
  char sbuf[100];
  int err = 0;

  PRGNAME = *argv;

  switch (parseCommandLine(argc, argv, &tocFile, &bitrate)) {
  case 0:
    printUsage();
    exit(1);
    break;
    
  case 1:
    printf("%s\n", VERSION);
    exit(0);
    break;
  }

  printVersion();

  if ((toc = Toc::read(tocFile)) == NULL) {
    message(-10, "Failed to read toc-file '%s'.", tocFile);
  }

  if ((lf = init_encoder(bitrate)) == NULL) {
    message(-10, "Cannot initialize lame encoder");
  }


  if ((p = strrchr(tocFile, '/')) != NULL)
    tocfileBaseName = strdupCC(p + 1);
  else
    tocfileBaseName = strdupCC(tocFile);

  if ((p = strrchr(tocfileBaseName, '.')) != NULL &&
      (strcmp(p, ".toc") == 0 || strcmp(p, ".cue") == 0)) {
    *p = 0;
  }

  if (strlen(tocfileBaseName) == 0) {
    delete[] tocfileBaseName;
    tocfileBaseName = strdupCC("unknown");
  }

  if ((cdTextItem = toc->getCdTextItem(0, cdTextLanguage, 
				       CdTextItem::CDTEXT_TITLE)) != NULL) {
    album = (const char*)cdTextItem->data();
    clean_string(album);
    if (album.empty())
      album = tocfileBaseName;
  }
  else {
    album = tocfileBaseName;
  }

  if ((cdTextItem = toc->getCdTextItem(0, cdTextLanguage, 
				       CdTextItem::CDTEXT_PERFORMER)) != NULL) {
    albumPerformer = (const char*)cdTextItem->data();
    clean_string(albumPerformer);
  }
  else {
    albumPerformer = "";
  }


  std::string mp3TargetDir;

  if (!TARGET_DIRECTORY.empty()) {
    mp3TargetDir = TARGET_DIRECTORY;

    if (*(TARGET_DIRECTORY.end() - 1) != '/')
      mp3TargetDir += "/";
    
    if (CREATE_ALBUM_DIRECTORY) {
      if (!album.empty() && !albumPerformer.empty()) {
	mp3TargetDir += albumPerformer;
	mp3TargetDir += "_";
	mp3TargetDir += album;
      }
      else {
	mp3TargetDir += tocfileBaseName;
      }

      if (mkdir(mp3TargetDir.c_str(), 0777) != 0) {
	message(-10, "Cannot create album directory \"%s\": %s",
		mp3TargetDir.c_str(), strerror(errno));
      }

      mp3TargetDir += "/";
    }
  }


  Msf astart, aend, nstart, nend;
  const Track *actTrack, *nextTrack;
  int trackNr;
  TrackIterator titr(toc);
  int firstEncodedTrack = 1;

  trackNr = 1;
  actTrack = titr.first(astart, aend);
  nextTrack = titr.next(nstart, nend);
  
  while (actTrack != NULL && err == 0) {

    if (actTrack->type() == TrackData::AUDIO) {

      // Retrieve CD-TEXT data for track title and performer
      if ((cdTextItem = toc->getCdTextItem(trackNr, cdTextLanguage, 
					   CdTextItem::CDTEXT_TITLE)) != NULL) {
	title = (const char*)cdTextItem->data();
	clean_string(title);
      }
      else {
	title = "";
      }

      if ((cdTextItem = toc->getCdTextItem(trackNr, cdTextLanguage, 
					   CdTextItem::CDTEXT_PERFORMER)) != NULL) {
	performer = (const char*)cdTextItem->data();
	clean_string(performer);
      }
      else {
	performer = "";
      }
      
      // build mp3 file name
      std::string mp3FileName;
      
      sprintf(sbuf, "%02d_", trackNr);
      
      mp3FileName += sbuf;
      
      if (!title.empty()) {
	mp3FileName += title;
	mp3FileName += "_";
      }
      
      mp3FileName += album;
      
      if (!albumPerformer.empty()) {
	mp3FileName += "_";
	mp3FileName += albumPerformer;
      }
      
      mp3FileName += ".mp3";


      long len = aend.lba() - astart.lba();
      
      if (nextTrack != NULL && nextTrack->type() == TrackData::AUDIO)
	len += nextTrack->start().lba();

      if (len > 0) {
	set_id3_tags(lf, trackNr, title, performer, album);

	if (firstEncodedTrack) {
	  if (lame_init_params(lf) < 0) {
	    message(-2, "Setting of lame parameters failed");
	    err = 1;
	    break;
	  }
	  message(1, "Lame encoder settings:");
	  lame_print_config(lf);
	  message(1, "Selected bit rate: %d kbit/s", bitrate);

	  if (VERBOSE >= 2)
	    lame_print_internals(lf);

	  message(1, "");


	  message(1, "Starting encoding to target directory \"%s\"...",
		  mp3TargetDir.empty() ? "." : mp3TargetDir.c_str());

	  firstEncodedTrack = 0;
	}
	else {
	  if (lame_init_bitstream(lf) != 0) {
	    message(-2, "Cannot initialize bit stream.");
	    err = 1;
	    break;
	  }
	}

	message(1, "Encoding track %d to \"%s\"...", trackNr,
		mp3FileName.c_str());

	if (!encode_track(lf, toc, mp3TargetDir + mp3FileName, astart.lba(), len)) {
	  message(-2, "Encoding of track %d failed.", trackNr);
	  err = 1;
	  break;
	}
      }
    }

    actTrack = nextTrack;
    astart = nstart;
    aend = nend;
    trackNr++;

    if (actTrack != NULL)
      nextTrack = titr.next(nstart, nend);
  }

  lame_close(lf);

  exit(err);
}
