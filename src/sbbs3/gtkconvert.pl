#!/usr/bin/perl

use strict;
use warnings;

my $filename = shift or die "Usage: $0 FILENAME\n";

open my $fh, "<", $filename || die "Cannot open $filename $!\n";
my $char;
my $newstr;
my $count;
while (read $fh,$char,1 ) {
  $newstr .= ord($char) . ",";
  $count++;
}
chop $newstr;
close $fh;

$filename =~ s/\./_/;
$filename .= ".c";

open my $fh2, ">" , $filename || die "Cannot open g $!\n";
print $fh2 "const char builder_interface[" . $count ."] = {" . $newstr . "};";
close $fh2;

