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

#Copyright 2001 Giuseppe Corbelli - cowo@lugbs.linux.it
#simple package to decode and print info of mp3 files

package mp3handler;
use strict;

# 1 Arg, scalar containing filename
sub new {
	my ($internal_mp3decoder) = 1;
	my ($mp3module) = 1;
	eval "use mp3dec";
	if ($@)	{
		my ($internal_mp3decoder) = 0;		
	} else	{
		use mp3dec;
	}
	eval "use MP3::Info";
	if ($@)	{
		my ($mp3module) = 0;
	}	else	{
		use MP3::Info;
	}

	my ($class) = shift;
	my ($Inputfile) = shift;
	
	if ( ! -r $Inputfile)	{
		print STDERR ("Cannot read $Inputfile.\n");
		return undef;
	}
	if ($mp3module)	{
		my ($mp3tag) = MP3::Info::get_mp3tag ($Inputfile);
		my ($mp3info) = MP3::Info::get_mp3info ($Inputfile);
		return bless	{'Filename'=>$Inputfile,
						'Outfile'=>undef,
						'ID3tagref'=>$mp3tag,
						'MP3info'=>$mp3info,
						'debug'=>0,
						'internal_mp3decoder'=>$internal_mp3decoder,
						'external_mp3decoder'=>undef,
						'mp3module'=>$mp3module
						}, $class;
	}	else	{
		return bless	{'Filename'=>$Inputfile,
						'Outfile'=>undef,
						'internal_mp3decoder'=>$internal_mp3decoder,
						'mp3module'=>$mp3module,
						'external_mp3decoder'=>undef,
						'debug'=>0
						}, $class;
	}	
}

#No args, call set_output_file first
#returns 0 if all right, 1 if error
sub to_wav	{
	my ($self) = shift;
	my (@temp, $Outfile, $qinfile);
	if ($self->{'debug'})	{
		print STDERR ("Decoding mp3 $self->{'Filename'} to wav $self->{'Outfile'}\n");
	}
	if ( ! ($self->{'internal_mp3decoder'} || $self->{'external_mp3decoder'}) )	{
		print STDERR ("No internal decoder and no external program defined. Cannot decode file.\n");
		return 1;
	}
	if ( ! defined ($self->{'Outfile'}))	{
		@temp = split (/\.mp3/, $self->{'Filename'});
		$Outfile = shift (@temp);
		$Outfile.='.wav';
		$self->set_output_file($Outfile);
	}	
	if ( -e $self->{'Outfile'})	{
		print "$self->{'Outfile'} exists, skipping mp3 decode\n\n";
		return 0;
	}
	if ($self->{'internal_mp3decoder'})	{		
		return (mp3dec::decode_mp3 ($self->{'Filename'}, $self->{'Outfile'} ) );
	}	elsif ( $self->{'external_mp3decoder'} ) {
		$qinfile = quotemeta ($self->{'Filename'});
		@temp = split (/.mp3/, $qinfile);
		$Outfile = shift (@temp);
		$Outfile.='.wav';		
		return system "$self->{'external_mp3decoder'} $qinfile $self->{'Outfile'}";
#		print "$self->{'external_mp3decoder'} $self->{'Filename'} $self->{'Outfile'}";
	}
}

#One arg, filename to write to
#Returns 0 f OK, 1 otherwise
sub set_output_file	{
	my ($outfile) = pop;
	my ($self) = shift;
	if (defined $outfile) {
		if ($self->{'debug'})	{
			print STDERR ("Success!\n");
		}
		$self->{'Outfile'}=$outfile;
		return 0;
	}
	return 1;
}
#One arg, filename to read from
#Returns 0 f OK, 1 otherwise
sub set_input_file	{
	my ($infile) = pop;
	my ($self) = shift;
	if (defined $infile) {
		if ($self->{'debug'})	{
			print STDERR ("Success!\n");
		}
		$self->{'Filename'}=$infile;
		return 0;
	}
	return 1;
}


