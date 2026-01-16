bin_PROGRAMS = gcdmaster

gcdmaster_SOURCES = \
	AddFileDialog.cc 	\
	AddSilenceDialog.cc 	\
	AudioCDProject.cc 	\
	AudioCDView.cc 		\
	BlankCDDialog.cc 	\
	CdDevice.cc 		\
	CdTextDialog.cc 	\
	ConfigManager.cc	\
	DeviceConfDialog.cc 	\
	DeviceList.cc 		\
	DumpCDProject.cc 	\
	DuplicateCDProject.cc 	\
	gcdmaster.cc 		\
	guiUpdate.cc 		\
	Icons.cc		\
	MessageBox.cc 		\
	PreferencesDialog.cc	\
	ProcessMonitor.cc 	\
	ProgressDialog.cc 	\
	Project.cc 		\
	ProjectChooser.cc 	\
	RecordCDSource.cc 	\
	RecordCDTarget.cc 	\
	RecordHDTarget.cc 	\
	RecordTocDialog.cc 	\
	RecordTocSource.cc 	\
	SampleDisplay.cc 	\
	SampleManager.cc 	\
	Settings.cc 		\
	TaskManager.cc		\
	TextEdit.cc 		\
	TocEdit.cc 		\
	TocEditView.cc 		\
	TocInfoDialog.cc 	\
	TrackDataScrap.cc 	\
	TrackInfoDialog.cc 	\
	TrackManager.cc 	\
	xcdrdao.cc		\
	AddFileDialog.h		\
	DumpCDProject.h		\
	ProjectChooser.h	\
	SoundIF.h		\
	AddSilenceDialog.h	\
	DuplicateCDProject.h	\
	Project.h		\
	TextEdit.h		\
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

gladedir = $(datadir)/gcdmaster/glade
glade_DATA = 			\
	glade/Preferences.glade	\
	glade/ProjectChooser.glade

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
	-I$(srcdir)/../dao			\
	-DCDRDAO_GLADEDIR=\""$(gladedir)"\"

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
icon_DATA = gcdmaster.pngff gcdmaster-doc.pngff

gsettings_SCHEMAS = org.gnome.gcdmaster.gschema.xml
@GSETTINGS_RULES@

resources.cc: gresource.xml
	glib-compile-resources \
		--target=$@ \
		--sourcedir=$(srcdir) \
		--generate-source \
		$(srcdir)/gresource.xml
resources.h: gresource.xml
	glib-compile-resources \
		--target=$@ \
		--sourcedir=$(srcdir) \
		--generate-header \
		$(srcdir)/gresource.xml

CLEANFILES = resources.cc resources.h

EXTRA_DIST = \
	$(man1_MANS)	\
	$(glade_DATA)	\
	$(mimeinfo_DATA)\
	$(desktop_DATA)	\
	$(icon_DATA)	\
	$(freedesktop_DATA)	\
	$(appreg_DATA)	\
	gresource.xml \
	org.gnome.gcdmaster.gschema.xml \
	$(RESOURCE_DEPS) \
	BUGS TASKS
