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
#simple package to decode and print info of mp3 files

package mp3handler;
use MP3::Info; 
#use Data::Dumper; 
use strict; 
use vars qw ($AUTOLOAD);

# 1 Arg, scalar containing filename
sub new {
	my ($proto, $Inputfile) = @_; 
	my ($class) = ref($proto)||$proto;
	my ($mp3module) = 1;
	my ($self) = {};

	#Identify available mp3 decoder
	foreach my $dir (split (/\:/, $ENV{'PATH'}))	{
		if ( (-x "$dir/lame") && (-s "$dir/lame") )	{
			$self->{'decoder_type'} = 'lame';
			$self->{'decoder'} = "$dir/lame";
			last;
		}
		if ( (-x "$dir/mpg321") && (-s "$dir/mpg321") )	{
			$self->{'decoder_type'} = 'mpg321';
			$self->{'decoder'} = "$dir/mpg321";
			last;
		}
		if ( (-x "$dir/mpg123") && (-s "$dir/mpg123") )	{
			$self->{'decoder_type'} = 'mpg123';
			$self->{'decoder'} = "$dir/mpg123";
			last;
		}
	}
	die ("Cannot find any of the supported mp3 decoders in \$PATH.") if ( !defined ($self->{'decoder'}));
	$self->{'Filename'} = $Inputfile;
	$self->{'Outfile'} = undef;
	$self->{'ID3tagref'} = MP3::Info::get_mp3tag ($Inputfile);
	$self->{'MP3info'} = MP3::Info::get_mp3info ($Inputfile);
	$self->{'debug'} = 0;
	bless ($self, $class);
	return $self;	
}

#No args, call set_output_file first
#returns 0 if all right, 1 if error
sub to_wav	{
	my ($self) = @_;
	my (@temp, $cmdline);
	printf ("\nDecoding file %s", $self->Filename);
	if (!$self->Outfile)	{
		$_ = $self->Filename; s/\.mp3/\.wav/i;
		$self->Outfile ($_);
		print ("\n\tNo outputfile defined. Used ".$self->Outfile."\n");
	} else	{
		printf (" to file %s\n", $self->Outfile);
	}
	if ( (-e $self->Outfile) && (-s $self->Outfile) )	{
		print $self->Outfile." exists, skipping mp3 decode\n";
		return 0;
	}
	if ($self->{'decoder_type'} =~ /lame/i)	{
		$cmdline = $self->{'decoder'}." --decode --mp3input -S ".quotemeta ($self->Filename)." ".quotemeta ($self->Outfile);
	}	elsif ($self->{'decoder_type'} =~ /mpg321/i)	{
		$cmdline = $self->{'decoder'}." -q -w ".quotemeta ($self->Outfile)." ".quotemeta ($self->Filename);
	}	else	{
		$cmdline = $self->{'decoder'}." -q -s ".quotemeta ($self->Filename)." \> ".quotemeta ($self->Outfile);
	}
	return system ("".$cmdline);
}
sub type	{
	return 'mp3';
}
sub MP3info	{
	my ($self) = shift;
	return $self->{'MP3info'};
}
sub ID3tagref	{
	my ($self) = shift;
	return $self->{'ID3tagref'};
}
sub AUTOLOAD    {
        my ($self) = shift;
        return if $AUTOLOAD =~ /::DESTROY$/;
        $AUTOLOAD =~ s/^.*:://;
		($AUTOLOAD =~ /^\s*filename\s*$/i) ? $AUTOLOAD = 'Filename' : 1;
		($AUTOLOAD =~ /^\s*outfile\s*$/i) ? $AUTOLOAD = 'Outfile' : 1;
		($AUTOLOAD =~ /^\s*debug\s*$/i) ? $AUTOLOAD = 'debug' : 1;
		($AUTOLOAD =~ /^\s*decoder\s*$/i) ? $AUTOLOAD = 'decoder' : 1;
		($AUTOLOAD =~ /^\s*decoder_type\s*$/i) ? $AUTOLOAD = 'decoder_type' : 1;
        @_ ? $self->{$AUTOLOAD} = shift : return ($self->{$AUTOLOAD});
}

sub bitrate	{
	my ($self) = shift;
	return $self->MP3info->{'BITRATE'};
}
sub frequency	{
	my ($self) = shift;
	return $self->MP3info->{'FREQUENCY'};
}
sub duration	{
	my ($self) = shift;
	return ( ($self->MP3info->{'MM'} * 60) + $self->MP3info->{'SS'} );
}
sub durationMM	{
	my ($self) = shift;
	return $self->MP3info->{'MM'};
}
sub durationSS	{
	my ($self) = shift;
	return $self->MP3info->{'SS'};
}
sub comment	{
	my ($self) = shift;
	@_ ? $self->ID3tagref->{'COMMENT'} = shift : return $self->ID3tagref->{'COMMENT'};
}
sub year	{
	my ($self) = shift;
	@_ ? $self->ID3tagref->{'YEAR'} = shift : return $self->ID3tagref->{'YEAR'};
}
sub artist	{
	my ($self) = shift;
	@_ ? $self->ID3tagref->{'ARTIST'} = shift : return $self->ID3tagref->{'ARTIST'};
}
sub title	{
	my ($self) = shift;
	@_ ? $self->ID3tagref->{'TITLE'} = shift : return $self->ID3tagref->{'TITLE'};
}
sub album	{
	my ($self) = shift;
	@_ ? $self->ID3tagref->{'ALBUM'} = shift : return $self->ID3tagref->{'ALBUM'};
}

#No args
#Returns 0
sub print_file_info	{
	my ($self) = @_;
	printf ("\nFilename : %s\n", $self->Filename);
	if (defined $self->ID3tagref) {	
		printf ("Artist name: %s\t", $self->artist);
		printf ("Album name: %s\t", $self->album);
		printf ("Song title: %s\n", $self->title);
		printf ("Year: %s\t", $self->year);
		printf ("Song comment: %s\t", $self->comment);
		printf ("Genre: %s\n", $self->genre);
	}
	printf ("Duration: %d min and %d sec\t", $self->durationMM, $self->durationSS);
	printf ("Average Bitrate: %d kb/s", $self->bitrate);
	if ($self->MP3info->{'VBR'})	{	print ("   --- VBR ---\n")	}
		else	{	print ("\n")};
	printf ("Layer: %d\t", $self->MP3info->{'LAYER'});
	if ($self->MP3info->{'STEREO'})	{	print ("2 channels, ")	}
		else	{print ("1 channel, ")};
	printf ("%d HZ\n", ($self->frequency) * 1000);
	return 0;
}

#No args
#Returns 0 if both MP3info and ID3tagref are present, 1 otherwise
sub tags_present	{
	my ($self) = @_;
	if ( (defined $self->MP3info) && (defined $self->ID3tagref) )	{
		return 1;
	} else { 
		return 0; 
	}
}

1;
