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
# This package is inherited by Mediahandler and provides mp3 info access
# and decoding. See the attached UML diagram for access methods.
# Everything should be done using methods.

package mp3handler;
require BaseInfo;
@ISA = qw(BaseInfo);
use MP3::Info; 
#use Data::Dumper; 
use strict; 
use File::Basename;
use vars qw($AUTOLOAD);

# 2 Args
# 1: self name
# 2: scalar containing filename
sub new {
	my ($proto) = shift; 
	my ($class) = ref($proto)||$proto;

	#Inherit from BaseInfo
	my $self = $class->SUPER::new();	
	#Checks on input file
	$self->Filename(shift);
	if (! $self->Filename)	{
		$self->Error("No input file defined.");
		return -1;
	}
	if (! -r $self->Filename)	{
		$self->Error("Input file is not readable");
		return -1;
	}
	if (-z $self->Filename)	{
		$self->Error("Input file has 0 size");
		return -1;
	}
	#Identify available mp3 decoder
	foreach my $dir (split (/\:/, $ENV{'PATH'}))	{
		if ( (-x "$dir/lame") && (-s "$dir/lame") )	{
			$self->decoder_type('lame');
			$self->decoder("$dir/lame");
			last;
		}
		if ( (-x "$dir/mpg321") && (-s "$dir/mpg321") )	{
			$self->decoder_type('mpg321');
			$self->decoder("$dir/mpg321");
			last;
		}
		if ( (-x "$dir/mpg123") && (-s "$dir/mpg123") )	{
			$self->decoder_type('mpg123');
			$self->decoder("$dir/mpg123");
			last;
		}
	}
	if ( !$self->decoder )	{
		$self->Error("Cannot find any of the supported mp3 decoders in \$PATH.");
		return -1;
	}
	
	my $ID3tagref = MP3::Info::get_mp3tag ($self->Filename);
	my $MP3info = MP3::Info::get_mp3info ($self->Filename);
	$self->Artist($ID3tagref->{"ARTIST"});
	$self->Album($ID3tagref->{"ALBUM"});
	$self->Title($ID3tagref->{"TITLE"});
	$self->Avgbr($MP3info->{"BITRATE"});
	$self->Year($ID3tagref->{"YEAR"});
	$self->Comment($ID3tagref->{"COMMENT"});
	$self->Genre($ID3tagref->{"GENRE"});
	$self->durationMM($MP3info->{"MM"});
	$self->durationSS($MP3info->{"SS"});
	$self->duration($MP3info->{"SECS"});
	$MP3info->{"STEREO"} ? $self->channels(2) : $self->channels(1);
	$self->frequency($MP3info->{"FREQUENCY"});
	$self->debug(1);
	bless ($self, $class);
	return $self;
}

sub type	{
	return "mp3";
}

#Decodes the file in mp3handler instance to the file specified as Outputfile in same instance
#Use system() and external decoder to do the work. Return value is external tool's one.
sub to_wav	{
	my ($self) = shift;
	my (@temp, $cmdline);
	if (! -w dirname($self->Filename) )	{
		$self->Error("Output directory is not writable");
		return -1;
	}
	printf ("\nDecoding file %s", $self->Filename) if ($self->debug);
	if (!$self->Outfile)	{
		$_ = $self->Filename; s/\.mp3/\.wav/i;
		$self->Outfile ($_);
		print ("\n\tNo outputfile defined. Used ".$self->Outfile."\n") if ($self->debug);
	} else	{
		printf (" to file %s\n", $self->Outfile) if ($self->debug);
	}
	if ( (-e $self->Outfile) && (-s $self->Outfile) && ($self->debug) )	{
		print $self->Outfile." exists, skipping mp3 decode\n" if ($self->debug);
		return 0;
	}
	if ($self->decoder_type =~ /lame/i)	{
		$cmdline = $self->decoder." --decode --mp3input -S ".quotemeta ($self->Filename)." ".quotemeta ($self->Outfile);
	}	elsif ($self->decoder_type =~ /mpg321/i)	{
		$cmdline = $self->decoder." -q -w ".quotemeta ($self->Outfile)." ".quotemeta ($self->Filename);
	}	else	{
		$cmdline = $self->decoder." -q -s ".quotemeta ($self->Filename)." \> ".quotemeta ($self->Outfile);
	}
	system ("".$cmdline);
	return $? >> 8;
}

#No args
#Returns 0
sub print_file_info	{
	my ($self) = @_;
	printf ("\nFilename : %s, type %s\n", $self->Filename, $self->type);
	printf ("\tArtist name: %s\t", $self->artist);
	printf ("Album name: %s\t", $self->album);
	printf ("Song title: %s\n", $self->title);
	printf ("\tYear: %s\t", $self->year);
	printf ("Song comment: %s\t", $self->comment);
	printf ("Genre: %s\n", $self->genre);
	printf ("\tDuration: %d min and %d sec\t", $self->durationMM, $self->durationSS);
	printf ("Average Bitrate: %d kb/s\n", $self->Avgbr);
	printf ("\t%d channel(s), %d HZ", $self->channels, ($self->frequency) * 1000);
	return 0;
}

1;
