/* main.c - handle command line arguments
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "cue2toc.h"

const char *progname = NULL;
int verbose;
void usage(char *progname);

int
main(int argc, char *argv[])
{
	FILE *infile = stdin;
	FILE *outfile = stdout;
	char *wavefile = NULL;
	char *outfile_name = NULL;
	int nocdtext = 0;
	int c;

	progname = argv[0];
	verbose = 0;
	opterr = 0;	/* we do error msgs ourselves */
	while ((c = getopt(argc, argv, ":hno:vw:")) != -1)
		switch (c) {
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		case 'n':
			nocdtext = 1;
			break;
		case 'o':
			outfile_name = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			wavefile = optarg;
			break;
		case ':':
			fprintf(stderr, "%s: option requires an argument -- "
					"%c\n", argv[0], optopt);
			exit(EXIT_FAILURE);
		case '?':
			fprintf(stderr, "%s: illegal option -- %c\n",
				argv[0], optopt);
			exit(EXIT_FAILURE);
		}

	switch(argc - optind) {
	case 0:
		infile = stdin;
		break;
	case 1:
		if (strcmp(argv[optind], "-") == 0)
			infile = stdin;
		else
			if ((infile = fopen(argv[optind], "r")) == NULL) {
				fprintf(stderr, "%s: could not open file "
					"\"%s\" for read: %s\n", argv[0],
					argv[optind], strerror(errno));
				exit(EXIT_FAILURE);
			}
		break;
	default:
		fprintf(stderr, "%s: bad number of arguments\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	if (outfile_name == NULL || strcmp(outfile_name, "-") == 0)
		outfile = stdout;
	else if ((outfile = fopen(outfile_name, "w")) == NULL) {
		fprintf(stderr, "%s: could not open "
			"file \"%s\" for write: %s\n", argv[0], optarg,
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	cue2toc(infile, outfile, wavefile, nocdtext);

	return EXIT_SUCCESS;
}

void
usage(char *prog)
{
	printf("Usage: %s [-hnv] [-o tocfile] [-w wavefile] [cuefile]\n", prog);
	printf(" -h\t\tdisplay this help message\n");
	printf(" -n\t\tdo not write CD-Text information\n");
	printf(" -o tocfile\twrite output to tocfile\n");
	printf(" -v\t\tbe verbose\n");
	printf(" -w wavefile\tname of WAVE file to be used in tocfile\n");
}
