#!/usr/bin/perl

# mp32dao.pl version 0.2 10/03/1999

use Audio::Wav;
use Audio::Tools::Time;
use strict;

my $wav = new Audio::Wav;

my ($count, $totalsamples, $divided, $rounded, $remainder);
my %tracks;
my @name = split(/\//, $0);
my $name = pop(@name);
print "-------- MP3-to-DAO Helper Script ---------\n";
print "Usage: $name [tocfile]\n";

my $tocfile = $ARGV[0];
unless ($tocfile) {
    $tocfile = "cd.toc";
    print "No toc file specified, using default \"cd.toc\"\n";
}


while (<*>) {
    chomp;
    if (/\.mp3$/i) {
    $count++;
    $count = sprintf("%02d", $count);
    $tracks{$count} = $_;
    }
}
$count || die "No mp3 files found!\n";

open (TOC, ">$tocfile") || die "Couldn't open $tocfile for write!\n";
my @tracksort = sort {$a <=> $b} keys %tracks;
my $samplestart = 0;
my $totaltracks =  scalar(@tracksort);

for (@tracksort) {
    my $infile = quotemeta($tracks{$_});

    unless(-e "$_\.wav") {    system("mpg123 -w $_\.wav -v $infile") }
        else { print "$_\.wav exists, skipping mp3 decode\n" }

    my $read = $wav->read( "$_\.wav" );
    my $audio_bytes = $read -> length();
    my $time = new Audio::Tools::Time 44100, 16, 2;
    my $sample = $time -> bytes_to_samples( $audio_bytes );

    print TOC "// Track $_\n";
    print TOC "TRACK AUDIO\n";
    print TOC "TWO_CHANNEL_AUDIO\n";
    print TOC "FILE \"$_\.wav\" ";
    print TOC "$samplestart\n";
    $totalsamples = $sample - $samplestart;
    $divided = $totalsamples / 588; 
    $rounded = int($divided) * 588;
    $remainder = $totalsamples - $rounded;
    print "Need to pad up $remainder samples\n";
    if (($remainder != 0) && ($_ != $totaltracks)) { 
	my $nextfile = $_ + 1;
	$nextfile = sprintf("%02d", $nextfile);
	print TOC "FILE \"$nextfile\.wav\" ";
	print TOC "0 $remainder\n\n";
	$samplestart = $remainder;
	}
}

print "Finished writing TOC file. You may now burn the CD using\n\n\tcdrdao write $tocfile\n\n";
close (TOC);
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

__END__

=head1 NAME

mp32dao - prepare a set of mp3 files to be burned directly to a CD using cdrdao

=head1 SYNOPSIS

B<mp32dao> [ F<tocfile> ]

=head1 DESCRIPTION

mp32dao requires the following:

=over 8

=item *
Perl 5

=item *
Audio::Wav Perl module
Audio::Tools::Time Perl module

=item *
cdrdao

=item *
mpg123

=back

=head1 DESCRIPTION

This script was written to facilitate the burning of DAO CD's
from a set of mp3 files. cdrdao does a good job of burning
DAO on Linux, but you need a Table of Contents (TOC) such as
the type that are stored on a CD in order to make it work. 
This script grabs the sample data out of the wave file headers
and writes the TOC correctly to make each track a multiple of
588 samples, by "stealing" samples from the next track, thus
avoiding having to pad the track with zero bytes, causing a
pop between tracks.

For instance, if you have stored your latest live music performance
in several mp3 files for convenience, but you want the CD to be
burned without the 2 second "pre-gap" between each file, you would
need to use this script to generate the TOC with the precise file
times to ensure continuity.

=head1 USAGE

Run this script in a directory containing your mp3 files. (you will
need to have 650+ megabytes free to hold the wave files)
It will convert all the mp3's to wave format using mpg123, then
will write a TOC suitable for burning a CD with cdrdao.

=head1 BUGS

Make sure your mp3's are titled appropriately to be listed in numeric
order, I.E.

F<01_first_file.mp3>

F<02_second_file.mp3>

F<03_third_file.mp3>

etc... 

This script will write the CD in the order it finds the files in the directory
list!

=head1 AUTHOR

Joe Stewart <mp32dao@httptech.com>

=head1 LICENSE

This program is released under the GNU Public License.

