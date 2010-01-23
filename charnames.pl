#!/usr/bin/perl
use strict;
use warnings;
use charnames qw/:full/;
use open ':std', ':utf8';
unless (eval { require "Unicode/Unihan.pm"; 1 }) {
	eval <<'UNICODY';
package Unicode::Unihan;
use Unicode::UCD 'charscript';
{
	my $warned = 0;
	sub warnonce {
		return if $warned++;
		die "No Unicode::Unihan\n";
	}
}
sub new { bless {}, __PACKAGE__ }
sub unimp { &warnonce; undef }
*Mandarin = \&unimp;
*On = \&unimp;
*Kun = \&unimp;
UNICODY
}
use Unicode::UCD 'charinfo';
use Getopt::Long qw/:config pass_through/;
GetOptions(
	'latin|nonlatin' => \(my $nonlatin = 0),
	'ascii|nonascii' => \(my $nonascii = 0),
	'unique+' => \(my $uniq = 0),
	'chr|low=s' => \(my $low = 0),
	'high=s' => \(my $high = 0),
	'all' => \(my $show_all = 0),
) or die 'options';
($low, $high) = ($1, $2) if ($low||'') =~ /^([\da-fx]+)(?:\.\.|-)([\da-fx]+)$/i;
$_ = oct for $low, $high;
$low ||= 0x20 if $high;
$high ||= 0x4000 if $low;
my $han = Unicode::Unihan->new;

sub info {
	local $_ = @_ ? shift : $_;
	my $c = $_;
	$show_all or $c =~ s/([[:^print:]])/sprintf "\\u%04x", ord $1/ge;
	my $x = sprintf "%04x", ord;
	my $uni = charinfo(ord);
	my $n = charnames::viacode(ord);
	my $script = $$uni{script} || $$uni{block} || '';
	next if $nonascii and $c =~ /[\x20-\x7e]/;
	next if $nonlatin and $script =~ /^(?:common|latin)$/i;
	my $h = ($script eq 'Han') ? $han->Mandarin($_) : '';
	print join "\t", $c, $x, $n||$h||'(no info)';
	print "\n";
}

if ($low) {
	info for map chr, $low..$high;
	exit;
}
my %seen;
while (<>) {
	chomp;
	%seen = () unless $uniq > 1;
	info for grep !$uniq || !$seen{$_}++, split //;
}
