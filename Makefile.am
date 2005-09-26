if COND_XDAO
MAYBE_XDAO = xdao
else
MAYBE_XDAO = 
endif

if COND_SCSILIB
MAYBE_SCSILIB = scsilib
else
MAYBE_SCSILIB =
endif

if COND_PCCTS
MAYBE_PCCTS = pccts
else
MAYBE_PCCTS =
endif

SUBDIRS = $(MAYBE_SCSILIB) $(MAYBE_PCCTS) trackdb utils paranoia dao $(MAYBE_XDAO)

DIST_SUBDIRS = trackdb utils paranoia dao xdao pccts

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
	scsilib		\
	specs/*.in	\
	aspi.reg	\
	cdrdao.lsm	\
	testtocs/*.toc	\
	contrib

dist-hook:
	rm -rf `find $(distdir)/contrib -name CVS`




