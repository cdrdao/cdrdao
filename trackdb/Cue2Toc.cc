/* cue2toc.c - conversion routines
 * Copyright (C) 2004 Matthias Czapla <dermatsch@gmx.de>
 *
 * This file is part of cue2toc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "Cue2Toc.h"
#include "TrackData.h"

#define TCBUFLEN 9	/* Buffer length for timecode strings (HH:MM:SS) */
#define MAXCMDLEN 10	/* Longest command (currently SONGWRITER) */

long tc2fr(const char *);
int fr2tc(char *, long fr);

/*
 * Input is divied into tokens that are separated by whitespace, horizantal
 * tabulator, line feed and carriage return. Tokens can be either commands
 * from a fixed set or strings. If a string is to contain any of the token
 * delimiting characters it must be enclosed in double quotes.
 */

static const char token_delimiter[] = { ' ', '\t', '\n', '\r' };

/* Return true if c is one of token_delimiter */
static int
isdelim(int c)
{
    int i;
    int n = sizeof(token_delimiter);

    for (i = 0; i < n; i++)
        if (c == token_delimiter[i])
            return 1;
    return 0;
}

/* Used as return type for get_command and index into cmds */
enum command { REM, CATALOG, CDTEXTFILE,
               FILECMD, PERFORMER, SONGWRITER, TITLE, TRACK, FLAGS, DCP,
               FOURCH, PRE, SCMS, ISRC, PREGAP, INDEX, POSTGAP, BINARY,
               MOTOROLA, AIFF, WAVE, MP3, OGG, UNKNOWN, END };

/* Except the last two these are the valid CUE commands */
char cmds[][MAXCMDLEN + 1] = { "REM", "CATALOG", "CDTEXTFILE",
                               "FILE", "PERFORMER", "SONGWRITER", "TITLE", "TRACK", "FLAGS", "DCP",
                               "4CH", "PRE", "SCMS", "ISRC", "PREGAP", "INDEX", "POSTGAP", "BINARY",
                               "MOTOROLA", "AIFF", "WAVE", "MP3", "OGG", "UNKNOWN", "END" };

/* These are for error messages */
static const char *fname = "stdin";
static long line;		/* current line number */
static long tokenstart;		/* line where last token started */

/* To generate meaningful error messages in check_once */
enum scope { CUESHEET, GLOBAL, ONETRACK };

/* Fatal error while processing input file */
static void
err_fail(const char *s)
{
    fprintf(stderr, "%s:%ld: %s\n", fname, tokenstart, s);
    exit(EXIT_FAILURE);
}

/* Fatal error */
static void
err_fail2(const char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(EXIT_FAILURE);
}

/* EOF while expecting more */
static void
err_earlyend()
{
    fprintf(stderr, "%s:%ld: Premature end of file\n", fname, line);
    exit(EXIT_FAILURE);
}

/* Warning. Keep on going. */
static void
err_warn(const char *s)
{
    fprintf(stderr, "%s:%ld: Warning, %s\n", fname, tokenstart, s);
}

