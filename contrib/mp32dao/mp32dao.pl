#!/usr/bin/perl 
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
#Now with cdtext, mp3 and ogg support
#Support for cdrecord: builds .inf files
#Giuseppe Corbelli <cowo@lugbs.linux.it>
#Original code by Joe Steward <mp32dao@httptech.com>
use FindBin;
use lib $FindBin::Bin;
use Cwd;
use strict;
use MediaHandler;
use Audio::Wav;
use Audio::Tools::Time;

my $wav = new Audio::Wav;
my ($count, $totalsamples, $divided, $rounded, $remainder, $artist, $album, $total, $song, $fh);
my $cdtext = 1;
my $samplestart = 0;
my (@tracklist, @filelist);

print "-------- MP3-to-DAO Helper Script ---------\n";
print "Usage: mp32dao.pl [tocfile]\n";

my $tocfile = $ARGV[0];
unless ($tocfile) {
    $tocfile = "cd.toc";
    warn "No toc file specified, using default \"cd.toc\"\n";
}
my ($list) = MediaHandler->new (getcwd());
die "No supported media files found!\n" if ($list->total == 0);
#Creates array of mediahandlers, sets artist and album.
foreach my $file (@{$list->list})	{
	if (!$file->artist || !$file->title || !$file->album) {$cdtext = 0};
	#Sets artist and album name
	if ($cdtext)	{
		#If artist is still not defined define as in first song tag
		if (!$artist)	{
			$artist=$file->artist;
		} 
		#If different artists mark as VVAA
		if ( ! $artist =~ /\s*\Q$file->artist\E\s*/i)	{
			$artist = "Various Artists";
		}
		#Same for album name
		if (!$album)	{ $album = $file->album }
		if (! $album =~ m/\Q$file->album\E/i)	{$album = "Compilation"};
	}
}
open ($fh, ">$tocfile") or die "Couldn't open $tocfile for write!\n";
tocfile_header($fh, $cdtext, $album, $artist);

#Set outputfiles and decode to wav.
print "\nDecoding compressed files to wav\n";
foreach my $file (@{$list->list})	{
	if ( $_=($file->to_wav) != 0 ) {
		printf ("\n\tWARNING: decoder for file %s exited with code %d\n", $file->Filename, $_);
	}
}
$count=0;
print "\nAnalyzing wav files and creating toc\n\n";
while ($count <= ($list->total-1))	{
	my $file = ${$list->list}[$count];
    my $read = $wav->read($file->Outfile);
    my $audio_bytes = $read->length;
    my $time = Audio::Tools::Time->new (44100, 16, 2);
    my $sample = $time->bytes_to_samples($audio_bytes);	
	tocfile_entry ($fh, $cdtext, $file, $samplestart, $count+1);
	#if ($cdtext) {build_inf ($file, $count)};
	if ($cdtext) {$file->write_inf ($count+1)};
    $totalsamples = $sample - $samplestart;
    $divided = $totalsamples / 588; 
    $rounded = int($divided) * 588;
    $remainder = $totalsamples - $rounded;
    printf ("\tNeed to pad up %d samples for track %d\n", $remainder, $count+1);
	if (($remainder != 0) && ($count ne ($list->total-1))) { 
		my $nextfile = ${$list->list}[$count+1];
		print $fh "FILE \"".$nextfile->Outfile."\" ";
		print $fh "0 $remainder\n\n";
		$samplestart = $remainder;
	}	
	$count++;
}
print "\nFinished writing TOC/inf files. You may now burn the CD using\n\
    \tcdrdao write $tocfile\
    \tcdrecord dev=X,Y,Z -useinfo -dao -text -audio *.wav\n\n";
print "You may want to normalize wav files right now.\n";
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

#Arguments:
#	fh: filehandle opened for writing
#	cdtext: if != 0 cdtext info wanted
#	title: only if cdtext wanted, is the album title
#	performer: only if cdtext wanted, is the album performer
sub tocfile_header	{
	my ($fh, $cdtext, $title, $performer) = @_;
	print $fh "CD_DA\n";
	if ($cdtext)	{
		print $fh "CD_TEXT \{\n";
			print $fh "\tLANGUAGE_MAP \{\n";
 		   print $fh "\t\t0 : EN\n";
			print $fh "\t\}\n\n";
		print $fh "\tLANGUAGE 0 \{\n";
			print $fh "\t\tTITLE \"$title\"\n";
			print $fh "\t\tPERFORMER \"$performer\"\n";
			#print $fh "\t\tDISC_ID \"XY12345\"\n";
			#print $fh "\t\tUPC_EAN \"\"\n";
			print $fh "\t\}\n";
		print $fh "\}\n";
	}
}

#Arguments:
#	fh: Filehandle opened for writing
#	cdtext: scalar != 0 if cdtext ON
#	song: reference to song structure
#	samplestart: scalar containing sample offset
#	count: track number
sub tocfile_entry	{
	my ($fh, $cdtext, $song, $samplestart, $count) = @_;
	
    print $fh "// Track $count\n";
    print $fh "TRACK AUDIO\n";
    print $fh "TWO_CHANNEL_AUDIO\n";
	if ($cdtext)	{
    	print $fh "CD_TEXT {\n";
    	print $fh "\tLANGUAGE 0 {\n";
    	print $fh "\t\tTITLE \"".$song->title."\"\n";
    	print $fh "\t\tPERFORMER \"".$song->artist."\"\n";
		#print $fh "\t\tISRC \"US-XX1-98-01234\"\n";
    	print $fh "\t}\n}\n";
    }
    print $fh "FILE \"".$song->Outfile."\" ";
	print $fh "$samplestart";
    print $fh "\n";
}
