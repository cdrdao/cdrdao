#!/usr/bin/perl -w -I /home/cowo/compile/cdrdao-1.1.4/contrib/mp32dao
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

#Modified version of the original mp32dao distributed with cdrdao. 
#Now with cdtext, ID3 tags and internal mp3 decoding if available.
#Giuseppe Corbelli <cowo@lugbs.linux.it>
#Original code by Joe Steward <mp32dao@httptech.com>

push (@INC, '.');
use MP3::Info;
use strict;
use mp3handler;
use Audio::Wav;
use Audio::Tools::Time;

my $wav = new Audio::Wav;
my ($count, $totalsamples, $divided, $rounded, $remainder, $artist, $album, $filename, $total, $song, $fh);
my $cdtext = 1;
#####################################
#----------LOOK HERE!!!--------------
#If you haven't mp3dec module installed
#you must use an external program.
#Put here programname + options
#Example:
my $external_mp3decoder = 'lame --decode --mp3input';
#my $external_mp3decoder = undef;
#####################################
my $samplestart = 0;
my (@tracklist, @filelist);

eval "use mp3dec";
if ($@)	{
	unless ($external_mp3decoder)	{
		die ("No external decoder defined! Edit mp32dao.pl at line 39!\n");
	}
}

print "-------- MP3-to-DAO Helper Script ---------\n";
print "Usage: mp32dao.pl [tocfile]\n";

my $tocfile = $ARGV[0];
unless ($tocfile) {
    $tocfile = "cd.toc";
    print "No toc file specified, using default \"cd.toc\"\n";
}

$total = mp3handler->read_mp3list ('./', \@filelist);
if ($total <= 0)	{ die "No mp3 files found or error occurred!\n" };

$count=1;
#Creates array of mp3handlers, sets artist and album.
foreach $filename (@filelist)	{
	$song=mp3handler->new($filename);
	#Tracklist starts at 1
	$tracklist[$count]=$song;
	if ($song->tags_present())	{	$cdtext = 0	};
		
	#Sets artist and album name
	if ($cdtext)	{
		if ($song->{'ID3tagref'}->{'ARTIST'})	{
			#If artist is still not defined define as in first song tag
			if (!$artist)	{ $artist=$song->{'ID3tagref'}->{'ARTIST'} } 
			#If different artists mark as VVAA
			if ($artist ne $song->{'ID3tagref'}->{'ARTIST'})	{	$artist = "Various Artists"};
		}
		if ( $song->{'ID3tagref'}->{'ALBUM'})	{
			if (!$album)	{ $album = $song->{'ID3tagref'}->{'ALBUM'} }
			if ($album ne $song->{'ID3tagref'}->{'ALBUM'})	{	$album = "MP3 Compilation"};
		}
	}
	$count++;
}

open ($fh, ">$tocfile") || die "Couldn't open $tocfile for write!\n";
tocfile_header($fh, $cdtext);

#Set outputfiles and decode to wav.
$count=1;
print STDOUT "\nDecoding mp3 files to wav\n\n";
while ($count <= $total) {
	$tracklist[$count]->print_file_info ();
	print STDOUT "\n";
	$tracklist[$count]->to_wav();
	$count++;
}

$count=1;
print STDOUT "\nAnalyzing wav files and creating toc\n\n";
while ($count <= $total)	{
    my $read = $wav->read( $tracklist[$count]->{'Outfile'} );
    my $audio_bytes = $read -> length();
    my $time = new Audio::Tools::Time 44100, 16, 2;
    my $sample = $time -> bytes_to_samples( $audio_bytes );	

	tocfile_entry ($fh, $cdtext, $tracklist[$count], $samplestart);
	
    $totalsamples = $sample - $samplestart;
    $divided = $totalsamples / 588; 
    $rounded = int($divided) * 588;
    $remainder = $totalsamples - $rounded;
    print "\tNeed to pad up $remainder samples for track $count\n";
    if (($remainder != 0) && ($count ne $total)) { 
		my $nextfile = $tracklist[($count+1)]->{'Outfile'};
		print $fh "FILE \"$nextfile\" ";
		print $fh "0 $remainder\n\n";
		$samplestart = $remainder;
	}	
	$count++;
}

print "\nFinished writing TOC file. You may now burn the CD using\n\n\tcdrdao write $tocfile\n\n";
close ($fh);
exit;

sub seconds_to_cd_time {
    my $seconds = shift;
    my ($minutes, $frames);
    my $minute_increment = int($seconds / 60);
    if ($minute_increment) {
        $seconds = $seconds - ($minute_increment * 60);
        $minutes = $minutes + $minute_increment;
        }
    my $decimal = $seconds - int($seconds);
    if ($decimal) {
        $frames = int($decimal * 75);
        }
    return sprintf ("%02d:%02d:%02d", $minutes, $seconds, $frames);
}

# 2 arg, FileHandle opened for writing, scalar==1 if cdtext ON
# returns nothing
sub tocfile_header	{
	my ($fh) = shift;
	my ($cdtext) = shift;
	print $fh "CD_DA\n";
	if ($cdtext)	{
		print $fh "CD_TEXT \{\n";
			print $fh "\tLANGUAGE_MAP \{\n";
 		   print $fh "\t\t0 : EN\n";
			print $fh "\t\}\n\n";
		print $fh "\tLANGUAGE 0 \{\n";
			print $fh "\t\tTITLE \"$album\"\n";
			print $fh "\t\tPERFORMER \"$artist\"\n";
			print $fh "\t\tDISC_ID \"XY12345\"\n";
			print $fh "\t\tUPC_EAN \"\"\n";
			print $fh "\t\}\n";
		print $fh "\}\n";
	}
}

# 4 arg; 
#	Filehandle opened for writing
#	scalar == 1 if cdtext ON
#	reference to song structure
#	samplestart: scalar containing sample offset
sub tocfile_entry	{
	my ($fh) = shift;
	my ($cdtext) = shift;
	my ($song) = shift;
	my ($samplestart) = shift;
	
    print $fh "// Track $count\n";
    print $fh "TRACK AUDIO\n";
    print $fh "TWO_CHANNEL_AUDIO\n";
	if ($cdtext)	{
    	print $fh "CD_TEXT {\n";
    	print $fh "\tLANGUAGE 0 {\n";
    	print $fh "\t\tTITLE \"".$song->{'ID3tagref'}->{'TITLE'}."\"\n";
    	print $fh "\t\tPERFORMER \"".$song->{'ID3tagref'}->{'ARTIST'}."\"\n";
    	print $fh "\t\tISRC \"US-XX1-98-01234\"\n";
    	print $fh "\t}\n}\n";
    }
    print $fh "FILE \"$song->{'Outfile'}\" ";
	print $fh "$samplestart";
    print $fh "\n";
}
exit;
