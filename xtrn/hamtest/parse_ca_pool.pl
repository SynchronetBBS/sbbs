#!/usr/bin/perl

my %f;
$/=undef;
while(my $file=<>) {
	$file =~ s/^.*CONTENT BODY -->//s;
	$file =~ s/<div class="prevNextBox.*$//s;
	$file =~ s/<div.*?<\/div>//sg;
	$file =~ s/<br \/><br \/><hr \/>//sg;
	$file =~ s/\&quot;/"/sg;
	$file =~ s/\&nbsp;/ /sg;
	$file =~ s/\&#8212;/-/sg;
	$file =~ s/>V<abbr title="High Frequency">HF<\/abbr>/>V/g;
	$file =~ s/<abbr\s+title\s*=\s*"([^"]*)"[^>]*>(.*?)<\/abbr>/\2 (\1)/sg;
	$file =~ s/<span [^>]*>([^<]*?)<\/span>/\1/sg;
	$file =~ s/<span [^>]*>([^<]*?)<\/span>/\1/sg;
	while($file =~ s/^[\s\r\n]*<p>\s*<strong>\s*(?:<a id="[^"]+">\s*<\/a>)?\s*([^\s]+)\s+\(([0-9]+)\)\s*(?:<\/strong>[\r\n\s]*|<br \/>[\r\n\s]*){2}[\r\n\s]*(.*?)<\/p>[\r\n\s]*<ul>(.*?)<\/ul>//sg) {
		my @answers;
		my ($title,$answer,$question)=($1,$2,$3);
		my $answers=$4;
		$question =~ s/[\s\r\n]+/ /gs;
		$question =~ s/<strong>([^<]*)<\/strong>/*\1*/sg;
		$question =~ s/"/\\"/g;
		my $se,$gr;
		if($title =~ /([A-Z]-[0-9]+)-([0-9]+)-/) {
			$se=$1;
			$gr="$1-$2";
		}
		while($answers=~/<li>\s*(.*?)\s*<\/li>/sg) {
			my $ans=$1;
			$ans =~ s/<strong>([^<]*)<\/strong>/*\1*/sg;
			$ans =~ s/[\s\r\n]+/ /gs;
			$ans =~ s/"/\\"/g;
			push @answers,$ans;
		}
		$f{$se}->{$gr}{$title}{question}=$question;
		$f{$se}->{$gr}{$title}{answer}=$answer-1;
		$f{$se}->{$gr}{$title}{answers}=[@answers];
		$state='';
	}
}
print "var test={\n";
foreach my $subelement (sort keys %f) {
	print "'".$subelement."':[\n";
	foreach my $group (sort keys %{$f{$subelement}}) {
		print "\t{\n\t\tgroup:'".$group."',\n\t\tquestions:[\n";
		foreach my $title (sort keys %{$f{$subelement}->{$group}}) {
			print "\t\t\t{\n\t\t\t\ttitle:\"$title\",\n";
			print "\t\t\t\tquestion:\"",$f{$subelement}->{$group}{$title}{question},"\",\n";
			print "\t\t\t\tanswer:",$f{$subelement}->{$group}{$title}{answer},",\n";
			print "\t\t\t\tanswers:[\n\t\t\t\t\"";
			print join("\",\n\t\t\t\t\t\"", @{$f{$subelement}->{$group}{$title}{answers}});
			print "\"\n\t\t\t\t],\n";
			print "\t\t\t},\n";
		}
		print "\t\t],\n\t},\n";
	}
	print "],\n";
}
print "}\n";
