ACLOCAL_AMFLAGS = -I m4

if COND_GCDMASTER
MAYBE_GCDMASTER = gcdmaster
else
MAYBE_GCDMASTER =
endif

SUBDIRS = trackdb utils paranoia dao $(MAYBE_GCDMASTER)

DIST_SUBDIRS = trackdb utils paranoia dao gcdmaster

EXTRA_DIST =		\
	pccts/h/*.h	\
	AUTHORS		\
	ChangeLog	\
	COPYING		\
	CREDITS		\
	INSTALL		\
	NEWS		\
	README		\
	README.PlexDAE	\
	README.Win32	\
	specs/*.in	\
	aspi.reg	\
	cdrdao.lsm	\
	testtocs/*.toc	\
	contrib

dist-hook:
	rm -rf `find $(distdir)/contrib -name CVS`




