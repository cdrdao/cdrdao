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

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#Copyright 2001 Giuseppe Corbelli - cowo@lugbs.linux.it
# Please look at mp3handler.pm for comments. This is almost the same,
# only difference is Ogg::Vorbis backend

package ogghandler;
require BaseInfo;
@ISA = qw(BaseInfo);
use strict;
#use Data::Dumper;
use vars qw($AUTOLOAD);
use Ogg::Vorbis;
use File::Basename;

# 1 Arg, scalar containing filename
sub new {
	my ($proto) = shift; 
	my ($class) = ref($proto)||$proto;
	my $self = $class->SUPER::new();

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
	
	#Identify available ogg decoder
	foreach my $dir (split (/\:/, $ENV{'PATH'}))	{
		if ( (-x "$dir/ogg123") && (-s "$dir/ogg123") )	{
			$self->decoder_type('ogg123');
			$self->decoder("$dir/ogg123");
			last;
		}
	}
	if ( !$self->decoder )	{
		$self->Error("Cannot find any of the supported ogg decoders in \$PATH.");
		return -1;
	}

	open (my ($fh), "<".$self->Filename);
	my $handle = Ogg::Vorbis->new;
	$handle->open ($fh);

	#First try the lowercase version, then the first char uppercase, last the all-uppercase
	foreach my $n (qw(artist album title date comment genre))	{
		if ($handle->comment->{$n})	{
			$self->$n ($handle->comment->{"$n"});
		}	elsif	($handle->comment->{"\u$n"})	{
			$self->$n ($handle->comment->{"\u$n"});
		}	elsif	($handle->comment->{"\U$n"})	{
			$self->$n ($handle->comment->{"\U$n"});
		}	else	{
			$self->$n ('');
		}
	}
	$self->Avgbr($handle->bitrate);
	$self->durationMM($handle->time_total/60);
	$self->durationSS($handle->time_total%60);
	$self->duration($handle->time_total);
	$self->channels($handle->info->channels);
	$self->frequency($handle->info->rate);
	$self->debug(1);
	bless ($self, $class);
	return $self;
}

sub type	{
	return 'ogg';
}

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
	printf ("Average Bitrate: %d kb/s\n", $self->Avgbr/1024);
	printf ("\t%d channel(s), %d HZ", $self->channels, $self->frequency);
	return 0;
}

sub to_wav	{
	my ($self) = shift;
	my (@temp, $cmdline);
	if (! -w dirname($self->Filename) )	{
		$self->Error("Output directory is not writable");
		return -1;
	}
	printf ("\nDecoding file %s", $self->Filename) if ($self->debug);
	if (!$self->Outfile)	{
		$_ = $self->Filename; s/\.ogg/\.wav/i;
		$self->Outfile ($_);
		print ("\n\tNo outputfile defined. Used ".$self->Outfile."\n") if ($self->debug);
	} else	{
		printf (" to file %s\n", $self->Outfile) if ($self->debug);
	}
	if ( (-e $self->Outfile) && (-s $self->Outfile) && ($self->debug) )	{
		print $self->Outfile." exists, skipping mp3 decode\n" if ($self->debug);
		return 0;
	}
	if ($self->decoder_type =~ /ogg123/i)	{
		$cmdline = $self->decoder." -q -d wav -f ".quotemeta ($self->Outfile)." ".quotemeta ($self->Filename);
	}
        #print $cmdline;
	#my ($ret)=(system ("".$cmdline))/256;
        #print "$ret";
	#return $? >> 8;
        return (system ("".$cmdline))/256;
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
		$inffilename =~ s/ogg/inf/i;
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
