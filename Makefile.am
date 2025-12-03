ACLOCAL_AMFLAGS = -I m4

if COND_GCDMASTER
MAYBE_GCDMASTER = gcdmaster
else
MAYBE_GCDMASTER =
endif

if COND_PCCTS
MAYBE_PCCTS = pccts
else
MAYBE_PCCTS =
endif

SUBDIRS = $(MAYBE_PCCTS) trackdb utils paranoia dao $(MAYBE_GCDMASTER)

DIST_SUBDIRS = trackdb utils paranoia dao gcdmaster pccts

EXTRA_DIST =		\
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




