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
# Copyright 2003 Giuseppe Corbelli - cowo@lugbs.linux.it
#
# This package is inherited by Mediahandler and provides flac info access
# and decoding. See the attached UML diagram for access methods.
# Everything should be done using methods.

package flachandler;
require BaseInfo;
@ISA = qw(BaseInfo);
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
	#Identify available flac decoder
	foreach my $dir (split (/\:/, $ENV{'PATH'}))	{
		if ( (-x "$dir/flac") && (-s "$dir/flac") )	{
			$self->decoder_type('flac');
			$self->decoder("$dir/flac");
			last;
		}
	}
	if ( !$self->decoder )	{
		$self->Error("Cannot find any of the supported flac decoders in \$PATH.");
		return -1;
	}

	#Identify available metaflac decoder
	foreach my $dir (split (/\:/, $ENV{'PATH'}))	{
		if ( (-x "$dir/metaflac") && (-s "$dir/metaflac") )	{
			$self->metaflac("$dir/metaflac");
			last;
		}
	}
	if ( !$self->metaflac )	{
		$self->Error("Cannot find metaflac program in \$PATH.");
		return -1;
	}
	
	#Use a pipe to get metaflac data
	my $cmdline = sprintf ("%s --list --block-type=STREAMINFO,VORBIS_COMMENT %s |", $self->metaflac, $self->Filename);
	my $pid = open (METAFLAC, $cmdline);
	if ( (!$pid) or ($pid < 0))	{
		$self->Error ("Cannot get info from metaflac program.");
		return -1;
	}
	while (<METAFLAC>)	{
		if (m/^\s*comment\[\d+\]\:\s*(\w+)\=(.+)$/)	{
			my $tag = $1;
			my $value = $2;
			foreach my $n (qw(artist album title year comment genre))	{
				if ($n =~ /\Q$tag\E/i)	{
					$self->$n ($value);
					printf ("%s matches %s value %s\n", $n, $tag, $value);
				}
			}
		}	elsif (m/^\s*sample\_rate\:\s*(\d+).*$/)	{
			my $freq = $1;
			$self->frequency($freq);
#			printf ("Frequency %d\n", $self->frequency);
		}	elsif (m/^\s*channels\:\s*(\d).*$/)	{
			my $chan = $1;
			$self->channels($chan);
#			printf ("Channels %d\n", $self->channels);
		}	elsif (m/^\s*total\ssamples\:\s*(\d+).*$/)	{
			my $samples = $1;
			my $secs = $samples / $self->frequency;
			$self->mm($secs / 60);
			$self->ss($secs % 60);
			$self->secs($secs);
#			printf ("MM %d SS %d secs %d\n", $self->mm, $self->ss, $self->secs);
		}
	}
	close (METAFLAC) or warn $! ? "Error closing metaflac pipe: $!"
                                  : "Exit status $? from metaflac";
	$self->debug(1);
	bless ($self, $class);
	return $self;
}

sub type	{
	return "flac";
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
		$_ = $self->Filename; s/\.flac/\.wav/i;
		$self->Outfile ($_);
		print ("\n\tNo outputfile defined. Used ".$self->Outfile."\n") if ($self->debug);
	} else	{
		printf (" to file %s\n", $self->Outfile) if ($self->debug);
	}
	if ( (-e $self->Outfile) && (-s $self->Outfile) && ($self->debug) )	{
		print $self->Outfile." exists, skipping flac decode\n" if ($self->debug);
		return 0;
	}
	if ($self->decoder_type =~ /flac/i)	{
		$cmdline = sprintf ("%s -d -o %s %s", $self->decoder, quotemeta ($self->Outfile), quotemeta ($self->Filename));
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

#Arguments
#       $trackno: Track number
# Optional: $filename: filename of .inf file
sub write_inf   {
	my ($self) = shift;
	my ($trackno) = shift;
	my ($inffilename);
	if (@_)	{
		$inffilename = shift;
	}	else	{
	    $inffilename = $self->Filename;
		$inffilename =~ s/flac/inf/i;
	}
    my ($inffilehandle);
    open ($inffilehandle, ">$inffilename");
    if (!$inffilehandle)        {last};
    my (@tl) = localtime (time ());
    $_ = sprintf ("#Created by mp32dao.pl on %02d/%02d/%d %02d:%02d:%02d\n", 
        $tl[3], $tl[4]+1, $tl[5]+1900, $tl[2], $tl[1], $tl[0]);
    print $inffilehandle $_;
    print $inffilehandle "#Source file is ".$self->Filename."\n";
    print $inffilehandle "#Report bugs to Giuseppe \"Cowo\" Corbelli <cowo\@lugbs.linux.it>\n\n";
    print $inffilehandle "Performer=\t'".$self->Artist."'\n";
    print $inffilehandle "Tracktitle=\t'".$self->Title."'\n";
    print $inffilehandle "Albumtitle=\t'".$self->Album."'\n";
    print $inffilehandle "Tracknumber=\t".($trackno)."\n";
    close ($inffilehandle);
}

1;
