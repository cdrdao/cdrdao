#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#Copyright 2001 Giuseppe Corbelli - cowo@lugbs.linux.it
#
# This package is inherited by both mp3handler and ogghandler. It simply provides
# uniform autoloaded access methods to common information fields.

package BaseInfo;
use strict;
use vars qw($AUTOLOAD);

sub new	{
	my ($proto) = shift;
	my ($class) = ref($proto) || $proto;
	my ($self) = {};
	
	bless ($self, $class);
	return $self;
}

# Autoloads methods with the same name as attribs. Standard args setup.
# 1 arg returns the actual value, 2 args to set the new one.
sub AUTOLOAD    {
        my ($self) = shift;
        return if $AUTOLOAD =~ /::DESTROY$/;
        $AUTOLOAD =~ s/^.*:://;
		($AUTOLOAD =~ /^\s*filename\s*$/i) ? $AUTOLOAD = 'Filename' : 1;
		($AUTOLOAD =~ /^\s*outfile\s*$/i) ? $AUTOLOAD = 'Outfile' : 1;
		($AUTOLOAD =~ /^\s*debug\s*$/i) ? $AUTOLOAD = 'debug' : 1;
		($AUTOLOAD =~ /^\s*decoder\s*$/i) ? $AUTOLOAD = 'decoder' : 1;
		($AUTOLOAD =~ /^\s*decoder_type\s*$/i) ? $AUTOLOAD = 'decoder_type' : 1;
		($AUTOLOAD =~ /^\s*artist\s*$/i) ? $AUTOLOAD = 'Artist' : 1;
		($AUTOLOAD =~ /^\s*album\s*$/i) ? $AUTOLOAD = 'Album' : 1;
		($AUTOLOAD =~ /^\s*title\s*$/i) ? $AUTOLOAD = 'Title' : 1; 
		($AUTOLOAD =~ /^\s*avgbr\s*$/i) ? $AUTOLOAD = 'Avgbr' : 1; 
		($AUTOLOAD =~ /^\s*year\s*$/i) ? $AUTOLOAD = 'Year' : 1; 
		($AUTOLOAD =~ /^\s*comment\s*$/i) ? $AUTOLOAD = 'Comment' : 1; 
		($AUTOLOAD =~ /^\s*genre\s*$/i) ? $AUTOLOAD = 'Genre' : 1; 
		($AUTOLOAD =~ /^\s*durationmm\s*$/i) ? $AUTOLOAD = 'durationMM' : 1; 
		($AUTOLOAD =~ /^\s*durationss\s*$/i) ? $AUTOLOAD = 'durationSS' : 1; 
		($AUTOLOAD =~ /^\s*duration\s*$/i) ? $AUTOLOAD = 'duration' : 1; 
		($AUTOLOAD =~ /^\s*channels\s*$/i) ? $AUTOLOAD = 'channels' : 1; 
		($AUTOLOAD =~ /^\s*frequency\s*$/i) ? $AUTOLOAD = 'frequency' : 1; 
		($AUTOLOAD =~ /^\s*debug\s*$/i) ? $AUTOLOAD = 'debug' : 1; 
		($AUTOLOAD =~ /^\s*error\s*$/i) ? $AUTOLOAD = 'Error' : 1;
		($AUTOLOAD =~ /^\s*decoder_type\s*$/i) ? $AUTOLOAD = 'decoder_type' : 1;
		($AUTOLOAD =~ /^\s*decoder\s*$/i) ? $AUTOLOAD = 'decoder' : 1;
        @_ ? $self->{$AUTOLOAD} = shift : return ($self->{$AUTOLOAD});
}

1;

