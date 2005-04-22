IMAGES =		\
	play.png	\
	pause.png	\
	stop.png	\
	gcdmaster.png	\
	open.png	\
	audiocd.png	\
	copycd.png	\
	dumpcd.png	\
	record.png

noinst_DATA = pixbufs.h
CLEANFILES = $(noinst_DATA)

pixbufs.h: $(IMAGES)
	gdk-pixbuf-csource --raw --name play_pixbuf $(srcdir)/play.png > $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name pause_pixbuf $(srcdir)/pause.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name stop_pixbuf $(srcdir)/stop.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name gcdmaster_pixbuf $(srcdir)/gcdmaster.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name open_pixbuf $(srcdir)/open.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name audiocd_pixbuf $(srcdir)/audiocd.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name copycd_pixbuf $(srcdir)/copycd.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name dumpcd_pixbuf $(srcdir)/dumpcd.png >> $(srcdir)/pixbufs.h
	gdk-pixbuf-csource --raw --name record_pixbuf $(srcdir)/record.png >> $(srcdir)/pixbufs.h

EXTRA_DIST = $(IMAGES) pixbufs.h
