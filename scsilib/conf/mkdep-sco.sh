#!/bin/sh
#ident "%W% %E% %Q%"
###########################################################################
# Copyright 1999 by J. Schilling
###########################################################################
#
# Create dependency list with SCO's cc
#
###########################################################################
#
# This script will probably not work correctly with a list of C-files
# but as we don't need it with 'smake' or 'gmake' it seems to be sufficient.
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
FILES=

for i in "$@"; do

	case "$i" in

	-*)	# ignore options
		;;
	*.c)	if [ ! -z "$FILES" ]; then
			FILES="$FILES "
		fi
		FILES="$FILES$i"
		;;
	esac
done

OFILES=`echo "$FILES" | sed 's;\(.*\).c;\1.o;'`

cc -H -E 2>&1 > /dev/null "$@" | grep -hv '^"' | sed -e "s;^;$OFILES: ;"
