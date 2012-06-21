function select_test()
{
	var obj={};
	var questions=[];
	console.mnemonics("Select exam class:\r\n~Advanced (Canadian)\r\n~Basic (Canadian)\r\n~Extra\r\n~General\r\n~Technician\r\n~Quit\r\n");
	switch(console.getkeys("ABEGTQ")) {
		case 'Q':
			exit;
		case 'A':
			load(obj,"Advanced.js");
			break;
		case 'B':
			load(obj,"Basic.js");
			break;
		case 'E':
			load(obj,"Extra.js");
			break;
		case 'G':
			load(obj,"General.js");
			break;
		case 'T':
			load(obj,"Technician.js");
			break;
	}
	for (var subelement in obj.test) {
		for (var group in obj.test[subelement]) {
			var q={};
			q.subelement=subelement;
			q.group=obj.test[subelement][group].group;
			q.question=obj.test[subelement][group].questions[random(obj.test[subelement][group].questions.length)];
			questions.push(q);
		}
	}
	return questions;
}
var test=select_test();
var correct=0;
var total=0;
var answered=[];
while(test.length) {
	console.clear();
	console.writeln(correct+" out of "+total+" correct, "+test.length+" remaining");
	var qa=test.splice(random(test.length), 1);
	var answer=qa[0].question.answers[qa[0].question.answer];
	var letter='A';
	var letters='';
	var la;

	console.crlf();
	console.writeln(word_wrap(qa[0].question.question,console.screen_columns-1,false).replace(/\n/g,"\r\n"));
	while(qa[0].question.answers.length) {
		var tanswer=qa[0].question.answers.splice(random(qa[0].question.answers.length),1);
		if(tanswer == answer)
			la=letter;
		letters += letter;
		console.mnemonics('~'+word_wrap(letter+' '+tanswer,console.screen_columns-1,false).replace(/\n/g,"\r\n"));
		letter=String.fromCharCode(letter.charCodeAt(0)+1);
	}
	console.crlf();
	console.write("Your answer (Q to quit): ");
	total++;
	var ma=console.getkeys(letters+'Q');
	if(ma=='Q')
		break;
	if(ma == la) {
		console.crlf();
		console.writeln("Correct!");
		correct++;
	}
	else {
		console.crlf();
		console.writeln("WRONG! Correct answer was "+la);
	}
	console.crlf();
	console.pause();
}

console.clear();
console.center("You answered "+correct+" out of "+total+" correctly!");
console.center(Math.floor(correct/total*100)+"%");
