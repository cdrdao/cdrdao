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
#simple package to decode and print info of ogg files

package ogghandler;
use strict;
#use Data::Dumper;
use vars qw( $AUTOLOAD  );
use Ogg::Vorbis;

# 1 Arg, scalar containing filename
sub new {
	my ($proto) = shift; my ($class) = ref($proto)||$proto;
	my ($Inputfile) = shift; my ($self);
	
	#Identify available ogg decoder
	foreach my $dir (split (/\:/, $ENV{'PATH'}))	{
		if ( (-x "$dir/ogg123") && (-s "$dir/ogg123") )	{
			$self->{'decoder_type'} = 'ogg123';
			$self->{'decoder'} = "$dir/ogg123";
			last;
		}
	}
	die ("Cannot find any of the supported ogg decoders in \$PATH.") if ( !defined ($self->{'decoder'}));
	open (my ($fh), "< $Inputfile");
	$self->{'Filename'} = $Inputfile;
	$self->{'handle'} = Ogg::Vorbis->new;
	$self->{'handle'}->open ($fh);
	$self->{'Outfile'} = undef;
	$self->{'comments'} = $self->{'handle'}->comment;
	$self->{'ogginfo'} = $self->{'handle'}->info;
	$self->{'bitrate'} = $self->{'handle'}->bitrate;
	$self->{'streams'} = $self->{'handle'}->streams;
	$self->{'serialnumber'} = $self->{'handle'}->serialnumber;
	$self->{'raw_total'} = $self->{'handle'}->raw_total;
	$self->{'pcm_total'} = $self->{'handle'}->pcm_total;
	$self->{'time_total'} = $self->{'handle'}->time_total;
	$self->{'debug'} = 0;
	$self->{'external_oggdecoder'} = undef;
	bless ($self, $class);
	return $self;
}

sub print_file_info	{
	my ($self) = @_;
	printf ("\nFilename : %s\n", $self->Filename);
	printf ("Artist name: %s\t", $self->comments->{'artist'});
	printf ("Album name: %s\t", $self->comments->{'album'});
	printf ("Song title: %s\n", $self->comments->{'title'});
	printf ("Year: %s\t", $self->comments->{'date'});
	printf ("Song comment: %s\t", $self->comment);
	printf ("Genre: %s\n", $self->comments->{'genre'});
	printf ("Duration: %d min and %d sec\t", $self->time_total/60, $self->time_total%60);
	printf ("Average Bitrate: %d Kb/s\n", $self->bitrate/1024);
	printf ("Version: %d\t", $self->ogginfo->version);
	printf ("%d channels, %d HZ\n", $self->ogginfo->channels, $self->ogginfo->rate);
	return 0;
}

sub to_wav	{
	my ($self) = shift;
	my ($cmdline);
	printf ("\nDecoding file %s", $self->Filename);
	if (!$self->Outfile)	{
		$_ = $self->Filename; s/\.ogg/\.wav/i;
		$self->Outfile ($_);
		print ("\n\tNo outputfile defined. Used ".$self->Outfile."\n");
	} else	{
		printf (" to file %s\n", $self->Outfile);
	}
	if ( (-e $self->Outfile) && (-s $self->Outfile) )	{
		print $self->Outfile." exists, skipping ogg decode\n";
		return 0;
	}
	if ($self->{'decoder_type'} =~ /ogg123/i)	{
		$cmdline = $self->{'decoder'}." -q -d wav -o file:".quotemeta ($self->Outfile)." ".quotemeta ($self->Filename);
	}
	return system ($cmdline);
}
sub handle	{
	my ($self) = shift;
	return $self->{'handle'};
}
sub type	{
	return 'ogg';
}
sub AUTOLOAD    {
        my ($self) = shift;
        return if $AUTOLOAD =~ /::DESTROY$/;
        $AUTOLOAD =~ s/^.*:://;
        $AUTOLOAD =~ s/^\s*\Ufilename\E\s*$/Filename/;
        $AUTOLOAD =~ s/^\s*\Uoutfile\E\s*$/Outfile/;
        $AUTOLOAD =~ s/^\s*\Ucomments\E\s*$/comments/;
        $AUTOLOAD =~ s/^\s*\Uogginfo\E\s*$/ogginfo/;
        $AUTOLOAD =~ s/^\s*\Ubitrate\E\s*$/bitrate/;
        $AUTOLOAD =~ s/^\s*\Ustreams\E\s*$/streams/;
        $AUTOLOAD =~ s/^\s*\Userialnumber\E\s*$/serialnumber/;
        $AUTOLOAD =~ s/^\s*\Uraw_total\E\s*$/raw_total/;
        $AUTOLOAD =~ s/^\s*\Upcm_total\E\s*$/pcm_total/;
        $AUTOLOAD =~ s/^\s*\Utime_total\E\s*$/time_total/;
        $AUTOLOAD =~ s/^\s*\Udebug\E\s*$/debug/;
        $AUTOLOAD =~ s/^\s*\Uexternal_oggdecoder\E\s*$/external_oggedecoder/;
        @_ ? $self->{$AUTOLOAD} = shift : return ($self->{$AUTOLOAD});
}

sub duration	{
	my ($self) = shift;
	return ($self->time_total);
}
sub durationMM	{
	my ($self) = shift;
	return $self->time_total/60;
}
sub durationSS	{
	my ($self) = shift;
	return $self->time_total%60;
}
sub year	{
	my ($self) = shift;
	return $self->comments->{'date'};
}
sub artist	{
	my ($self) = shift;
	return $self->comments->{'artist'};
}
sub title	{
	my ($self) = shift;
	return $self->comments->{'title'};
}
sub album	{
	my ($self) = shift;
	return $self->comments->{'album'};
}

1;
