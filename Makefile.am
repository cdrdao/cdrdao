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

SUBDIRS = $(MAYBE_SCSILIB) pccts trackdb utils paranoia dao $(MAYBE_XDAO)

DIST_SUBDIRS = trackdb utils paranoia dao xdao

EXTRA_DIST =		\
	AUTHORS		\
	ChangeLog	\
	COPYING		\
	CREDITS		\
	INSTALL		\
	NEWS		\
	README		\
	README.FreeBSD	\
	README.PlexDAE	\
	README.Win32	\
	scsilib		\
	pccts		\
	specs/*.in
