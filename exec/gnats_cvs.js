#!/sbbs/exec/jsexec -x -c /sbbs/ctrl
load("gnatslib.js");

var logmsg='----- CVS Commit Message -----\n';
var name=argv[0];

var gnats_user="guest";
var password=undefined;
if(argc>1)
        gnats_user=argv[1];
if(argc>2)
        password=argv[2];
var gnats = new GNATS("bugs.synchro.net",gnats_user,password);

if(!gnats.connect())
        handle_error();

while((line=readln()) != undefined)
	logmsg += line+"\n";
logmsg+='--- End CVS Commit Message ---\n\n';

auditprs=new Object;
stateprs=new Object;
myRe=/PR:([0-9]+)/g;
while((pr=myRe.exec(logmsg))!=undefined) {
	auditprs[pr[1]]=pr[1];
}

myRe=/PR:([0-9]+)-\>([a-z]*)/g;
while((pr=myRe.exec(logmsg))!=undefined) {
	stateprs[pr[1]]=pr[2];
}

for(pr in stateprs) {
	var oldstate = gnats.get_field(pr,"State");
	oldstate = oldstate.replace(/[\r\n]/g,'');
	if(stateprs[pr] != oldstate) {
		log("Changeing state of PR: "+pr+" to "+stateprs[pr]);
		if(!gnats.replace(pr, "State", stateprs[pr], logmsg))
			handle_error();
		else
			delete auditprs[pr];
	}
}

for(pr in auditprs) {
	var oldname = gnats.get_field(pr,"Responsible");
	oldname = oldname.replace(/[\r\n]/g,'');
	if(name != oldname) {
		log("Changeing responsible to "+name+" for PR: "+pr);
		if(!gnats.replace(pr, "Responsible", name, logmsg))
			handle_error();
	}
	else {
		log("Sending followup to PR: "+pr);
		if(!gnats.send_followup(pr, name, name, logmsg))
			handle_error();
	}
}

function handle_error()
{
        log(gnats.error);
        gnats.close();
        exit(1);
}
