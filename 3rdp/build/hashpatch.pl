my $path = shift;
while(<>) {
	s/^(#define CRYPTLIB_VERSION.*)$/"$1\n#define CRYPTLIB_PATCHES \"" . (chomp($val = `cat $path\/cl-*.patch | if (which md5sum > \/dev\/null 2>&1); then md5sum; else md5; fi`), $val) . "\""/e;
} continue {
	print
}
