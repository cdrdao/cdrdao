#ident @(#)rules.inc	1.1 99/08/28 
###########################################################################
# Written 1999 by J. Schilling
###########################################################################
#
# Rules for creating include files (usually found in .../incs/machdir)
#
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
include		$(SRCROOT)/$(RULESDIR)/rules.obj
###########################################################################

PTARGET=	$(OINCSDIR)/$(TARGET)
PTARGETC=	$(ARCHDIR)/$(TARGETC)

all:		$(PTARGETC) $(PTARGET)

$(PTARGETC):	$(OFILES) $(SRCLIBS)
		$(LDCC) -o $@ $(POFILES) $(LDFLAGS) $(LDLIBS)

$(PTARGET):	$(OINCSDIR) $(PTARGETC)
		@echo "	==> GENERATING include file \"$@\""; $(PTARGETC) > $@

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.clr
include		$(SRCROOT)/$(RULESDIR)/rules.ins
include		$(SRCROOT)/$(RULESDIR)/rules.tag
include		$(SRCROOT)/$(RULESDIR)/rules.hlp
#include		$(SRCROOT)/$(RULESDIR)/rules.cnf
include		$(SRCROOT)/$(RULESDIR)/rules.dep
###########################################################################
