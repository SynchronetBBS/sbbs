#!/usr/bin/perl

my $title;
my $se;
my $gr;
my $state;
my $answer;
my $question;
my @answers;
my $correct;
my %f;
while (<>) {
	s/[\r\n]//g;
	s/'/\\'/g;
	s/"/\\"/g;
	s/\x96/-/g;
	s/-\?/-/g;
	if($state eq 'IN') {
		if(/^~~$/ || /^\s*$/) {
			# Write out question here...
			$f{$se}->{$gr}{$title}{question}=$question;
			$f{$se}->{$gr}{$title}{answer}=ord($answer)-ord('A');
			$f{$se}->{$gr}{$title}{answers}=[@answers];
			$state='';
			@answers=();
		}
		else {
			if(/[A-Z]\.\s+(.*)/) {
				push(@answers,$1);
			}
		}
	}
	elsif($state eq 'Q') {
		$question=$_;
		$state='IN';
	}
	elsif(/^[A-Z][0-9][A-Z][0-9]{2} \(([A-Z])\)/) {
		$answer=$1;
		$title=$_;
		$state='Q';
	}
	elsif(/^SUBELEMENT/) {
		$se=$_;
	}
	elsif(/[A-Z][0-9][A-Z]\s/) {
		$gr=$_;
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
