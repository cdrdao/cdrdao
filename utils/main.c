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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "cue2toc.h"

const char *progname = NULL;
int verbose;
void usage(void);

int
main(int argc, char *argv[])
{
	char *wavefile = NULL;
	char *outfile = NULL;
	char *infile = NULL;
	int nocdtext = 0;
	int c;

	struct cuesheet *cs;

	progname = argv[0];
	verbose = 1;
	opterr = 0;	/* we do error msgs ourselves */
	while ((c = getopt(argc, argv, ":hno:qvw:")) != -1)
		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		case 'n':
			nocdtext = 1;
			break;
		case 'o':
			if (strcmp(optarg, "-") == 0)
				outfile = NULL;		/* use stdout */
			else
				outfile = optarg;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'v':
			printf("cue2toc 0.2\n");
			printf("Report bugs to <dermatsch@gmx.de>\n");
			exit(EXIT_SUCCESS);
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
		infile = NULL;	/* use stdin */
		break;
	case 1:
		if (strcmp(argv[optind], "-") == 0)
			infile = NULL;	/* use stdin */
		else
			infile = argv[optind];
		break;
	default:
		fprintf(stderr, "%s: bad number of arguments\n",
			progname);
		exit(EXIT_FAILURE);
	}

	cs = read_cue(infile, wavefile);
	write_toc(outfile, cs, nocdtext ? 0 : 1);

	return EXIT_SUCCESS;
}

void
usage(void)
{
	printf("Usage: %s [-hnqv] [-o tocfile] [-w wavefile] [cuefile]\n",
	       progname);
	printf(" -h\t\tdisplay this help message\n");
	printf(" -n\t\tdo not write CD-Text information\n");
	printf(" -o tocfile\twrite output to tocfile\n");
	printf(" -q\t\tquiet mode\n");
	printf(" -v\t\tdisplay version information\n");
	printf(" -w wavefile\tname of WAVE file to be used in tocfile\n");
}
