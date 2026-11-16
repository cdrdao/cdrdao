bin_PROGRAMS = gcdmaster

gcdmaster_SOURCES = \
	AddFileDialog.h		\
	AddFileDialog.cc	\
	AddSilenceDialog.h	\
	AddSilenceDialog.cc	\
	AudioCDProject.h	\
	AudioCDProject.cc 	\
	AudioCDRead.h		\
	AudioCDRead.cc		\
	AudioCDView.h		\
	AudioCDView.cc		\
	BlankCDDialog.cc	\
	CddbWindow.h		\
	CddbWindow.cc		\
	CdDevice.h		\
	CdDevice.cc		\
	DeviceMonitor.h		\
	DeviceMonitor.cc	\
	CdrdaoDirect.h		\
	CdrdaoDirect.cc		\
	ConfigManager.cc	\
	DeviceSelector.h	\
	DeviceSelector.cc	\
	gcdmaster.cc 		\
	main.cc			\
	guiUpdate.cc 		\
	MessageBox.h		\
	MessageBox.cc 		\
	PreferencesDialog.cc	\
	ProcessMonitor.cc	\
	ProgressDialog.cc	\
	RecordCDSource.h	\
	RecordCDSource.cc	\
	RecordCDTarget.h	\
	RecordCDTarget.cc	\
	RecordTocDialog.h	\
	RecordTocDialog.cc	\
	RecordTocSource.h	\
	RecordTocSource.cc	\
	SampleDisplay.h		\
	SampleDisplay.cc	\
	SampleManager.h		\
	SampleManager.cc	\
	Settings.h		\
	Settings.cc 		\
	SoundIF.h		\
	TaskManager.h		\
	TaskManager.cc		\
	TextEdit.h		\
	TextEdit.cc		\
	TocEdit.h		\
	TocEdit.cc 		\
	TocEditView.h		\
	TocEditView.cc 		\
	TocInfoDialog.h		\
	TocInfoDialog.cc	\
	TrackDataScrap.h	\
	TrackDataScrap.cc	\
	TrackInfoDialog.h	\
	TrackInfoDialog.cc	\
	TrackManager.h		\
	TrackManager.cc		\
	gcdmaster.h		\
	GenericView.h		\
	BlankCDDialog.h		\
	guiUpdate.h		\
	ProcessMonitor.h	\
	xcdrdao.h		\
	ProgressDialog.h	\
	ConfigManager.h		\
	PreferencesDialog.h
	Settings.h

EXTRA_gcdmaster_SOURCES =	\
	SoundIF-ao.cc 		\
	SoundIF-linux.cc 	\
	SoundIF-none.cc 	\
	SoundIF-solaris.cc	\
	DeviceMonitorLinux.cc	\
	DeviceMonitorFreeBSD.cc	\
	DeviceMonitorNetBSD.cc	\
	DeviceMonitorOSX.cc	\
	DeviceMonitorWindows.cc	\
	DeviceMonitorNull.cc

nodist_gcdmaster_SOURCES = resources.cc

BUILT_SOURCES = resources.cc resources.h

gcdmaster_LDADD =					\
	@sound_if_obj@					\
	@device_monitor_obj@				\
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
	@sound_if_obj@					\
	@device_monitor_obj@

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