/* Get next command from file */
static enum command
get_command(FILE *f)
{
    int c;
    char buf[MAXCMDLEN + 1];
    int i = 0;

    /* eat whitespace */
    do {
        c = getc(f);
        if (c == '\n')
            line++;
    } while (isdelim(c));

    if (c == EOF)
        return END;
	
    tokenstart = line;

    /* get command, transform to upper case */
    do {
        buf[i++] = toupper(c);
        c = getc(f);
    } while (!isdelim(c) && c!= EOF && i < MAXCMDLEN);

    if (!isdelim(c)) return UNKNOWN; /* command longer than MAXCMDLEN */
    if (c == EOF) return END;
    if (c == '\n') line++;

    buf[i] = '\0';

    if (strcmp(buf, cmds[REM]) == 0) return REM;
    else if (strcmp(buf, cmds[CATALOG]) == 0) return CATALOG;
    else if (strcmp(buf, cmds[CDTEXTFILE]) == 0) return CDTEXTFILE;
    else if (strcmp(buf, cmds[FILECMD]) == 0) return FILECMD;
    else if (strcmp(buf, cmds[PERFORMER]) == 0) return PERFORMER;
    else if (strcmp(buf, cmds[SONGWRITER]) == 0) return SONGWRITER;
    else if (strcmp(buf, cmds[TITLE]) == 0) return TITLE;
    else if (strcmp(buf, cmds[TRACK]) == 0) return TRACK;
    else if (strcmp(buf, cmds[FLAGS]) == 0) return FLAGS;
    else if (strcmp(buf, cmds[DCP]) == 0) return DCP;
    else if (strcmp(buf, cmds[FOURCH]) == 0) return FOURCH;
    else if (strcmp(buf, cmds[PRE]) == 0) return PRE;
    else if (strcmp(buf, cmds[SCMS]) == 0) return SCMS;
    else if (strcmp(buf, cmds[ISRC]) == 0) return ISRC;
    else if (strcmp(buf, cmds[PREGAP]) == 0) return PREGAP;
    else if (strcmp(buf, cmds[INDEX]) == 0) return INDEX;
    else if (strcmp(buf, cmds[POSTGAP]) == 0) return POSTGAP;
    else if (strcmp(buf, cmds[BINARY]) == 0) return BINARY;
    else if (strcmp(buf, cmds[MOTOROLA]) == 0) return MOTOROLA;
    else if (strcmp(buf, cmds[AIFF]) == 0) return AIFF;
    else if (strcmp(buf, cmds[WAVE]) == 0) return WAVE;
    else if (strcmp(buf, cmds[MP3]) == 0) return MP3;
    else if (strcmp(buf, cmds[OGG]) == 0) return OGG;
    else return UNKNOWN;
}

/* Skip leading token delimiters then read at most n chars from f into s.
 * Put terminating Null at the end of s. This implies that s must be
 * really n + 1. Return number of characters written to s. The only case to
 * return zero is on EOF before any character was read.
 * Exit the program indicating failure if string is longer than n. */
static size_t
get_string(FILE *f, char *s, size_t n)
{
    int c;
    size_t i = 0;

    /* eat whitespace */
    do {
        c = getc(f);
        if (c == '\n')
            line++;
    } while (isdelim(c));

    if (c == EOF)
        return 0;

    tokenstart = line;

    if (c == '\"') {
        c = getc(f);
        if (c == '\n') line++;
        while (c != '\"' && c != EOF && i < n) {
            s[i++] = c;
            c = getc(f);
            if (c == '\n') line++;
        }
        if (i == n && c != '\"' && c != EOF)
            err_fail("String too long");
    } else {
        while (!isdelim(c) && c != EOF && i < n) {
            s[i++] = c;
            c = getc(f);
        }
        if (i == n && !isdelim(c) && c != EOF)
            err_fail("String too long");
    }
    if (i == 0) err_fail("Empty string");
    if (c == '\n') line++;
    s[i] = '\0';

    return i;
}

/* Return track mode */
static TrackData::Mode get_track_mode(FILE *f)
{
  char buf[] = "MODE1/2048";
  char *pbuf = buf;

  if (get_string(f, buf, sizeof(buf) - 1) < 1)
    err_fail("Illegal track mode");

  /* transform to upper case */
  while (*pbuf) {
    *pbuf = toupper(*pbuf);
    pbuf++;
  }

  if (strcmp(buf, "AUDIO") == 0) return TrackData::AUDIO;
  if (strcmp(buf, "MODE1/2048") == 0) return TrackData::MODE1;
  if (strcmp(buf, "MODE1/2352") == 0) return TrackData::MODE1_RAW;
  if (strcmp(buf, "MODE2/2336") == 0) return TrackData::MODE2;
  if (strcmp(buf, "MODE2/2352") == 0) return TrackData::MODE2_RAW;

  err_fail("Unsupported track mode");
  return TrackData::AUDIO;
}

