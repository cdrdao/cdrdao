/* @(#)getargs.c	2.24 99/05/13 Copyright 1985, 1988, 1995 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)getargs.c	2.24 99/05/13 Copyright 1985, 1988, 1995 J. Schilling";
#endif
#define	NEW
/*
 *	Copyright (c) 1985, 1988, 1995 J. Schilling
 *
 *	1.3.88	 Start implementation of release 2
 */
/*
 *	Parse arguments on a command line.
 *	Format string specifier (appearing directly after flag name):
 *		''	BOOL
 *		'*'	string
 *		'?'	char
 *		'#'	number
 *		'&'	call function
 *		'+'	inctype		+++ NEU +++
 */
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
/* LINTLIBRARY */
#include <mconfig.h>
#include <standard.h>
#include <getargs.h>
#include <ctype.h>
#include <vadefs.h>
#include <strdefs.h>

#define	NOARGS		  0	/* No more args			*/
#define	NOTAFLAG	  1	/* Not a flag type argument	*/
#define BADFLAG		(-1)	/* Not a valid flag argument	*/
#define BADFMT		(-2)	/* Error in format string	*/
#define NOTAFILE	(-3)	/* Seems to be a flag type arg	*/


	int	_getargs __PR((int *, char *const **, const char *,
							BOOL, va_list));
LOCAL	int	dofile __PR((int *, char *const **, const char **));
LOCAL	int	doflag __PR((int *, char *const **, const char *,
						const char *, BOOL, va_list));
LOCAL	int	dosflags __PR((const char *, const char *, BOOL, va_list));
LOCAL	int	checkfmt __PR((const char *));
LOCAL	int	checkeql __PR((const char *));

LOCAL	va_list	va_dummy;

LOCAL	char	fmtspecs[] = "#?*&+"; 

#define	isfmtspec(c)		(strchr(fmtspecs, c) != NULL)

/*---------------------------------------------------------------------------
|
|	get flags until a non flag type argument is reached
|
+---------------------------------------------------------------------------*/
/* VARARGS3 */
#ifdef	PROTOTYPES
int getargs(int *pac, char *const **pav, const char *fmt, ...)
#else
int getargs(pac, pav, fmt, va_alist)
	int	*pac;
	char	**pav[];
	char	*fmt;
	va_dcl
#endif
{
	va_list	args;
	int	ret;

#ifdef	PROTOTYPES
	va_start(args, fmt);
#else
	va_start(args);
#endif
	ret = _getargs(pac, pav, fmt, TRUE, args);
	va_end(args);
	return (ret);
}


/*---------------------------------------------------------------------------
|
|	get all flags on the command line, do not stop on files
|
+---------------------------------------------------------------------------*/
/* VARARGS3 */
#ifdef	PROTOTYPES
int getallargs(int *pac, char *const **pav, const char *fmt, ...)
#else
int getallargs(pac, pav, fmt, va_alist)
	int	*pac;
	char	**pav[];
	char	*fmt;
	va_dcl
#endif
{
	va_list	args;
	int	ret;

#ifdef	PROTOTYPES
	va_start(args, fmt);
#else
	va_start(args);
#endif
	for (;; (*pac)--, (*pav)++) {
		if ((ret = _getargs(pac, pav, fmt, TRUE, args)) != NOTAFLAG)
			break;
	}
	va_end(args);
	return (ret);
}


/*---------------------------------------------------------------------------
|
|	get next non flag type argument (i.e. a file)
|
+---------------------------------------------------------------------------*/
int getfiles(pac, pav, fmt)
	int		*pac;
	char *const	*pav[];
	const char	*fmt;
{
	return (_getargs(pac, pav, fmt, FALSE, va_dummy));
}


/*---------------------------------------------------------------------------
|
|	check args until the next non flag type argmument is reached
|	*pac is decremented, *pav is incremented so that the
|	non flag type argument is at *pav[0]
|
|	return code:
|		NOARGS		no more args
|		NOTAFLAG	not a flag type argument
|		BADFLAG		a non-matching flag type argument
|		BADFMT		bad syntax in format string
|
|
+---------------------------------------------------------------------------*/
/*LOCAL*/ int _getargs(pac, pav, fmt, setargs, args)
	register int		*pac;
	register char	*const	**pav;
		 const char	*fmt;
		BOOL		setargs;
		va_list		args;
{
	const	 char	*argp;
		 int	ret;


	for(; *pac > 0; (*pac)--, (*pav)++) {
		argp = **pav;

		ret = dofile(pac, pav, &argp);

		if (ret != NOTAFILE)
			return (ret);

		ret = doflag(pac, pav, argp, fmt, setargs, args);

		if (ret != NOTAFLAG)
			return (ret);
	}
	return (NOARGS);
}


