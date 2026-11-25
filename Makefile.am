ACLOCAL_AMFLAGS = -I m4

if COND_GCDMASTER
MAYBE_GCDMASTER = gcdmaster
else
MAYBE_GCDMASTER =
endif

SUBDIRS = trackdb utils paranoia dao $(MAYBE_GCDMASTER)

DIST_SUBDIRS = trackdb utils paranoia dao gcdmaster

EXTRA_DIST =		\
	AUTHORS		\
	ChangeLog	\
	COPYING		\
	CREDITS		\
	INSTALL		\
	NEWS		\
	README.md	\
	README.PlexDAE	\
	specs/cdrdao.fedora.spec.in	\
	aspi.reg	\
	cdrdao.lsm	\
	testtocs	\
	contrib

dist-hook:
	rm -rf `find $(distdir)/contrib -name CVS`