static void check_once(enum command cmd, char *s, enum scope sc);

/* Read at most CDTEXTLEN chars into s */
static void
get_cdtext(FILE *f, enum command cmd, char *s, enum scope sc)
{
    check_once(cmd, s, sc);
    if (get_string(f, s, CDTEXTLEN) < 1)
        err_earlyend();
}

/* All strings have their first character initialized to '\0' so if s[0]
   is not Null the cmd has already been seen in input. In this case print
   a message end exit program indicating failure. The only purpose of the
   arguments cmd and sc is to print meaningful error messages. */
static void
check_once(enum command cmd, char *s, enum scope sc)
{
    if (s[0] == '\0')
        return;
    fprintf(stderr, "%s:%ld: %s allowed only once", fname, line, cmds[cmd]);
    switch (sc) {
    case CUESHEET:	fprintf(stderr, "\n"); break;
    case GLOBAL:	fprintf(stderr, " in global section\n"); break;
    case ONETRACK:	fprintf(stderr, " per track\n"); break;
    }
    exit(EXIT_FAILURE);
}

/* Allocate, initialize and return new track */
static struct trackspec*
new_track(void)
{
    struct trackspec *track;
    int i;

    if ((track = (struct trackspec*) malloc(sizeof(struct trackspec)))
        == NULL)
        err_fail("Memory allocation error in new_track()");

    track->copy = track->pre_emphasis = track->four_channel_audio 
        = track->pregap_data_from_file = 0;
    track->isrc[0] = track->title[0] = track->performer[0]
        = track->songwriter[0] = track->filename[0] = '\0';
    track->pregap = track->start = track->postgap = -1;

    for (i = 0; i < NUM_OF_INDEXES; i++)
        track->indexes[i] = -1;
    track->next = NULL;

    return track;
}

