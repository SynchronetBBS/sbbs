function select_test()
{
	var obj={};
	var test={pass:100,questions:[]};
	console.clear();
	console.mnemonics("Select exam class:\r\n\r\n~Advanced (Canadian)\r\n~Basic (Canadian)\r\n~Extra\r\n~General\r\n~Technician\r\n~Quit\r\n");
	console.crlf();
	console.write("Class: ");
	switch(console.getkeys("ABEGTQ")) {
		default:
		case 'Q':
			return;
		case 'A':
			test.name='Advanced';
			test.pass=70;
			break;
		case 'B':
			test.name='Basic';
			test.pass=70;
			test.honours=80;
			break;
		case 'E':
			test.name="Extra";
			test.pass=74;
			break;
		case 'G':
			test.name="General";
			test.pass=74;
			break;
		case 'T':
			test.name="Technician";
			test.pass=74;
			break;
	}
	load(obj,test.name+".js");
	for (var subelement in obj.test) {
		for (var group in obj.test[subelement]) {
			var q={};
			q.subelement=subelement;
			q.group=obj.test[subelement][group].group;
			q.question=obj.test[subelement][group].questions[random(obj.test[subelement][group].questions.length)];
			test.questions.push(q);
		}
	}
	return test;
}

function administer_test(test)
{
	if(test==undefined)
		return;
	var correct=0;
	var total=0;
	var answered=[];
	var pct;
	while(test.questions.length) {
		console.clear();
		console.writeln(correct+" out of "+total+" correct, "+test.questions.length+" remaining");
		var qa=test.questions.splice(random(test.questions.length), 1);
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
	pct = Math.floor(correct/(total+test.questions.length)*100);
	var msg=pct+'% ';
	if(pct > test.pass) {
		msg += 'YOU PASSED';
		if(test.honours != undefined && pct >= test.honours)
			msg += ' WITH HONOURS';
		msg += '!';
	}
	else {
		msg += 'is not a passing score - keep trying!';
	}
	console.center(msg);
}

administer_test(select_test());
