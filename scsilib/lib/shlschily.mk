#ident %W% %E% %Q%
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
CCOM=		cc
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

SUBARCHDIR=	/pic
.SEARCHLIST:	. $(ARCHDIR) stdio $(ARCHDIR)
VPATH=		.:stdio:$(ARCHDIR)
INSDIR=		lib
TARGETLIB=	schily
CPPOPTS +=	-Istdio -DNO_SCANSTACK
include		Targets
LIBS=		

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.shl
###########################################################################
# Gmake has a bug with the VPATH= option. Some of the macros are
# not correctly expanded. I had to remove all occurrences of
# $@ $* and $^ on some places for this reason.
###########################################################################

.INIT:
	@echo "	==> The folloging messages may occur:"
	@echo "	==> cannot find include file: <align.h>"
	@echo "	==> cannot find include file: <avoffset.h>"
	@echo "	==> this is not an error, these files are made during the build."

cmpbytes.o fillbytes.o movebytes.o: align.h
$(ARCHDIR)/cmpbytes.o $(ARCHDIR)/fillbytes.o $(ARCHDIR)/movebytes.o: align.h

align_test.o:	align_test.c
		$(CC) -c $(CPPFLAGS) -D__OPRINTF__ -o $(ARCHDIR)/align_test.o align_test.c

align_test:	align_test.o
		$(LDCC) -o $(ARCHDIR)/align_test $(ARCHDIR)/align_test.o

align.h:	align_test
		$(ARCHDIR)/align_test > $(ARCHDIR)/align.h

getav0.o:	avoffset.h
$(ARCHDIR)/getav0.o:	avoffset.h
raisecond.o:	avoffset.h
$(ARCHDIR)/raisecond.o:	avoffset.h
saveargs.o:	avoffset.h
$(ARCHDIR)/saveargs.o:	avoffset.h

avoffset.o:	avoffset.c
		$(CC) -c $(CPPFLAGS) -D__OPRINTF__ -o $(ARCHDIR)/avoffset.o avoffset.c

avoffset:	avoffset.o getfp.o
		$(LDCC) -o $(ARCHDIR)/avoffset $(ARCHDIR)/avoffset.o $(ARCHDIR)/getfp.o

avoffset.h:	avoffset
		-$(ARCHDIR)/avoffset > $(ARCHDIR)/avoffset.h

###########################################################################
# The next line is needed for old buggy gmake releases before release 3.74.
# Sources before gmake 3.75 now are no longer available on ftp servers,
# the GNU people seem to know why ;-)
# Only one line is needed to have a rule for creating the OBJ dir.
# Do not insert more then one line with $(ARCHDIR) on the right side
# gmake would go into infinite loops otherwise.
###########################################################################
$(ARCHDIRX)align_test$(DEP_SUFFIX):	$(ARCHDIR)

include		$(ARCHDIRX)avoffset$(DEP_SUFFIX)
include		$(ARCHDIRX)align_test$(DEP_SUFFIX)

CLEAN_FILEX=	$(ARCHDIR)/align_test.o $(ARCHDIR)/align_test
CLEAN_FILEX +=	$(ARCHDIR)/avoffset.o $(ARCHDIR)/avoffset

CLOBBER_FILEX=	$(ARCHDIR)/align_test.d $(ARCHDIR)/align_test.dep \
		$(ARCHDIR)/align.h
CLOBBER_FILEX += $(ARCHDIR)/avoffset.d $(ARCHDIR)/avoffset.dep \
		$(ARCHDIR)/avoffset.h