#One arg: string containing filename and options
#Returns 0 f OK, 1 otherwise
sub set_external_mp3decoder	{
	my ($external_mp3decoder) = pop;
	my ($self) = shift;
	$self->{'external_mp3decoder'}=$external_mp3decoder;
	return 0;
}

#One arg, scalar 1 if wanna debug, 0 otherwise.
#Returns object debug value
sub set_debug	{
	my ($self) = shift;
	my ($debug) = pop;
	if (defined $debug)	{
		$self->{'debug'} = $debug;
	};
	return $self->{'debug'};
}

#No args
#Returns 0
sub print_file_info	{
	my ($self) = shift;
	unless ($self->{'mp3module'})	{
		print "No MP3::Info module available. No support for ID3 tags.\n";
		return;
	}
	if (defined $self->{'ID3tagref'}) {	
		print ("Artist name: ");
		if (defined $self->{'ID3tagref'}->{'ARTIST'})	{
			 print ("$self->{'ID3tagref'}->{'ARTIST'}\t");
		}
		print ("Album name: ");
		if (defined $self->{'ID3tagref'}->{'ALBUM'})	{
			print ("$self->{'ID3tagref'}->{'ALBUM'}\n");
		}
		print ("Song title: ");
		if (defined $self->{'ID3tagref'}->{'TITLE'})	{
			print ("$self->{'ID3tagref'}->{'TITLE'}\n");
		}
		print ("Year: ");
		if (defined $self->{'ID3tagref'}->{'YEAR'})	{
			print ("$self->{'ID3tagref'}->{'YEAR'}\t");
		}
		print ("Song comment: ");
		if (defined $self->{'ID3tagref'}->{'COMMENT'})	{
			print ("$self->{'ID3tagref'}->{'COMMENT'}\n");
		}
		print ("Genre: ");
		if (defined $self->{'ID3tagref'}->{'GENRE'})	{
			print ("$self->{'ID3tagref'}->{'GENRE'}\n");
		}
	}
	if (defined $self->{'MP3info'}) 	{
		print ("Duration: $self->{'MP3info'}->{'MM'} min and $self->{'MP3info'}->{'SS'} sec\t");
		print ("Bitrate: $self->{'MP3info'}->{'BITRATE'} kb/s");
		if ($self->{'MP3info'}->{'VBR'})	{	print ("   --- VBR ---\n")	}
			else	{	print ("\n")};
		print ("Layer: $self->{'MP3info'}->{'LAYER'}\t");
		if ($self->{'MP3info'}->{'STEREO'})	{	print ("2 channels, ")	}
			else	{print ("1 channel, ")};
		print ("$self->{'MP3info'}->{'FREQUENCY'} KHZ\n");
	}
	return 0;
}

#No args
#Returns 0 if both MP3info and ID3tagref are present, 1 otherwise
sub tags_present	{
	my ($self) = shift;
	unless ($self->{'mp3module'})	{
		print "No MP3::Info module available. No support for ID3 tags.\n";
		return;
	}
	if ( (defined $self->{'MP3info'}) && (defined $self->{'ID3tagref'}) )	{
		return 0;
	} else { 
		return 1; 
	}
}

#2 args: directory name, reference of array
#returns number of mp3files found or -1 if error
#reads all filenames (*.mp3) in given directory, sorts them
#and puts in array.
sub read_mp3list	{
	my ($self) = shift;
	my ($dirname) = shift;
	my ($arrayref) = pop;

	my ($filename);
	my $total=0;

	if ( ! opendir (DIR, $dirname) )	{
		print STDERR ("Cannot open directory $dirname\n");
		return -1;
	}

	#Reads all mp3 filenames in @filelist
	while ($filename = readdir (DIR))       {
		chomp ($filename);
		if ( ! -r $filename )	{ 
			print STDERR ("Cannot read file $filename\n");
			return -1;
		}

		if ($filename =~ /.*\.mp3/i)	{		
			push (@$arrayref, $filename);
			$total++;
		}
	}
	@$arrayref=sort(@$arrayref);
	return $total;
}
1;