/*---------------------------------------------------------------------------
|
| check if *pargp is a file type argument
|
+---------------------------------------------------------------------------*/
LOCAL int dofile(pac, pav, pargp)
	register int		*pac;
	register char *const	**pav;
		 const char	**pargp;
{
	register const char	*argp = *pargp;


	if (argp[0] == '-') {
		/*
		 * "-" is a special non flag type argument
		 *     that usually means take stdin instead of a named file
		 */
		if (argp[1] == '\0')
			return (NOTAFLAG);
		/*
		 * "--" is a prefix to take the next argument
		 *	as non flag type argument
		 * NOTE: Posix requires "--" to indicate the end of the
		 *	 flags on the command line. But we are currently not
		 *	 Posix.
		 */
		if (argp[1] == '-' && argp[2] == '\0') {
			if (--(*pac) > 0) {
				(*pav)++;
				return (NOTAFLAG);
			} else {
				return (NOARGS);
			}
		}
	}

	/*
	 * now check if it may be flag type argument
	 * flag type arguments begin with a '-', a '+' or contain a '='
	 * i.e. -flag +flag or flag=
	 */
	if (argp[0] != '-' && argp[0] != '+' && (!checkeql(argp)))
		return (NOTAFLAG);

	/*
	 * flags beginning with '-' don't have to include the '-' in
	 * the format string.
	 * flags beginning with '+' have to include it in the format string.
	 */
	if (argp[0] == '-')
		(*pargp)++;

	return (NOTAFILE);
}


