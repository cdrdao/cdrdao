#ident %W% %E% %Q%
###########################################################################
# Written 1999 by Andreas Mueller
###########################################################################
# Makefile for local dynamic configuration
###########################################################################
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

all: clobber
	$(LN) -s $(SRCROOT)/include libschily
	$(LN) -s $(SRCROOT)/libscg/scg scg
	$(LN) -s $(SRCROOT)/incs/$(OARCH)/xconfig.h xconfig.h
	$(LN) -s $(SRCROOT)/libs/$(OARCH)/libschily.a libschily.a
	$(LN) -s $(SRCROOT)/libs/$(OARCH)/libscg.a libscg.a 

clobber:
	$(RM_F) libschily scg xconfig.h libschily.a libscg.a

distclean: clobber

OTHERTARGETS=	clean install ibins depend rmdep \
		TAGS tags rmtarget relink

$(OTHERTARGETS):
	@echo "$@: nothing to make"