/* Read the cuefile and return a pointer to the cuesheet */
struct cuesheet*
read_cue(const char *cuefile, const char *wavefile)
{
    FILE *f;
    enum command cmd;
    struct cuesheet *cs = NULL;
    struct trackspec *track = NULL;
    size_t n;
    int c;
    char file[FILENAMELEN + 1];
    char timecode_buffer[TCBUFLEN];
    char devnull[FILENAMELEN + 1];	/* just for eating CDTEXTFILE arg */

    if (NULL == cuefile) {
        f = stdin;
    } else if (NULL == (f = fopen(cuefile, "r"))) {
        fprintf(stderr, "Could not open file \"%s\" for "
                "reading: %s\n", cuefile, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (cuefile)
        fname = cuefile;

    if ((cs = (struct cuesheet*) malloc(sizeof(struct cuesheet))) == NULL)
        err_fail("Memory allocation error in read_cue()");

    cs->catalog[0] = '\0';
    cs->type = (session_type)0;
    cs->title[0] = '\0';
    cs->performer[0] = '\0';
    cs->songwriter[0] = '\0';
    cs->tracklist = NULL;

    file[0] = '\0';
    line = 1;

    /* global section */
    while ((cmd = get_command(f)) != TRACK) {
        switch (cmd) {
        case UNKNOWN:
            err_fail("Unknown command");
        case END:
            err_earlyend();
        case REM:
            c = getc(f);
            while (c != '\n' && c != EOF)
                c = getc(f);
            break;
        case CDTEXTFILE:
            err_warn("ignoring CDTEXTFILE...");
            if (get_string(f, devnull, FILENAMELEN) == 0)
                err_warn("Syntactically incorrect "
                         "CDTEXTFILE command. But who "
                         "cares...");
            break;
        case CATALOG:
            check_once(CATALOG, cs->catalog, CUESHEET);
            n = get_string(f, cs->catalog, 13);
            if (n != 13)
                err_fail("Catalog number must be 13 "
                         "characters long");
            break;
        case TITLE:
            get_cdtext(f, TITLE, cs->title, GLOBAL);
            break;
        case PERFORMER:
            get_cdtext(f, PERFORMER, cs->performer, GLOBAL);
            break;
        case SONGWRITER:
            get_cdtext(f, SONGWRITER, cs->songwriter, GLOBAL);
            break;
        case FILECMD:
            check_once(FILECMD, file, GLOBAL);
            if (get_string(f, file, FILENAMELEN) < 1)
                err_earlyend();

            switch (cmd = get_command(f)) {
            case MOTOROLA:
                err_warn("big endian binary file");
            case BINARY:
                break;
            case AIFF:
                err_warn("AIFF not supported by cdrdao");
            case MP3:
            case OGG:
            case WAVE:
                if (wavefile) {
                    strncpy(file, wavefile, FILENAMELEN);
                    file[FILENAMELEN] = '\0';
                }
                break;
            default:
                err_fail("Unsupported file type");
            }
            break;
        default:
            err_fail("Command not allowed in global section");
            break;
        }

    }

    /* leaving global section, entering track specifications */
    if (file[0] == '\0')
        err_fail("TRACK without previous FILE");

    while (cmd != END) {
        switch(cmd) {
        case UNKNOWN:
            err_fail("Unknown command");
        case REM:
            c = getc(f);
            while (c != '\n' && c != EOF)
                c = getc(f);
            break;
        case TRACK:
            if (track == NULL)	/* first track */
                cs->tracklist = track = new_track();
            else {
                track = track->next = new_track();
            }

            /* the CUE format is "TRACK nn MODE" but we are not
               interested in the track number */
            while (isdelim(c = getc(f)))
                if (c == '\n') line++;
            while (!isdelim(c = getc(f))) ;
            if (c == '\n') line++;

            track->mode = get_track_mode(f);

            /* audio tracks with binary files seem quite common */
            /*
              if (track->mode == AUDIO && filetype == BINARY
              || track->mode != AUDIO && filetype == WAVE)
              err_fail("File and track type mismatch");
            */

            strcpy(track->filename, file);
            break;
        case TITLE:
            get_cdtext(f, TITLE, track->title, ONETRACK);
            break;
        case PERFORMER:
            get_cdtext(f, PERFORMER, track->performer, ONETRACK);
            break;
        case SONGWRITER:
            get_cdtext(f, SONGWRITER, track->songwriter, ONETRACK);
            break;
        case ISRC:
            check_once(ISRC, track->isrc, ONETRACK);
            if (get_string(f, track->isrc, 12) != 12)
                err_fail("ISRC must be 12 characters long");
            break;
        case FLAGS:
            if (track->copy || track->pre_emphasis 
                || track->four_channel_audio)
                err_fail("FLAGS allowed only once per track");

            /* get the flags */
            cmd = get_command(f);
            while (cmd == DCP || cmd == FOURCH || cmd == PRE
                   || cmd == SCMS) {
                switch (cmd) {
                case DCP:
                    track->copy = 1; break;
                case FOURCH:
                    track->four_channel_audio = 1; break;
                case PRE:
                    track->pre_emphasis = 1; break;
                case SCMS:
                    err_warn("serial copy management "
                             "system flag not supported "
                             "by cdrdao"); break;
                default:
                    err_fail("Should not get here");
                }
                cmd = get_command(f);
            }
            /* current non-FLAG command is already in cmd, so
               avoid get_command() call below */
            continue; break;
        case PREGAP:
            if (track->pregap != -1)
                err_fail("PREGAP allowed only once per track");
            if (get_string(f, timecode_buffer, TCBUFLEN - 1) < 1)
                err_earlyend();
            track->pregap = tc2fr(timecode_buffer);
            if (track->pregap == -1)
                err_fail("Timecode out of range");
            track->pregap_data_from_file = 0;
            break;
        case POSTGAP:
            if (track->postgap != -1)
                err_fail("POSTGAP allowed only once per track");
            if (get_string(f, timecode_buffer, TCBUFLEN - 1) < 1)
                err_earlyend();
            track->postgap = tc2fr(timecode_buffer);
            if (track->postgap == -1)
                err_fail("Timecode out of range");
            break;
        case INDEX:
            if (get_string(f, timecode_buffer, 2) < 1)
                err_earlyend();
            n = atoi(timecode_buffer);
            if (n < 0 || n > 99)
                err_fail("Index out of range");

            /* Index 0 is track pregap and Index 1 is start
               of track. Index 2 to 99 are the true subindexes
               and only allowed if the preceding one was there
               before */
            switch (n) {
            case 0:
                if (track->start != -1)
                    err_fail("Indexes must be sequential");
                if (track->pregap != -1)
                    err_fail("PREGAP allowed only once "
                             "per track");
                if (get_string(f, timecode_buffer,
                               TCBUFLEN - 1) < 1)
                    err_earlyend();
                /* This is only a temporary value until
                   index 01 is read */
                track->pregap = tc2fr(timecode_buffer);
                if (track->pregap == -1)
                    err_fail("Timecode out of range");
                track->pregap_data_from_file = 1;
                break;
            case 1:
                if (track->start != -1)
                    err_fail("Each index allowed only "
                             "once per track");
                if (get_string(f, timecode_buffer,
                               TCBUFLEN - 1) < 1)
                    err_fail("Missing timecode");
                track->start = tc2fr(timecode_buffer);
                if (track->start == -1)
                    err_fail("Timecode out of range");
                /* Fix the pregap value */
                if (track->pregap_data_from_file)
                    track->pregap = track->start
                        - track->pregap;
                break;
            case 2:
                if (track->start == -1)
                    err_fail("Indexes must be sequential");
                if (track->indexes[n - 2] != -1)
                    err_fail("Each index allowed only "
                             "once per track");
                if (get_string(f, timecode_buffer,
                               TCBUFLEN - 1) < 1)
                    err_fail("Missing timecode");
                track->indexes[n - 2] = tc2fr(timecode_buffer);
                if (track->indexes[n - 2] == -1)
                    err_fail("Timecode out of range");
                break;
            default:	/* the other 97 indexes */
                /* check if previous index is there */
                if (track->indexes[n - 3] == -1)
                    err_fail("Indexes must be sequential");
                if (track->indexes[n - 2] != -1)
                    err_fail("Each index allowed only "
                             "once per track");
                if (get_string(f, timecode_buffer,
                               TCBUFLEN - 1) < 1)
                    err_fail("Missing timecode");
                track->indexes[n - 2] = tc2fr(timecode_buffer);
                if (track->indexes[n - 2] == -1)
                    err_fail("Timecode out of range");
                break;
            }
            break;
        case FILECMD:
            if (get_string(f, file, FILENAMELEN) < 1)
                err_earlyend();

            switch (cmd = get_command(f)) {
            case MOTOROLA:
                err_warn("big endian binary file");
            case BINARY:
                break;
            case AIFF:
                err_warn("AIFF and MP3 not supported by "
                         "cdrdao");
            case MP3:
            case OGG:
            case WAVE:
                if (wavefile) {
                    strncpy(file, wavefile, FILENAMELEN);
                    file[FILENAMELEN] = '\0';
                }
                break;
            default:
                err_fail("Unsupported file type");
            }
            break;
        default:
            err_fail("Command not allowed in track spec");
            break;
        }
		
        cmd = get_command(f);
    }

    return cs;
}

/* Deduce the disc session type from the track modes */
static enum session_type
determine_session_type(struct trackspec *list)
{
    struct trackspec *track = list;
    /* set to true if track of corresponding type is found */
    int audio = 0;
    int mode1 = 0;
    int mode2 = 0;

    while (track != NULL) {
        switch (track->mode) {
        case TrackData::AUDIO:
            audio = 1; break;
        case TrackData::MODE1: case TrackData::MODE1_RAW:
            mode1 = 1; break;
        case TrackData::MODE2: case TrackData::MODE2_RAW:
            mode2 = 1; break;
        default:	/* should never get here */
            err_fail2("Dont know how this could happen, but here "
                      "is a track with an unknown mode :|");
        }
        track = track->next;
    }

    /* CD_DA	only audio
     * CD_ROM	only mode1 with or without audio
     * CD_ROM_XA	only mode2 with or without audio
     */
    if (audio && !mode1 && !mode2)
        return CD_DA;
    else if ((audio && mode1 && !mode2) || (!audio && mode1 && !mode2))
        return CD_ROM;
    else if ((audio && !mode1 && mode2) || (!audio && !mode1 && mode2))
        return CD_ROM_XA;
    else
        return INVALID;
}

/* Return true if cuesheet contains any CD-Text data */
static int
contains_cdtext(struct cuesheet *cs)
{
    struct trackspec *track = cs->tracklist;

    if (cs->title[0] != '\0' || cs->performer[0] != '\0'
        || cs->songwriter[0] != '\0')
        return 1;

    while (track) {
        if (track->title[0] != '\0' || track->performer[0] != '\0'
            || track->songwriter[0] != '\0')
            return 1;
        track = track->next;
    }

    return 0;
}

/* fprintf() with indentation. The argument indent is the number of spaces
   to print per level. E.g. with indent=4 and level=3 there are 12 spaces
   printed. Every eight spaces are replaced by a single tabulator. The
   return value is the return value of fprintf(). */

static int ifprintf(std::ostream& o, int indent, int level,
                    const char *format, ...)
{
    static char twolines[161];

    va_list ap;
    int fprintf_return = 0;
    int tabs = indent * level / 8;
    int spaces = indent * level % 8;
    int i;

    for (i = 0; i < tabs; i++)
        o << '\t';
    for (i = 0; i < spaces; i++)
        o << ' ';

    va_start(ap, format);
    fprintf_return = vsprintf(twolines, format, ap);
    va_end(ap);
    o << twolines;

    return fprintf_return;
}

/* Write a track to the file f. The arguments i and l are the indentation
   amount and level (see ifprintf above). Do not write CD-Text data if
   cdtext is zero. */
static void
write_track(struct trackspec *tr, std::ostream& f, int i, int l, int cdtext)
{
    char timecode_buffer[TCBUFLEN];
    long start = 0, len = 0;
    int j = 0;

    f << std::endl;
    ifprintf(f, i, l++, "TRACK ");
    switch(tr->mode) {
    case TrackData::AUDIO:	f << "AUDIO\n"; break;
    case TrackData::MODE1:	f << "MODE1\n"; break;
    case TrackData::MODE1_RAW:	f << "MODE1_RAW\n"; break;
    case TrackData::MODE2:	f << "MODE2\n"; break;
    case TrackData::MODE2_RAW:	f << "MODE2_RAW\n"; break;
    default:	err_fail2("Unknown track mode"); /* cant get here */
    }

    /* Flags and ISRC */
    if (tr->copy)
        ifprintf(f, i, l, "COPY\n");
    if (tr->pre_emphasis)
        ifprintf(f, i, l, "PRE_EMPHASIS\n");
    if (tr->four_channel_audio)
        ifprintf(f, i, l, "FOUR_CHANNEL_AUDIO\n");
    if (tr->isrc[0] != '\0')
        ifprintf(f, i, l, "ISRC \"%s\"\n", tr->isrc);

    /* CD-Text data */
    if (cdtext && (tr->title[0] != '\0' || tr->performer[0] != '\0'
                   || tr->songwriter[0] != '\0')) {
        ifprintf(f, i, l++, "CD_TEXT {\n");
        ifprintf(f, i, l++, "LANGUAGE 0 {\n");
        if (tr->title[0] != '\0')
            ifprintf(f, i, l, "TITLE \"%s\"\n", tr->title);
        if (tr->performer[0] != '\0')
            ifprintf(f, i, l, "PERFORMER \"%s\"\n", tr->performer);
        if (tr->songwriter[0] != '\0')
            ifprintf(f, i, l, "SONGWRITER \"%s\"\n",
                     tr->songwriter);
        ifprintf(f, i, --l, "}\n");	/* LANGUAGE 0 { */
        ifprintf(f, i, --l, "}\n");	/* CD_TEXT { */
    }

    /* Pregap with zero data */
    if (tr->pregap != -1 && !tr->pregap_data_from_file) {
        if (fr2tc(timecode_buffer, tr->pregap) == -1)
            err_fail2("Pregap out of range");
        ifprintf(f, i, l, "PREGAP %s\n", timecode_buffer);
    }

    /* Specify the file */
    start = 0;
    if (tr->mode == TrackData::AUDIO) {
        ifprintf(f, i, l, "AUDIOFILE \"%s\" ", tr->filename);
        if (tr->start != -1) {
            if (tr->pregap_data_from_file) {
                start = tr->start - tr->pregap;
            } else
                start = tr->start;
        }
        if (fr2tc(timecode_buffer, start) == -1)
            err_fail2("Track start out of range");
        f << timecode_buffer;
    } else {
      if (tr->start) {
        long datastart = (tr->pregap_data_from_file ?
                          tr->start - tr->pregap : tr->start);
        ifprintf(f, i, l, "DATAFILE \"%s\" #%d", tr->filename,
                 datastart * AUDIO_BLOCK_LEN);
        start = datastart;
      } else {
        ifprintf(f, i, l, "DATAFILE \"%s\"", tr->filename);
      }
    }

    /* If next track has the same filename and specified a start
       value use the difference between start of this and start of
       the next track as the length of the current track */
    if (tr->next
        && strcmp(tr->filename, tr->next->filename) == 0
        && tr->next->start != -1) {
      if (tr->next->pregap_data_from_file)
        len = tr->next->start - tr->next->pregap - start;
      else
        len = tr->next->start - start;
      if (fr2tc(timecode_buffer, len) == -1)
        err_fail2("Track length out of range");
      f << ' ' << timecode_buffer << std::endl;
    } else {
      f << std::endl;
    }

    /* Pregap with data from file */
    if (tr->pregap_data_from_file) {
        if (fr2tc(timecode_buffer, tr->pregap) == -1)
            err_fail2("Pregap out of range");
        ifprintf(f, i, l, "START %s\n", timecode_buffer);
    }

    /* Postgap */
    if (tr->postgap != -1) {
        if (fr2tc(timecode_buffer, tr->postgap) == -1)
            err_fail2("Postgap out of range");
        if (tr->mode == TrackData::AUDIO)
            ifprintf(f, i, l, "SILENCE %s\n", timecode_buffer);
        else
            ifprintf(f, i, l, "ZERO %s\n", timecode_buffer);
    }

    /* Indexes */
    while (tr->indexes[j] != -1 && i < NUM_OF_INDEXES) {
        if (fr2tc(timecode_buffer, tr->indexes[j++]) == -1)
            err_fail2("Index out of range");
        ifprintf(f, i, l, "INDEX %s\n", timecode_buffer);
    }

}

// Write the cuesheet cs to the output stream. Do not write CD-Text
// data if cdt is zero.

void write_toc(std::ostream& f, struct cuesheet *cs, bool cdt)
{
    int i = 4;		/* number of chars for indentation */
    int l = 0;		/* current leven of indentation */
    bool cdtext = contains_cdtext(cs) && cdt;
    struct trackspec *track = cs->tracklist;

    if ((cs->type = determine_session_type(cs->tracklist)) == INVALID)
        err_fail2("Invalid combination of track modes");

    ifprintf(f, i, l, "// Generated by cue2toc 0.2\n");
    ifprintf(f, i, l, "// Report bugs to <dermatsch@gmx.de>\n");

    if (cs->catalog[0] != '\0')
        ifprintf(f, i, l, "CATALOG \"%s\"\n", cs->catalog);

    switch (cs->type) {
    case CD_DA:	ifprintf(f, i, l, "CD_DA\n"); break;
    case CD_ROM:	ifprintf(f, i, l, "CD_ROM\n"); break;
    case CD_ROM_XA:	ifprintf(f, i, l, "CD_ROM_XA\n"); break;
    default:	err_fail2("Should never get here");
    }

    if (cdtext) {
        ifprintf(f, i, l++, "CD_TEXT {\n");
        ifprintf(f, i, l++, "LANGUAGE_MAP {\n");
        ifprintf(f, i, l, "0 : EN\n");
        ifprintf(f, i, --l, "}\n");
        ifprintf(f, i, l++, "LANGUAGE 0 {\n");
        if (cs->title[0] != '\0')
            ifprintf(f, i, l, "TITLE \"%s\"\n", cs->title);
        if (cs->performer[0] != '\0')
            ifprintf(f, i, l, "PERFORMER \"%s\"\n", cs->performer);
        if (cs->songwriter[0] != '\0')
            ifprintf(f, i, l, "SONGWRITER \"%s\"\n",
                     cs->songwriter);
        ifprintf(f, i, --l, "}\n");
        ifprintf(f, i, --l, "}\n");
    }

    while (track) {
        write_track(track, f, i, l, cdtext);
        track = track->next;
    }
}

/* Interpret argument as timecode value ("MM:SS:FF") and return the total
   number of frames. Tries to work in a way similar to atoi(), ignoring any
   trailing non-timecode junk. Skips leading whitespace.
   I want it to be as flexible as possible, recognizing simple values like
   "0" (interpreted as "00:00:00"), "1:2" ("00:01:02") and so on.
   Returns -1 on error (argument NULL or some value out of range) */

#define MAXDIGITS	2
#define NUMOFNUMS	3

long
tc2fr(const char *tc)
{
	int minutes = 0;
	int seconds = 0;
	int frames = 0;
	long totalframes = 0;

	char tmp[MAXDIGITS + 1];
	int nums[NUMOFNUMS];
	int n = 0;
	int i = 0;
	int last_was_colon = 0;
	int stop = 0;

	if (tc == NULL)
		return -1;

	for (i = 0; i <= MAXDIGITS; i++)
		tmp[i] = '\0';

	while (isspace(*tc))
		tc++;

	for (n = 0; n < NUMOFNUMS; n++) {
		if (n > 0) {
			if (tc[0] != ':') {
				--n;
				break;
			} else
				tc++;
		}

		for (i = 0; i < MAXDIGITS; i++) {

			if (isdigit(tc[i])) {
				tmp[i] = tc[i];
				last_was_colon = 0;
			} else if (tc[i] == ':') {
				if (i == 0)
					stop = 1;
				break;
			} else { 
				stop = 1;
				break;
			}
		}
		
		if (i != 0) {
			tmp[i] = '\0';
			nums[n] = atoi(tmp);
			tc = &tc[i];
		} else
			--n;

		if (stop)
			break;
	}

	if (n == NUMOFNUMS)
		--n;
	
	frames = seconds = minutes = 0;

	switch (n) {
	case 0:
		frames = nums[0];
		break;
	case 1:
		seconds = nums[0];
		frames = nums[1];
		break;
	case 2:
		minutes = nums[0];
		seconds = nums[1];
		frames = nums[2];
		break;
	}

	totalframes = ((60 * minutes) + seconds) * 75 + frames;

	if (seconds > 59 || frames > 74)
		return -1;

	return totalframes;
}


/* Writes formatted timecode string ("MM:SS:FF") into tc, calculated from
   frame number fr.
   Returns -1 on error (frames value out of range) */

int
fr2tc(char *tc, long fr)
{
	int m;
	int s;
	int f;

	if (fr > 449999 || fr < 0) {	/* 99:59:74 */
		strcpy(tc, "00:00:00");
		return -1;
	}

	f = fr % 75;
	fr -= f;
	s = (fr / 75) % 60;
	fr -= s * 75;
	m = fr / 75 / 60;

	sprintf(tc, "%02d:%02d:%02d", m, s, f);
	return 0;
}
