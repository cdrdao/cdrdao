#ident %W% %E% %Q%
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		include
TARGET=		align.h
TARGETC=	align_test
CFILES=		align_test.c

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.inc
###########################################################################
