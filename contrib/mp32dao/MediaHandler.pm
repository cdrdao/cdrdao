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
#builds a list of handlers for all mediafiles in a directory

package MediaHandler;
require mp3handler;
require ogghandler;
require flachandler;
@ISA = qw(mp3handler ogghandler flachandler);
use strict; 
use vars qw ( @ext $AUTOLOAD );

#These are supported media files. Each time a new one is added
#its extension is put here and appropriate handler is instantiated in
#MediaHandler constructor.
@ext = ('.mp3', '.ogg', '.flac');

#2 args: self name and directory name
#returns blessed reference
#reads all filenames (extensions in @ext) in given directory,
#creates an handler for each one and puts istances in $self->{'LIST'}
sub new	{
	my ($proto, $dirname) = @_;
	my ($class) = ref($proto)||$proto;

	my ($filename, $dh, @filelist, $self);
	my ($total)=0;
	$self->{'DIR'} = $dirname;
	die ("Cannot open directory $dirname\n") if (!opendir (my ($DH), "$dirname"));
	die ("No write permissions for directory $dirname\n") if (!-w "$dirname");
	while ($filename = readdir ($DH))       {
		next if ( (!-r "$dirname/$filename" ) || (-z "$dirname/$filename") || (-d "$dirname/$filename") );
		chomp ($filename);
		foreach (@ext)	{
			if ($filename =~ m/\Q$_\E/i)	{
				push (@filelist, $filename);
				$total++;
			}
		}
	}
	$self->{'TOTAL'} = $total;
	@filelist=sort(@filelist);
	foreach $filename (@filelist)	{
		if ($filename =~ m/\.ogg/i)	{
			push (@{$self->{'LIST'}}, ogghandler->new("$dirname/$filename"));
		}
		if ($filename =~ m/\.wav/i)	{
		}
		if ($filename =~ m/\.mp3/i)	{
			push (@{$self->{'LIST'}}, mp3handler->new("$dirname/$filename"));
		}
		if ($filename =~ m/\.flac/i)	{
			push (@{$self->{'LIST'}}, flachandler->new("$dirname/$filename"));
		}
	}
	bless ($self, $class);
	return $self;
}

sub AUTOLOAD    {
        my ($self) = shift;
        return if $AUTOLOAD =~ /::DESTROY$/;
        $AUTOLOAD =~ s/^.*:://;
		($AUTOLOAD =~ /^\s*dir\s*$/i) ? $AUTOLOAD = 'DIR' : 1;
        ($AUTOLOAD =~ /^\s*list\s*$/i) ? $AUTOLOAD = 'LIST' : 1;
        ($AUTOLOAD =~ /^\s*total\s*$/i) ? $AUTOLOAD = 'TOTAL' : 1;
        return ($self->{$AUTOLOAD});
}

1;
