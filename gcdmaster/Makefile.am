SUBDIRS = stock

bin_PROGRAMS = gcdmaster

gcdmaster_SOURCES = \
	AddFileDialog.cc	\
	AddSilenceDialog.cc	\
	AudioCDProject.cc	\
	AudioCDView.cc		\
	BlankCDWindow.cc	\
	CdDevice.cc		\
	CdTextDialog.cc		\
	ConfigManager.cc	\
	DeviceList.cc		\
	DumpCDProject.cc	\
	DuplicateCDProject.cc	\
	MessageBox.cc		\
	PreferencesDialog.cc	\
	ProcessMonitor.cc	\
	ProgressDialog.cc	\
	Project.cc		\
	ProjectChooser.cc	\
	RecordCDSource.cc	\
	RecordCDTarget.cc	\
	RecordHDTarget.cc	\
	RecordTocDialog.cc	\
	RecordTocSource.cc	\
	SampleDisplay.cc	\
	SampleManager.cc	\
	TaskManager.cc		\
	TextEdit.cc		\
	TocEdit.cc		\
	TocEditView.cc		\
	TocInfoDialog.cc	\
	TrackDataScrap.cc	\
	TrackInfoDialog.cc	\
	TrackManager.cc		\
	gcdmaster.cc		\
	guiUpdate.cc		\
	main.cc

nodist_gcdmaster_SOURCES =	\
	resources.cc

EXTRA_gcdmaster_SOURCES =	\
	SoundIF-ao.cc 		\
	SoundIF-linux.cc 	\
	SoundIF-none.cc 	\
	SoundIF-solaris.cc

resources.cc: ui/gcdmaster.gresource.xml ui/*.ui ui/*.png
	glib-compile-resources --target=$@ --sourcedir=ui  --generate-source $<

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

icondir = $(datadir)/icons
icon_DATA = gcdmaster.png gcdmaster-doc.png

AM_CPPFLAGS =					\
	-I$(srcdir)/../trackdb			\
	-I$(srcdir)/../paranoia			\
	-I$(srcdir)/../dao			\
	-DGCDMASTER_GLADEDIR=\""$(gladedir)"\"	\
	-DGCDMASTER_ICONDIR=\""$(icons)"\"

man1_MANS = gcdmaster.man

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = gcdmaster.keys gcdmaster.mime

freedesktopdir = $(datadir)/mime/packages
freedesktop_DATA = gcdmaster.xml

desktopdir = $(datadir)/applications
desktop_DATA = gcdmaster.desktop

appregdir = $(datadir)/application-registry
appreg_DATA = gcdmaster.applications

gsettings_SCHEMAS = org.gnome.gcdmaster.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(man1_MANS)	\
	$(glade_DATA)	\
	$(mimeinfo_DATA)\
	$(desktop_DATA)	\
	$(icon_DATA)	\
	$(freedesktop_DATA)	\
	$(appreg_DATA)	\
	BUGS TASKS 	\
	org.gnome.gcdmaster.gschema.xml

CLEANFILES = resources.cc

check-syntax:
	$(CXXCOMPILE) -o /dev/null -S ${CHK_SOURCES}
