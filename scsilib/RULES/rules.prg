#ident "@(#)rules.prg	1.8 99/08/29 "
###########################################################################
# Written 1996 by J. Schilling
###########################################################################
#
# Generic rules for program names
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
#
# This file holds definitions that are common to all architectures.
# It should be included first and then partially overwritten,
# if the current architecture requires some changes.
#
###########################################################################

CLEAN_FILES=	core err

SHELL=		/bin/sh
LN=		/bin/ln
SYMLINK=	/bin/ln -s
RM=		/bin/rm
MV=		/bin/mv
LORDER=		lorder
TSORT=		tsort
CTAGS=		vctags
ETAGS=		etags
UMASK=		umask $(UMASK_VAL)
UMASK_DEF=	002

RM_FORCE=	-f
RM_RECURS=	-r
RM_RF=		$(RM_RECURS) $(RM_FORCE)

RM_F=		$(RM) $(RM_FORCE)

INSMODE_DEF=	755
INSUSR_DEF=	bin
INSGRP_DEF=	bin

_DEFUMASK=	$(_UNIQ)$(DEFUMASK)
__DEFUMASK=	$(_DEFUMASK:$(_UNIQ)=$(UMASK_DEF))
UMASK_VAL=	$(__DEFUMASK:$(_UNIQ)%=%)

_DEFINSMODE=	$(_UNIQ)$(DEFINSMODE)
__DEFINSMODE=	$(_DEFINSMODE:$(_UNIQ)=$(INSMODE_DEF))
INSMODE=	$(__DEFINSMODE:$(_UNIQ)%=%)

_DEFINSUSR=	$(_UNIQ)$(DEFINSUSR)
__DEFINSUSR=	$(_DEFINSUSR:$(_UNIQ)=$(INSUSR_DEF))
INSUSR=		$(__DEFINSUSR:$(_UNIQ)%=%)

_DEFINSGRP=	$(_UNIQ)$(DEFINSGRP)
__DEFINSGRP=	$(_DEFINSGRP:$(_UNIQ)=$(INSGRP_DEF))
INSGRP=		$(__DEFINSGRP:$(_UNIQ)%=%)


LD=		@echo "	==> LINKING   \"$@\""; ld
LOCALIZE=	@echo "	==> LOCALIZING \"$@\""; $(RM_F) $@; cp
INSTALL=	@echo "	==> INSTALLING \"$@\""; $(RM_F) $@; cp
CHMOD=		@echo "	==> SEETING PERMISSIONS ON \"$@\""; chmod
CHOWN=		@echo "	==> SETTING OWNER ON \"$@\""; chown
CHGRP=		@echo "	==> SETTING GROUP ON \"$@\""; chgrp
AR=		@echo "	==> ARCHIVING  \"$@\""; ar
#YACC=		@echo "	==> YACCING \"$@\""; yacc
#LEX=		@echo "	==> LEXING \"$@\""; lex
#AWK=		@echo "	==> AWKING \"$@\""; awk
MKDEP=		@echo "	==> MAKING DEPENDENCIES \"$@\""; makedepend
MKDEP_OUT=	-f -
MKDIR=		@echo "	==> MAKING DIRECTORY \"$@\""; $(UMASK); mkdir
MKDIR_SH=	@echo "	==> MAKING DIRECTORY \"$@\""; $(UMASK); sh $(SRCROOT)/conf/mkdir-sh