/*---------------------------------------------------------------------------
|
|	compare argp with the format string
|	if a match is found store the result a la scanf in one of the
|	arguments pointed to in the va_list
|
|	If setargs is FALSE, only check arguments for getfiles()
|	in this case, va_list may be a dummy arg.
|
+---------------------------------------------------------------------------*/
LOCAL int doflag(pac, pav, argp, fmt, setargs, oargs)
		int		*pac;
		char	*const	**pav;
	register const char	*argp;
	register const char	*fmt;
		BOOL		setargs;
		va_list		oargs;
{
	long	val;
	int	singlecharflag	= 0;
	BOOL	isspec;
	BOOL	haseql		= checkeql(argp);
	const char	*sargp	= argp;
	const char	*sfmt	= fmt;
	va_list	args;
	char	*const	*spav	= *pav;
	int		spac	= *pac;
	void		*curarg	= (void *)0;

	/*
	 * Initialize 'args' to the start of the argument list.
	 * I don't know any portable way to copy an arbitrary
	 * C object so I use a system-specific routine
	 * (probably a macro) from stdarg.h.  (Remember that
	 * if va_list is an array, 'args' will be a pointer
	 * and '&args' won't be what I would need for memcpy.)
	 * It is a system requirement for SVr4 compatibility
	 * to be able to do this assgignement. If your system
	 * defines va_list to be an array but does not define
	 * va_copy() you are lost.
	 * This is needed to make sure, that 'oargs' will not
	 * be clobbered.
	 */
	va_copy(args, oargs);

	if (setargs)
		curarg = va_arg(args, void *);

	/*
	 * check if the first flag in format string is a singlechar flag
	 */
	if (fmt[1] == ',' || fmt[1] == '+' || fmt[1] == '\0')
		singlecharflag++;
	/*
	 * check the whole format string for a match
	 */
	for(;;) {
		for(;*fmt; fmt++,argp++) {
			if (*fmt == '\\') {
				/*
				 * Allow "#?*&+" to appear inside a flag.
				 * NOTE: they must be escaped by '\\' only
				 *	 inside the the format string.
				 */
				fmt++;
				isspec = FALSE;
			} else {
				isspec = isfmtspec(*fmt);
			}
			/*
			 * If isspec is TRUE, the arg beeing checked starts
			 * like a valid flag. Argp now points to the rest.
			 */
			if (isspec) {
				/*
				 * If *argp is '+' and we are on the
				 * beginning of the arg that is currently
				 * checked, this cannot be an inc type flag.
				 */
				if (*argp == '+' && argp == sargp)
					continue;
				/*
				 * skip over to arg of flag
				 */
				if (*argp == '=') {
					argp++;
				} else if (*argp != '\0' && haseql) {
					/*
					 * Flag and arg are not separated by a
					 * space.
					 * Check here for:
					 * xxxxx=yyyyy	match on '&'
					 * Checked before:
					 * abc=yyyyy	match on 'abc&'
					 * 		or	 'abc*' 
					 * 		or	 'abc#' 
					 * We come here if 'argp' starts with
					 * the same sequence as a valid flag
					 * and contains an equal sign.
					 * We have tested before if the text
					 * before 'argp' matches exactly.
					 * At this point we have no exact match
					 * and we only allow to match
					 * the special pattern '&'.
					 * We need this e.g. for 'make'.
					 * Here allow any flag type argument to
					 * match the format string "&" to set
					 * up a function that handles all odd
					 * stuff that getargs will not grok.
					 */
					if (argp != sargp || *fmt != '&')
						goto nextarg;
				}
				/*
				 * *arpp == '\0' || !haseql
				 * We come here if 'argp' starts with
				 * the same sequence as a valid flag.
				 * This will match on the following args:
				 * -farg	match on 'f*'
				 * -f12		match on 'f#'
				 * +12		match on '+#'
				 * -12		match on '#'
				 * and all args that are separated from
				 * their flags.
				 * In the switch statement below, we check
				 * if the text after 'argp' (if *argp != 0) or
				 * the next arg is a valid arg for this flag.
 				 */
				break;
			} else if (*fmt == *argp) {
				if (argp[1] == '\0' &&
				    (fmt[1] == '\0' || fmt[1] == ',')) {

					if (setargs)
						*((int *)curarg) = TRUE;


					return (checkfmt(fmt));/* XXX */
				}
			} else {
				/*
				 * skip over to next format identifier
				 * & reset arg pointer
				 */
			nextarg:
				while (*fmt != ',' && *fmt != '\0') {
					/* function has extra arg on stack */
					if (*fmt == '&' && setargs)
						curarg = va_arg(args, void *);
					fmt++;
				}
				argp = sargp;
				break;
			}
		}
		switch(*fmt) {

		case '\0':
			/*
			 * Boolean type has been tested before.
			 */
			if (singlecharflag && 
			   (val = dosflags(sargp, sfmt, setargs, oargs)) !=
								BADFLAG)
				return (val);


			return (BADFLAG);

		case ',':
			fmt++;
			if (fmt[1] == ',' || fmt[1] == '+' || fmt[1] == '\0')
				singlecharflag++;
			if (setargs)
				curarg = va_arg(args, void *);
			continue;

		case '*':
			if (*argp == '\0') {
				if (*pac > 1) {
					(*pac)--;
					(*pav)++;
					argp = **pav;
				} else {
					return (BADFLAG);
				}
			}
			if (setargs)
				*((const char **)curarg) = argp;


			return (checkfmt(fmt));

		case '?':
			/*
			 * If more than one char arg, it
			 * cannot be a character argument.
			 */
			if (argp[1] != '\0')
				goto nextchance;
			if (setargs)
				*((char *)curarg) = *argp;


			return (checkfmt(fmt));

		case '+':
			/*
			 * inc type is similar to boolean,
			 * there is no arg in argp to convert.
			 */
			if (*argp != '\0')
				goto nextchance;
			if (fmt[1] == 'l' || fmt[1] == 'L') {
				if (setargs)
					*((long *)curarg) += 1;
				fmt++;
			} else if (fmt[1] == 's' || fmt[1] == 'S') {
				if (setargs)
					*((short *)curarg) += 1;
				fmt++;
			} else {
				if (fmt[1] == 'i' || fmt[1] == 'I')
					fmt++;
				if (setargs)
					*((int *)curarg) += 1;
			}


			return (checkfmt(fmt));

		case '#':
			if (*argp == '\0') {
				if (*pac > 1) {
					(*pac)--;
					(*pav)++;
					argp = **pav;
				} else {
					return (BADFLAG);
				}
			}
			if (*astol(argp, &val) != '\0') {
				/*
				 * arg is not a valid number!
				 * go to next format in the format string
				 * and check if arg matches any other type
				 * in the format specs.
				 */
			nextchance:
				while(*fmt != ',' && *fmt != '\0') {
					if (*fmt == '&' && setargs)
						curarg = va_arg(args, void *);
					fmt++;
				}
				argp = sargp;
				*pac = spac;
				*pav = spav;
				continue;
			}
			if (fmt[1] == 'l' || fmt[1] == 'L') {
				if (setargs)
					*((long *)curarg) = val;
				fmt++;
			} else if (fmt[1] == 's' || fmt[1] == 'S') {
				if (setargs)
					*((short *)curarg) = val;
				fmt++;
			} else {
				if (fmt[1] == 'i' || fmt[1] == 'I')
					fmt++;
				if (setargs)
					*((int *)curarg) = val;
			}

			return (checkfmt(fmt));

		case '&':
			if (*argp == '\0') {
				if (*pac > 1) {
					(*pac)--;
					(*pav)++;
					argp = **pav;
				} else {
					return (BADFLAG);
				}
			}

			if ((val = checkfmt(fmt)) != NOTAFLAG)
				return (val);

			if (setargs) {
				int	ret;
				void	*funarg = va_arg(args, void *);

				ret = ((*(getargfun)curarg) (argp, funarg));
				if (ret != NOTAFILE)
					return (ret);
				fmt++;
			} else {
				return (val);
			}
			/*
			 * Called function returns NOTAFILE: try next format.
			 */
		}
	}
}


