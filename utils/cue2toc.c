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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "cue2toc.h"
#include "timecode.h"

extern const char *progname;
extern int verbose;

/* Adds an item to end of the linked list of struct track structures. */

struct track *
additem(struct track *list)
{
	struct track *item;

	if ((item = (struct track*)malloc(sizeof(struct track))) == NULL) {
		fprintf(stderr, "%s: could not add item to list of tracks: "
				"memory allocation error\n", progname);
		exit(EXIT_FAILURE);
	}

	if (list != NULL) {
		while (list->next != NULL)
			list = list->next;
		list->next = item;
	}

	item->next = NULL;

	return item;
}


/* Read a _hole_ line (no matter how long), allocating space as necessary.
   Remember to free the returned pointer. */

char *
readline(FILE *f)
{
	char *line = NULL;
	int chunksize = 256;
	int numchunks = 0;
	long i = 0;
	int c;

	while ((c = getc(f)) != EOF) {
		if (i >= numchunks * chunksize - 2)
			if ((line = (char*) realloc(line, ++numchunks *
			chunksize * sizeof(char))) == NULL) {
				fprintf(stderr, "%s: could not read line: "
					"memory allocation error\n", progname);
				exit(EXIT_FAILURE);
			}
		
		if (c == '\n') {
			line[i] = '\0';
			break;
		}
		line[i++] = c;
	}

	return line;
}


/* Copys the string from src to dest, removing quotes and skipping leading
   whitespace. At most TEXTSIZE - 1 characters are copied. */

void
textcpy(char *dest, const char *src)
{
	int quot = 0;
	int i = 0;

	while (*src == ' ' || *src == '\t')
		src++;
	if (*src == '"') {
		src++;
		quot = 1;
	}

	while (*src != '\n' && *src != '\0' && ++i < TEXTSIZE)
		if (quot && *src == '"')
			break;
		else
			*dest++ = *src++;
	*dest = '\0';
}


/* Reads in track information from file and stores it in a linked list of
   struct track structures. Writes .toc file to stdout. */

