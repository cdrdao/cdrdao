#!/usr/bin/perl

use strict 'subs';
use strict 'refs';

@dev = ('--device', '/dev/pg0:0,0');

sub help {
	print "Syntax: $0 [-H] [options] audiofiles\n";
	print <<"EOF" ;
Use cdrdao on the wav audio file arguments, making an appropriate toc file.
-d cdrw	Use cdrw as the CDRW device (default: $dev[1]).
-o file	Output the toc file on this file - do not use a temporary file.
-p	Perform a 'print-size' cdrdao command.
-i	Perform a 'toc-info' cdrdao command.
-c	Perform a 'show-toc' cdrdao command.
-t	Perform a 'read-test' cdrdao command.
-w	Write the CD in DAO mode (default, if no other action is specified).
-s	Simulate writing only ('simulate' instead of 'write' command).
-j	Do not eject the CD after writing it.
-n	Print the cdrdao commands, instead of executing them.
EOF
}

require 'getopts.pl';
&Getopts('o:pictwsjnH');
if ($opt_H) { &help ; exit }
$dev[1] = $opt_d if $opt_d;
$opt_w = 1 unless $opt_p || $opt_i || $opt_c || $opt_t || $opt_w || $opt_s || $opt_o ne "";

die "Usage: $0 [options] audiofiles" unless @ARGV;

$fname = $opt_o ne "" ? $opt_o : "/tmp/toc$$";
open(F, "> $fname") || die "open($fname): $!, stopped";
print F "CD_DA\n";

foreach (@ARGV) {
	print F "\nTRACK AUDIO\nNO COPY\n";
#	print F "NO PRE_EMPHASIS\nTWO_CHANNEL_AUDIO\n";
	print F "FILE \"$_\" 0\n";
#	print F "START 00:02:00\n" if $no++;
}
close F;

if ($opt_p) {
	if ($opt_n) { print "cdrdao print-size $fname\n" }
	else { system 'cdrdao', 'print-size', $fname}
}

if ($opt_i) {
	if ($opt_n) { print "cdrdao toc-info $fname\n" }
	else { system 'cdrdao', 'toc-info', $fname}
}

if ($opt_c) {
	if ($opt_n) { print "cdrdao show-toc $fname\n" }
	else { system 'cdrdao', 'show-toc', $fname}
}
if ($opt_t) {
	if ($opt_n) { print "cdrdao read-test $fname\n" }
	else { system 'cdrdao', 'read-test', $fname}
}

if ($opt_w || $opt_s) {
	unshift @dev, $opt_s ? 'simulate' : 'write';
	push @dev, '--eject' unless $opt_s || $opt_j;
	push @dev, $fname;
	if ($opt_n) { print "cdrdao @dev\n" } else { system 'cdrdao', @dev }
}
unlink $fname unless $opt_o ne "";
__END__