/*---------------------------------------------------------------------------
|
|	parse args for combined single char flags
|
+---------------------------------------------------------------------------*/
typedef struct {
	void	*curarg;
	short	count;
	char	c;
	char	type;
} sflags;

LOCAL int dosflags(argp, fmt, setargs, oargs)
	register const char	*argp;
	register const char	*fmt;
		BOOL		setargs;
		va_list		oargs;
{
#define	MAXSF	32
		 sflags	sf[MAXSF];
		 va_list args;
	register sflags	*rsf	= sf;
	register int	nsf	= 0;
	register const char *p	= argp;
	register int	i;
	register void	*curarg = (void *)0;

	/*
	 * Initialize 'args' to the start of the argument list.
	 * I don't know any portable way to copy an arbitrary
	 * C object so I use a system-specific routine
	 * (probably a macro) from stdarg.h.  (Remember that
	 * if va_list is an array, 'args' will be a pointer
	 * and '&args' won't be what I would need for memcpy.)
	 * It is a system requirement for SVr4 compatibility
	 * to be able to do this assgignement. If your system
	 * defines va_list to be an array but does not define
	 * va_copy() you are lost.
	 * This is needed to make sure, that 'oargs' will not
	 * be clobbered.
	 */
	va_copy(args, oargs);

	if (setargs)
		curarg = va_arg(args, void *);

	while (*p) {
		for (i=0; i < nsf; i++) {
			if (rsf[i].c == *p)
				break;
		}
		if (i >= MAXSF)
			return (BADFLAG);
		if (i == nsf) {
			rsf[i].curarg = (void *)0;
			rsf[i].count = 0;
			rsf[i].c = *p;
			rsf[i].type = (char)-1;
			nsf++;
		}
		rsf[i].count++;
		p++;
	}

	while (*fmt) {
		if (!isfmtspec(*fmt) &&
		    (fmt[1] == ',' || fmt[1] == '+' || fmt[1] == '\0') &&
		     strchr(argp, *fmt)) {
			for (i=0; i < nsf; i++) {
				if (rsf[i].c == *fmt) {
					if (fmt[1] == '+') {
						fmt++;
						if (fmt[1] == ',' ||
						    fmt[1] == '\0') {
							rsf[i].type = 'i';
						} else {
							rsf[i].type = fmt[1];
						}
					} else {
						rsf[i].type = fmt[1];
					}
					rsf[i].curarg = curarg;
					break;
				}
			}
		}
		while (*fmt != ',' && *fmt != '\0') {
			/* function has extra arg on stack */
			if (*fmt == '&' && setargs)
				curarg = va_arg(args, void *);
			fmt++;
		}
		if (*fmt != '\0')
			fmt++;

		if (setargs)
			curarg = va_arg(args, void *);
	}
	for (i=0; i < nsf; i++) {
		if (rsf[i].type == (char)-1)
			return (BADFLAG);
		if (rsf[i].curarg) {
			if (rsf[i].type == ',' || rsf[i].type == '\0') {
				*((int *)rsf[i].curarg) = TRUE;
			} else if (rsf[i].type == 'i' || rsf[i].type == 'I') {
				*((int *)rsf[i].curarg) += rsf[i].count;
			} else if (rsf[i].type == 'l' || rsf[i].type == 'L') {
				*((long *)rsf[i].curarg) += rsf[i].count;
			} else if (rsf[i].type == 's' || rsf[i].type == 'S') {
				*((short *)rsf[i].curarg) += rsf[i].count;
			} else {
				return (BADFLAG);
			}
		}
	}
	return (NOTAFLAG);
}

/*---------------------------------------------------------------------------
|
|	If the next format character is a comma or the string delimiter,
|	there are no invalid format specifiers. Return success.
|	Otherwise raise the getarg_bad_format condition.
|
+---------------------------------------------------------------------------*/
LOCAL int checkfmt(fmt)
	const char	*fmt;
{
	char	c;

	c = *(++fmt);	/* non constant expression */


	if (c == ',' || c == '\0') {
		return (NOTAFLAG);
	} else {
		raisecond("getarg_bad_format", (long)fmt);
		return (BADFMT);
	}
}

/*---------------------------------------------------------------------------
|
|	Parse the string as long as valid characters can be found.
|	Valid flag identifiers are chosen from the set of
|	alphanumeric characters, '-' and '_'.
|	If the next character is an equal sign the string
|	contains a valid flag identifier.
|
+---------------------------------------------------------------------------*/
static int checkeql(str)
	register const char *str;
{
	register unsigned char c;

	for (c = (unsigned char)*str;
				isalnum(c) || c == '_' || c == '-'; c = *str++)
		;
	return (c == '=');
}
