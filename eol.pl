#!/usr/bin/perl
use strict;
use warnings;
my $usage = <<"USAGE";
Usage: $0 [options] file(s)
\t{ -mac | -pc (-dos | -windows) | -unix }\toutput type
\t-dest { mac | pc (dos | windows) | unix }\toutput type
\t-bak\tsave backup copy in FILENAME.bak
\t-delete\tDon't save backups
\t-backup format\tmore complicated backup-copy naming scheme:
\tEx: -backup '.*.sv' (the default) saves a backup copy in: .FILENAME.sv
\tEx: -backup '*.old' saves a backup copy in: FILENAME.old
\tEx: -backup 'old.*' saves a backup copy in: old.FILENAME
\t-quiet\tNo informative messages
\t-noisy\tExtra informative messages
\t-status N\tPrint status dot every N lines
USAGE

use Getopt::Long qw/:config pass_through/;
GetOptions(
	'mac|m' => \ (my $mac = 0),
	'pc|PC|dos|DOS|d|p|w|win|windows' => \ (my $pc = 0),
	'unix|u' => \ (my $unix = 0),
	'dest=s' => \ (my $dest = 'unix'),
	'backup=s' => \ (my $backup = ''),
	'delete|D' => \ (my $delete = 0),
	'bak' => \ (my $bak = 0),
	'quiet|q' => \ (my $quiet = 0),
	'verbose|noisy' => \ (my $noisy = 0),
	'status|s=i' => \ (my $status = 1000),
) or die $usage;

$backup = '.*.sv' if $bak;
unless ($mac or $pc or $unix) {
    $mac++  if $dest =~ /^m(?:ac)?$/i;
    $pc++   if $dest =~ /^p(?:c)?$/i or $dest =~ /^d(?:os)?$/i or $dest =~ /^w(?:in(?:dows)?)?$/i;
    $unix++ if $dest =~ /^u(?:nix)?$/i;
}
my $specified = 0;
$specified += $_ ? 1 : 0 for $mac, $pc, $unix;
die "Please specify ONE of -pc (-dos / -windows) / -mac / -unix\n$usage" if $specified != 1;

my $out = "\x0a";
$out = "\x0d" if $mac;
$out = "\x0d\x0a" if $pc;
$out = "\x0a" if $unix;

if ($noisy) {
    my $style = $pc ? 'PC (DOS/Windows)' : $mac ? 'Mac (pre-OSX)' : 'Unix';
    s/\r/\\r/g, s/\n/\\n/g for my $logical = $out;
    s/(.)/sprintf "\\x%02x", ord $1/ges for my $hex = $out;
    s/(.)/sprintf "[%d]", ord $1/ges for my $dec = $out;
    warn "Outputting $style style endings ($logical = $hex = $dec)\n";
    warn "Reading from standard input...\n" unless @ARGV;
}

$^I = $delete ? '' : $backup;
$/ = "\x0a";
my $prev = '';
while (<>) {
    if ($ARGV ne $prev) {
        $. = 1;
        if ($noisy) {
            print STDERR "\tDone\n" if $prev;
            print STDERR "Processing $ARGV";
        }
        $prev = $ARGV;
    }
    if ($noisy) { print STDERR "." unless $. % $status; }
    s/^[\x0a\x0d]+(?=[^\x0a\x0d])//;
    s/(?:\x0d(?:\x0a)?)|(?:\x0a(?:\x0d)?)/$out/g;
    print;
}
print STDERR "\tDone\n" if $noisy;
