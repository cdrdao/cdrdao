/* @(#)saveargs.c	1.7 98/05/31 Copyright 1995 J. Schilling */
/* save argc, argv for command error printing routines */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <mconfig.h>
#include <standard.h>
#include <strdefs.h>
#include <stdxlib.h>
#include <avoffset.h>

#if	!defined(AV_OFFSET) || !defined(FP_INDIR)
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif
#ifdef	NO_SCANSTACK
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif

static	int	ac_saved;
static	char	**av_saved;
static	char	*av0_saved;
static	char	*progname_saved;

static	char	av0_sp[32];	/* av0 space, avoid malloc() in most cases */
static	char	prn_sp[32];	/* name space, avoid malloc() in most cases */
static	char	dfl_str[] = "?";

void save_args(ac, av)
	int	ac;
	char	*av[];
{
	int	slen;

	ac_saved = ac;
	av_saved = av;

	if (av0_saved && av0_saved != av0_sp)
		free(av0_saved);

	slen = strlen(av[0]) + 1;

	if (slen <= sizeof(av0_sp))
		av0_saved = av0_sp;
	else
		av0_saved = malloc(slen);

	if (av0_saved)
		strcpy(av0_saved, av[0]);
}

int saved_ac()
{
	return (ac_saved);
}

char **saved_av()
{
	return (av_saved);
}

char *saved_av0()
{
	return (av0_saved);
}

void set_progname(name)
	const char	*name;
{
	int	slen;

	if (progname_saved && progname_saved != prn_sp)
		free(progname_saved);

	slen = strlen(name) + 1;

	if (slen <= sizeof(prn_sp))
		progname_saved = prn_sp;
	else
		progname_saved = malloc(slen);

	if (progname_saved)
		strcpy(progname_saved, name);
}

char *get_progname()
{
#ifdef	HAVE_SCANSTACK
	char	*progname;
#endif

	if (progname_saved)
		return (progname_saved);
	if (av0_saved)
		return (av0_saved);
#ifdef	HAVE_SCANSTACK
	progname = getav0();		/* scan stack to get argv[0] */
	if (progname)
		return (progname);
#endif
	return (dfl_str);
}
