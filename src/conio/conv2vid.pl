#!/usr/bin/perl

use MIME::Decoder;

my @fontfiles=glob("/usr/share/syscons/fonts/*.fnt");
$decoder = new MIME::Decoder 'x-uuencode' or die "unsupported";

my $fonts=0;

open COUT, "> allfonts.c";
print COUT "#include <stdio.h>		/* NULL */\n\n";
print COUT "#include \"allfonts.h\"\n\n";
my $started;
my %fontdata;
foreach my $fontfile (@fontfiles) {
	my $width,$height;
	my $decoded=`uudecode -o /dev/stdout $fontfile`;
	my $bytes=length($decoded);
	my $fname=$fontfile;
	$fname =~ s/\.fnt$//;
	$fname =~ s/^.*\///;
	$fname =~ s/-/_/g;
	if($fname =~ /([0-9]+)x([0-9]+)/) {
		$width=$1;
		$height=$2;
	}

	my $lines=0;
	$started=0;
	$fontdata{$fname}="\"";
	while(length($decoded)) {
		my $ch=substr($decoded,0,1);
		$decoded=substr($decoded,1);
		$fontdata{$fname}.='\x'.unpack("H2",$ch);
		$lines++;
		if($height && !($lines % $height)) {
			if(length($decoded)) {
				$fontdata{$fname}.="\"\n\t\t\"";
			}
		}
	}
	$fontdata{$fname}.="\"\n\t";
	$fonts++;
}

open IN, "< /usr/share/syscons/fonts/INDEX.fonts";
my %fonts;
my %fontsizes;
while (<IN>) {
	next if(/^\s*#/);
	chomp;
	my ($filename, $lang, $desc) = split(/:/);
	next unless ($lang eq 'en');
	next if ($filename !~ /\.fnt$/);
	$filename =~ s/\..*?$//;
	$filename =~ s/-/_/g;
	$filename =~ s/_(8x[0-9]*)$//;
	if($1 eq '8x16') {
		$fontsizes{$filename} |=1;
	}
	elsif($1 eq '8x14') {
		$fontsizes{$filename} |=2;
	}
	elsif($1 eq '8x8') {
		$fontsizes{$filename} |=4;
	}
	else {
		print STDERR "Unknown font size: $1\n";
	}
	$desc =~ s/"/\\"/g;
	$desc =~ s/\s+8x[0-9]*//g;
	$desc =~ s/,\s*$//g;
	$fonts{$filename}=$desc;
}
close IN;
my $arraysize=(scalar keys %fonts)+1;
print COUT "struct conio_font_data_struct conio_fontdata[".$arraysize."] = {\n";
$started=0;
foreach my $font (keys %fonts) {
	if($started) {
		print COUT "\t,{";
	}
	else {
		print COUT "\t {";
		$started=1;
	}
	if($fontsizes{$font} & 1) {
		print COUT $fontdata{"$font\_8x16"};
		print COUT ", ";
	}
	else {
		print COUT "NULL, ";
	}
	if($fontsizes{$font} & 2) {
		print COUT $fontdata{"$font\_8x14"};
		print COUT ", ";
	}
	else {
		print COUT "NULL, ";
	}
	if($fontsizes{$font} & 4) {
		print COUT $fontdata{"$font\_8x8"};
		print COUT ", ";
	}
	else {
		print COUT "NULL, ";
	}
	print COUT "\"$fonts{$font}\"}\n";
}
print COUT "\t,{NULL, NULL, NULL, NULL}\n";
print COUT "};\n";

close COUT;

open HOUT,"> allfonts.h";
print HOUT <<ENDOFHEADER;
#ifndef _ALLFONTS_H_
#define _ALLFONTS_H_

struct conio_font_data_struct {
        char *eight_by_sixteen;
        char *eight_by_fourteen;
        char *eight_by_eight;
        char *desc;
};

extern struct conio_font_data_struct conio_fontdata[$arraysize];

#endif
ENDOFHEADER