int
cue2toc(FILE *in, FILE *out, char *wavefile, int nocdtext)
{
	struct track *list;	/* always points to first element of list */
	struct track *cur;	/* current element in list */
	char *line;		/* current line of input file */
	char *pos;		/* current character in line */

	char cmd_file[] =	"FILE";
	char cmd_index[] =	"INDEX 01";
	char cmd_track[] =	"TRACK";
	char cmd_title[] =	"TITLE";
	char cmd_perf[] =	"PERFORMER";
	char cmd_writ[] = 	"SONGWRITER";
	char cmd_comp[] =	"COMPOSER";
	char cmd_arra[] =	"ARRANGER";
	char cmd_mess[] =	"MESSAGE";

	char disc_title[TEXTSIZE] = "unknown";
	char disc_performer[TEXTSIZE] = "unknown";
	char disc_file[TEXTSIZE] = "filename";

	int first_track = 1;	/* set to zero after first track printed */
	int hascdtext = 0;	/* set to one if source contains CD-Text */
	char track_index[9];	/* buffers for timecode strings */
	char track_length[10];

	if (wavefile != NULL)
		textcpy(disc_file, wavefile);

	/* get the disc information */
	while ((line = readline(in)) != NULL) {
		pos = line;

		while (isspace(*pos) && *pos != '\0')
			pos++;
		if (*pos == '\0') {	/* empty line */
			free(line);
			continue;
		}

		if (strncmp(pos, cmd_track, strlen(cmd_track)) == 0) {
			free(line);
			break;	/* track definitions start here */
		} else if (strncmp(pos, cmd_title, strlen(cmd_title)) == 0) {
			hascdtext = 1;
			textcpy(disc_title, pos + strlen(cmd_title));
		} else if (strncmp(pos, cmd_perf, strlen(cmd_perf)) == 0) {
			hascdtext = 1;
			textcpy(disc_performer, pos + strlen(cmd_perf));
		} else if (strncmp(pos, cmd_file, strlen(cmd_file)) == 0) {
			if (wavefile == NULL)
				textcpy(disc_file, pos + strlen(cmd_file));
		} else
			if (verbose)
				fprintf(stderr, "%s: unsupported command in "
					"header: %s\n", progname, pos);
		free(line);
	}

	if (line == NULL)
		return -1;
	
	list = additem(NULL);	/* initialize list */
	cur = list;
	strcpy(cur->title, "unknown");
	strcpy(cur->performer, "unknown");
	strcpy(cur->songwriter, "unknown");
	strcpy(cur->composer, "unknown");
	strcpy(cur->arranger, "unknown");
	strcpy(cur->message, "unknown");

	/* get the tracks */
	while ((line = readline(in)) != NULL) {
		pos = line;

		while (isspace(*pos) && *pos != '\0')
			pos++;
		if (*pos == '\0')
			continue;

		if (strncmp(pos, cmd_title, strlen(cmd_title)) == 0) {
			hascdtext = 1;
			textcpy(cur->title, pos + strlen(cmd_title));
		} else if (strncmp(pos, cmd_perf, strlen(cmd_perf)) == 0) {
			hascdtext = 1;
			textcpy(cur->performer, pos + strlen(cmd_perf));
		} else if (strncmp(pos, cmd_writ, strlen(cmd_writ)) == 0) {
			hascdtext = 1;
			textcpy(cur->songwriter, pos + strlen(cmd_writ));
		} else if (strncmp(pos, cmd_comp, strlen(cmd_comp)) == 0) {
			hascdtext = 1;
			textcpy(cur->composer, pos + strlen(cmd_comp));
		} else if (strncmp(pos, cmd_arra, strlen(cmd_arra)) == 0) {
			hascdtext = 1;
			textcpy(cur->arranger, pos + strlen(cmd_arra));
		} else if (strncmp(pos, cmd_mess, strlen(cmd_mess)) == 0) {
			hascdtext = 1;
			textcpy(cur->message, pos + strlen(cmd_mess));
		} else if (strncmp(pos, cmd_index, strlen(cmd_index)) == 0) {
			if ((cur->index = tc2fr(pos + strlen(cmd_index))) < 0
			    && verbose)
				fprintf(stderr, "%s: bogus timecode "
					"value: %s\n", progname,
					pos + strlen(cmd_index));
		} else if (strncmp(pos, cmd_track, strlen(cmd_track)) == 0) {
			cur = additem(list);
			strcpy(cur->title, "unknown");
			strcpy(cur->performer, "unknown");
			strcpy(cur->songwriter, "unknown");
			strcpy(cur->composer, "unknown");
			strcpy(cur->arranger, "unknown");
			strcpy(cur->message, "unknown");
		} else
			if (verbose)
				fprintf(stderr, "%s: unsupported command in "
					"track: %s\n", progname, pos);

		free(line);
	}

	/* dump disc info */
	fprintf(out, "CD_DA\n");
	if (hascdtext && !nocdtext) {
		fprintf(out, "CD_TEXT {\n");
		fprintf(out, "    LANGUAGE_MAP {\n");
		fprintf(out, "\t0 : EN\n");
		fprintf(out, "    }\n");
		fprintf(out, "    LANGUAGE 0 {\n");
		fprintf(out, "\tTITLE \"%s\"\n", disc_title);
		fprintf(out, "\tPERFORMER \"%s\"\n", disc_performer);
		fprintf(out, "    }\n}\n\n");
	}

	cur = list;

	/* dump track info */
	while (cur != NULL) {
		fprintf(out, "TRACK AUDIO\n");
		if (hascdtext && !nocdtext) {
			fprintf(out, "CD_TEXT {\n");
			fprintf(out, "    LANGUAGE 0 {\n");
			fprintf(out, "\tTITLE \"%s\"\n", cur->title);
			fprintf(out, "\tPERFORMER \"%s\"\n", cur->performer);

/* Havent seen any .cue with these */
/*
			fprintf(out, "\tSONGWRITER \"%s\"\n", cur->songwriter);
			fprintf(out, "\tCOMPOSER \"%s\"\n", cur->composer);
			fprintf(out, "\tARRANGER \"%s\"\n", cur->arranger);
			fprintf(out, "\tMESSAGE \"%s\"\n", cur->message);
*/
			fprintf(out, "    }\n}\n");
		}

		if (first_track) {
			fprintf(out, "PREGAP 0:2:0\n");
			first_track = 0;
		}

		if (fr2tc(track_index, cur->index) < 0 && verbose)
			fprintf(stderr, "%s: conversion error for %ld\n",
				progname, cur->index);

		if (cur->next != NULL) {
			track_length[0] = ' ';
			if (fr2tc(track_length + 1,
				  cur->next->index - cur->index) < 0 && verbose)
				fprintf(stderr, "%s: conversion error "
					"for %ld\n", progname,
					cur->next->index - cur->index);
		} else
			track_length[0] = '\0';

		fprintf(out, "FILE \"%s\" %s%s\n", disc_file, track_index,
			track_length);
		if (hascdtext && !nocdtext)
			fprintf(out, "\n"); /* better readable if CD-Text */

		list = cur;
		cur = cur->next;
		free(list);
	}
	return 0;
}
