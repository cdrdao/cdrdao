#ident "%W% %E% %Q%"
###########################################################################
#
# global definitions for SCO Systems
#
###########################################################################
#
# Compiler stuff
#
###########################################################################
DEFCCOM=	cc
#DEFCCOM=	gcc
###########################################################################
#
# If the next line is commented out, compilattion is done with max warn level
# If the next line is uncommented, compilattion is done with minimal warnings
#
###########################################################################
#CWARNOPTS=

DEFINCDIRS=	$(SRCROOT)/include
LDPATH=		
RUNPATH=	

###########################################################################
#
# Installation config stuff
#
###########################################################################
#INS_BASE=	/usr/local
#INS_KBASE=	/
INS_BASE=	/tmp/schily
INS_KBASE=	/tmp/schily/root
#
DEFUMASK=	002
#
DEFINSMODE=    755
DEFINSUSR=     bin
DEFINSGRP=     bin
