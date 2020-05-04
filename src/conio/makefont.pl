#!/usr/local/bin/perl

my @sources = (
	'font/plane00/unifont-base.hex',
	'font/plane00/spaces.hex',
	'font/plane00/wqy.hex',
	'font/plane00/hangul-syllables.hex',
	'font/plane00/copyleft.hex',
	'font/plane00csur/plane00csur.hex',
	'font/plane00csur/plane00csur-spaces.hex',
	'font/plane00/alt/codepage-437.hex',
	'font/plane01/plane01.hex',
	'font/plane01/space.hex',
	'font/plane0E/plane0E.hex',
	'font/plane0Fcsur/plane0Fcsur.hex'
);

my %font;
foreach my $source (@sources) {
	open IN,'<',$source;
	while (<IN>) {
		chomp;
		my ($code, $data) = split(':');
		$code = hex($code);
		$data =~ s/([0-9A-F]{2})/\\x$1/g;
		if (defined $font{$code} && $font{$code} ne $data) {
			if ($source ne './font/plane00/alt/codepage-437.hex') {
				die "Item $code already exists ($source)!";
			}
		}
		$font{$code} = $data;
	}
	close IN;
}

print "#include <stdint.h>\n";
print "#include <stdbool.h>\n\n";
print "struct unifont_entry {\n";
print "	const uint32_t cp;\n";
print "	const bool wide;\n";
print "	const char * const data;\n";
print "};\n\n";
print "const struct unifont_entry unifont[",(scalar keys %font),"] = {\n";
foreach my $cp (sort {$a <=> $b} keys %font) {
	my $wide = length($font{$cp}) > 64 ? 1 : 0;
	printf("	{0x%05x, %d, \"%s\"},\n", $cp, $wide, $font{$cp});
}
print "};\n";
