SUBDIRS = stock

bin_PROGRAMS = gcdmaster

# gcdmaster_SOURCES = \
# 	AddFileDialog.cc 	\
# 	AddSilenceDialog.cc 	\
# 	AudioCDProject.cc 	\
# 	BlankCDWindow.cc 	\
# 	CdDevice.cc 		\
# 	CdTextDialog.cc 	\
# 	ConfigManager.cc	\
# 	DeviceConfDialog.cc 	\
# 	DeviceList.cc 		\
# 	DuplicateCDProject.cc 	\
# 	gcdmaster.cc 		\
# 	GenericView.cc 		\
# 	guiUpdate.cc 		\
# 	Icons.cc		\
# 	MessageBox.cc 		\
# 	ProcessMonitor.cc 	\
# 	ProgressDialog.cc 	\
# 	Project.cc 		\
# 	ProjectChooser.cc 	\
# 	RecordCDSource.cc 	\
# 	RecordCDTarget.cc 	\
# 	SampleManager.cc 	\
# 	TextEdit.cc 		\
# 	TocEdit.cc 		\
# 	TocEditView.cc 		\
# 	TrackDataScrap.cc 	\
# 	TrackInfoDialog.cc 	\
# 	TrackManager.cc 	\
# 	xcdrdao.cc		\
# 	AddFileDialog.h		\
# 	ProjectChooser.h	\
# 	SoundIF.h		\
# 	AddSilenceDialog.h	\
# 	DuplicateCDProject.h	\
# 	Project.h		\
# 	TextEdit.h		\
# 	gcdmaster.h		\
# 	RecordCDSource.h	\
# 	TocEdit.h		\
# 	AudioCDProject.h	\
# 	RecordCDTarget.h	\
# 	TocEditView.h		\
# 	GenericView.h		\
# 	BlankCDWindow.h		\
# 	guiUpdate.h		\
# 	TrackDataScrap.h	\
# 	CdDevice.h		\
# 	Icons.h			\
# 	TrackInfoDialog.h	\
# 	CdTextDialog.h		\
# 	MessageBox.h		\
# 	TrackManager.h		\
# 	DeviceConfDialog.h	\
# 	ProcessMonitor.h	\
# 	SampleManager.h		\
# 	xcdrdao.h		\
# 	DeviceList.h		\
# 	ProgressDialog.h	\
# 	ConfigManager.h		\
# 	PreferencesDialog.h

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
	GenericView.cc		\
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
	resources.c

EXTRA_gcdmaster_SOURCES =	\
	SoundIF-ao.cc 		\
	SoundIF-linux.cc 	\
	SoundIF-none.cc 	\
	SoundIF-solaris.cc

resources.c: ui/gcdmaster.gresource.xml ui/*.ui ui/*.png
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
	@GTKMM3_LIBS@

AM_CXXFLAGS = @GTKMM3_CFLAGS@ @AO_CFLAGS@
AM_CFLAGS = @GTK3_CFLAGS@

if COND_MP3
gcdmaster_LDADD += @MAD_LIBS@
AM_CXXFLAGS += @MAD_CFLAGS@
endif

if COND_OGG
gcdmaster_LDADD += @VORBISFILE_LIBS@
AM_CXXFLAGS += @VORBISFILE_CFLAGS@
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

# gsettings_SCHEMAS is a list of all the schemas you want to install
gsettings_SCHEMAS = org.gnome.gcdmaster.gschema.xml

# include the appropriate makefile rules for schema handling
@GSETTINGS_RULES@

CLEANFILES = resources.c

check-syntax:
	$(CXXCOMPILE) -o /dev/null -S ${CHK_SOURCES}
