noinst_LIBRARIES = libdao.a

bin_PROGRAMS = cdrdao

cdrdao_SOURCES = \
	main.cc

libdao_a_SOURCES = \
	main.cc			\
	dao.cc			\
	port.cc			\
	data.cc			\
	CdrDriver.cc		\
	CDD2600Base.cc		\
	CDD2600.cc		\
	PlextorReader.cc	\
	GenericMMC.cc		\
	SubChannel.cc		\
	PQSubChannel16.cc	\
	PWSubChannel96.cc	\
	PQChannelEncoder.cc	\
	GenericMMCraw.cc	\
	PlextorReaderScan.cc	\
	RicohMP6200.cc		\
	TaiyoYuden.cc		\
	YamahaCDR10x.cc		\
	TeacCdr55.cc		\
	SonyCDU920.cc		\
	SonyCDU948.cc		\
	ToshibaReader.cc	\
	CdTextEncoder.cc	\
	Settings.cc		\
	CDD2600Base.h		\
	CDD2600.h		\
	cdda_interface.h	\
	CdrDriver.h		\
	CdTextEncoder.h		\
	dao.h			\
	data.h			\
	GenericMMC.h		\
	GenericMMCraw.h		\
	PlextorReader.h		\
	PlextorReaderScan.h	\
	port.h			\
	PQChannelEncoder.h	\
	PQSubChannel16.h	\
	PWSubChannel96.h	\
	remote.h		\
	RicohMP6200.h		\
	ScsiIf.h		\
	Settings.h		\
	sg_err.h		\
	SonyCDU920.h		\
	SonyCDU948.h		\
	SubChannel.h		\
	TaiyoYuden.h		\
	TeacCdr55.h		\
	ToshibaReader.h		\
	winaspi.h		\
	YamahaCDR10x.h

EXTRA_libdao_a_SOURCES = 	\
	dao-win32.cc		\
	decodeSense.cc		\
	sg_err.cc		\
	ScsiIf-lib.cc		\
	ScsiIf-common.cc	\
	ScsiIf-linux.cc		\
	ScsiIf-freebsd-cam.cc	\
	ScsiIf-netbsd.cc	\
	ScsiIf-irix.cc		\
	ScsiIf-osx.cc		\
	ScsiIf-nt.cc		\
	ScsiIf-win.cc

libdao_a_LIBADD =		\
	@scsilib_objs@

libdao_a_DEPENDENCIES =		\
	@scsilib_objs@

cdrdao_LDADD =						\
	./libdao.a					\
	$(top_builddir)/paranoia/libcdda_paranoia.a	\
	$(top_builddir)/trackdb/libtrackdb.a		\
	@thread_libs@					\
	@scsilib_libs@

if COND_MP3
cdrdao_LDADD += @MAD_LIBS@
endif

if COND_OGG
cdrdao_LDADD += @VORBISFILE_LIBS@
endif

cdrdao_LDADD += @AO_LIBS@

cdrdao_DEPENDENCIES = \
	$(top_builddir)/paranoia/libcdda_paranoia.a	\
	$(top_builddir)/trackdb/libtrackdb.a		\
	@scsilib_objs@					\
	libdao.a

drivertabledir = $(datadir)/cdrdao
drivertable_DATA = cdrdao.drivers

CDRDAO_DATA_DIR = $(datadir)/cdrdao
DRIVER_TABLE_FILE = $(CDRDAO_DATA_DIR)/drivers

install-data-hook:
	cd $(DESTDIR)$(datadir)/cdrdao && \
	mv cdrdao.drivers drivers

AM_CPPFLAGS =						\
	-I$(srcdir)/../trackdb				\
	-I$(srcdir)/../paranoia				\
	@scsilib_incl@

AM_CXXFLAGS = -DDRIVER_TABLE_FILE=\"$(DRIVER_TABLE_FILE)\"

man1_MANS = cdrdao.man
EXTRA_DIST = $(man1_MANS) cdrdao.drivers

