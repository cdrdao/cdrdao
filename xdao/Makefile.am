SUBDIRS = stock

bin_PROGRAMS = gcdmaster

gcdmaster_SOURCES = \
	AddFileDialog.cc 	\
	AddSilenceDialog.cc 	\
	AudioCDChild.cc		\
	AudioCDProject.cc 	\
	AudioCDView.cc 		\
	BlankCDDialog.cc 	\
	CdDevice.cc 		\
	CdTextDialog.cc 	\
	DeviceConfDialog.cc 	\
	DeviceList.cc 		\
	DumpCDProject.cc 	\
	DuplicateCDProject.cc 	\
	gcdmaster.cc 		\
	GenericChild.cc 	\
	GenericView.cc 		\
	guiUpdate.cc 		\
	Icons.cc		\
	MessageBox.cc 		\
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
	AudioCDChild.h		\
	gcdmaster.h		\
	RecordCDSource.h	\
	TocEdit.h		\
	AudioCDProject.h	\
	GenericChild.h		\
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
	DeviceConfDialog.h	\
	ProcessMonitor.h	\
	SampleManager.h		\
	xcdrdao.h		\
	DeviceList.h		\
	ProgressDialog.h	\
	Settings.h

EXTRA_gcdmaster_SOURCES =	\
	SoundIF-linux.cc 	\
	SoundIF-none.cc 	\
	SoundIF-solaris.cc

gcdmaster_LDADD =					\
	@sound_if_obj@					\
	@scsilib_objs@					\
	-L$(top_builddir)/dao -ldao			\
	-L$(top_builddir)/paranoia -lcdda_paranoia	\
	-L$(top_builddir)/trackdb -ltrackdb		\
	@scsilib_libs@					\
	@LIBGUIMM2_LIBS@ @GTKMM2_LIBS@

gcdmaster_DEPENDENCIES = \
	$(top_builddir)/dao/libdao.a			\
	$(top_builddir)/paranoia/libcdda_paranoia.a	\
	$(top_builddir)/trackdb/libtrackdb.a		\
	@sound_if_obj@

INCLUDES =					\
	-I$(srcdir)/../trackdb			\
	-I$(srcdir)/../paranoia			\
	-I$(srcdir)/../dao

AM_CXXFLAGS = @GTKMM2_CFLAGS@ @LIBGUIMM2_CFLAGS@

man1_MANS = gcdmaster.man
EXTRA_DIST = $(man1_MANS) gcdmaster.desktop BUGS TASKS gcdmaster.png

desktopdir = $(datadir)/applications
desktop_DATA = gcdmaster.desktop

icondir = $(datadir)/pixmaps
icon_DATA = gcdmaster.png
