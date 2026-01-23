bin_PROGRAMS = gcdmaster

gcdmaster_SOURCES = \
	AudioCDProject.cc 	\
	BlankCDDialog.cc	\
	CdDevice.cc		\
	ConfigManager.cc	\
	DeviceList.cc		\
	gcdmaster.cc 		\
	main.cc			\
	guiUpdate.cc 		\
	MessageBox.cc 		\
	PreferencesDialog.cc	\
	ProcessMonitor.cc	\
	TaskManager.cc		\
	Settings.cc 		\
	SampleManager.cc	\
	TrackDataScrap.cc	\
	ProgressDialog.cc	\
	TocEdit.cc 		\
	TocEditView.cc 		\
	SoundIF.h		\
	gcdmaster.h		\
	RecordCDSource.h	\
	TocEdit.h		\
	AudioCDProject.h	\
	RecordCDTarget.h	\
	TocEditView.h		\
	AudioCDView.h		\
	GenericView.h		\
	RecordHDTarget.h	\
	TocInfoDialog.h		\
	BlankCDDialog.h		\
	guiUpdate.h		\
	RecordTocDialog.h	\
	TrackDataScrap.h	\
	CdDevice.h		\
	Icons.h			\
	RecordTocSource.h	\
	TrackInfoDialog.h	\
	CdTextDialog.h		\
	MessageBox.h		\
	SampleDisplay.h		\
	TrackManager.h		\
	TaskManager.h		\
	DeviceConfDialog.h	\
	ProcessMonitor.h	\
	SampleManager.h		\
	xcdrdao.h		\
	DeviceList.h		\
	ProgressDialog.h	\
	ConfigManager.h		\
	PreferencesDialog.h	\
	Settings.h

EXTRA_gcdmaster_SOURCES =	\
	SoundIF-ao.cc 		\
	SoundIF-linux.cc 	\
	SoundIF-none.cc 	\
	SoundIF-solaris.cc

nodist_gcdmaster_SOURCES = resources.cc

BUILT_SOURCES = resources.cc resources.h

gcdmaster_LDADD =					\
	@sound_if_obj@					\
	$(top_builddir)/dao/libdao.a			\
	$(top_builddir)/paranoia/libcdda_paranoia.a	\
	$(top_builddir)/trackdb/libtrackdb.a		\
	@scsilib_libs@					\
	@GTKMM_LIBS@					\
	@LIBICONV@

AM_CXXFLAGS = @GTKMM_CFLAGS@ @AO_CFLAGS@

if COND_MP3
gcdmaster_LDADD += @MAD_LIBS@
AM_CXXFLAGS += @MAD_CFLAGS@
endif

if COND_OGG
gcdmaster_LDADD += @VORBISFILE_LIBS@
AM_CXXFLAGS += @VORBISFILE_CFLAGS@
endif

if COND_FLAC
gcdmaster_LDADD += @FLAC_LIBS@ @SAMPLERATE_LIBS@
endif

gcdmaster_LDADD += @AO_LIBS@

gcdmaster_DEPENDENCIES = \
	$(top_builddir)/dao/libdao.a			\
	$(top_builddir)/paranoia/libcdda_paranoia.a	\
	$(top_builddir)/trackdb/libtrackdb.a		\
	@sound_if_obj@

AM_CPPFLAGS =					\
	-I$(srcdir)/../trackdb			\
	-I$(srcdir)/../paranoia			\
	-I$(srcdir)/../dao

man1_MANS = gcdmaster.man

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = gcdmaster.keys gcdmaster.mime

freedesktopdir = $(datadir)/mime/packages
freedesktop_DATA = gcdmaster.xml

desktopdir = $(datadir)/applications
desktop_DATA = gcdmaster.desktop

appregdir = $(datadir)/application-registry
appreg_DATA = gcdmaster.applications

icondir = $(datadir)/pixmaps
icon_DATA = gcdmaster.png gcdmaster-doc.png

gsettings_SCHEMAS = org.gnome.gcdmaster.gschema.xml
@GSETTINGS_RULES@

resources.cc: ui/gresource.xml
	glib-compile-resources \
		--target=$@ \
		--sourcedir=ui \
		--generate-source \
		$<
resources.h: ui/gresource.xml
	glib-compile-resources \
		--target=$@ \
		--sourcedir=ui \
		--generate-header \
		$<

CLEANFILES = resources.cc resources.h

EXTRA_DIST = \
	$(man1_MANS)	\
	$(mimeinfo_DATA)\
	$(desktop_DATA)	\
	$(icon_DATA)	\
	$(freedesktop_DATA)	\
	$(appreg_DATA)	\
	gresource.xml \
	org.gnome.gcdmaster.gschema.xml \
	$(RESOURCE_DEPS) \
	BUGS TASKS
