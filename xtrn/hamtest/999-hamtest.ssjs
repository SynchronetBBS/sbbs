// Ham Exam

/*
 * You will need to set the path in mod_opts.ini
 * in the HamExam section
 */

function encode(str)
{
	return html_encode(str, false, true, false, false);
}

var test={
	question_list:[],
};
var i;
var correct=0;
var wrong=0;
if(http_request.query.action==undefined) {
	print('<h3>Select Exam Class</h3><ul>');
	print('<li><a class="link" href="?page='+http_request.query.page[0]+'&amp;action=advanced">Advanced (Canadian)</a></li>');
	print('<li><a class="link" href="?page='+http_request.query.page[0]+'&amp;action=basic">Basic (Canadian)</a></li>');
	print('<li><a class="link" href="?page='+http_request.query.page[0]+'&amp;action=extra">Extra (American)</a></li>');
	print('<li><a class="link" href="?page='+http_request.query.page[0]+'&amp;action=general">General (American)</a></li>');
	print('<li><a class="link" href="?page='+http_request.query.page[0]+'&amp;action=technician">Technician (American)</a></li>');
	print('</ul>');
}
else {
	var opts=load('modopts.js','HamExam');

	switch(http_request.query.action[0]) {
		case 'advanced':
			test.name='Advanced';
			test.pass=70;
			break;
		case 'basic':
			test.name='Basic';
			test.pass=70;
			test.honours=80;
			break;
		case 'extra':
			test.name="Extra";
			test.pass=74;
			break;
		case 'general':
			test.name="General";
			test.pass=74;
			break;
		case 'technician':
			test.name="Technician";
			test.pass=74;
			break;
	}
	var obj={};
	load(obj,opts.path+'/'+test.name+".js");
	if(http_request.query.questions==undefined) {
		if(http_request.query.correct==undefined) {
			for (var subelement in obj.test) {
				for (var group in obj.test[subelement]) {
					test.question_list.push(subelement+'.'+group+'.'+random(obj.test[subelement][group].questions.length));
				}
			}
		}
	}
	else {
		test.question_list=http_request.query.questions;
	}
	test.questions=[];
	var q,m;
	for(i in test.question_list) {
		q={};
		m=test.question_list[i].match(/^([^.+]+).([^.+]+).([^.+]+)$/);

		if(m) {
			q.subelement=m[1];
			q.group=m[2];
			q.question=obj.test[m[1]][m[2]].questions[m[3]];
			test.questions.push(q);
		}
	}
	q=undefined;
	print('<form action="?page='+http_request.query.page[0]+'&amp;action='+http_request.query.action[0]+'" method="POST"><div>');
	if(http_request.query.correct != undefined) {
		correct = parseInt(http_request.query.correct[0],10);
	}
	if(http_request.query.wrong != undefined) {
		wrong = parseInt(http_request.query.wrong[0],10);
	}
	if(http_request.query.answer!=undefined && http_request.query.question!=undefined) {
		m=http_request.query.question[0].match(/^([^.]+).([^.]+).([^.]+)$/);
		if(m) {
			var tmp=obj.test[m[1]][m[2]].questions[m[3]];
			if(tmp.answers[tmp.answer] == http_request.query.answer[0]) {
				if(http_request.query.failed==undefined)
					correct++;
			}
			else {
				q={};
				q.subelement=m[1];
				q.group=m[2];
				q.question=obj.test[m[1]][m[2]].questions[m[3]];
				q.q=http_request.query.question[0]
				q.failed={};
				q.failed[http_request.query.answer[0]]=1;
				if(http_request.query.failed!=undefined) {
					for(i in http_request.query.failed) {
						q.failed[http_request.query.failed[i]]=1;
					}
				}
				else {
					wrong++;
				}
				for(i in q.failed) {
					print('<input type="hidden" name="failed" value="'+encode(i)+'">');
				}
			}
			test.questions.push(q);
		}
	}
	print('<input type="hidden" name="correct" value="'+correct+'">');
	print('<input type="hidden" name="wrong" value="'+wrong+'">');
	if(q==undefined && test.question_list.length) {
		i=random(test.question_list.length);
		q=test.questions[i];
		q.failed={};
		test.questions.splice(i,1);
		q.q=test.question_list.splice(i,1)[0];
	}
	if(q==undefined) {
		// Test is all done!
		print('<div class="results" style="text-align: center">');
		print("<h3>You answered "+correct+" out of "+(correct+wrong)+" correctly!</h3>");
		var pct = Math.floor(correct/(correct+wrong)*100);

		write('<h1>'+pct+'% ');
		if(pct >= test.pass) {
			write('YOU PASSED');
			if(test.honours != undefined && pct >= test.honours)
				write(' WITH HONOURS');
			write('!');
		}
		else {
			write('is not a passing score - keep trying!');
		}
		print('</h1></div>');
	}
	else {
		print('<input type="hidden" name="question" value="'+encode(q.q)+'">');
		for(i in test.question_list) {
			print('<input type="hidden" name="questions" value="'+encode(test.question_list[i])+'">');
		}

		print('<b>'+q.subelement+'</b><br>');
		print(q.question.title+'<br><br>');
		print('<div class="answers" style="text-align: center">');
		print('<h2>'+q.question.question+'</h2><br>');
		var ans;
		while(q.question.answers.length) {
			ans=q.question.answers.splice(random(q.question.answers.length),1)[0];
			print('<button style="border-style: none;" '+(q.failed[ans]!=undefined?'disabled class="backDropColor" ':'class="standardColor link" ')+'type="submit" name="answer" value="'+encode(ans)+'">'+encode(ans)+'</button><br>');
		}
		print('</div><br>');
		print(correct+' out of '+(correct+wrong)+' correct, '+(test.question_list.length+1)+' remaining.');
	}
	print('</div></form>');
}
