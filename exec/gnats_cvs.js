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

for(pr in auditprs) {
	if(name != gnats.get_field(pr,"Responsible")) {
		log("Changeing responsible to "+name+" for PR: "+pr);
		if(!gnats.replace(pr, "Responsible", name, "CVS Commit"))
			handle_error();
	}
	log("Appending log info to PR: "+pr);
	if(!gnats.append(pr,"Audit-Trail", logmsg, "CVS Commit"))
		handle_error();
}

for(pr in stateprs) {
	if(stateprs[pr] != gnats.get_field(pr,"State")) {
		log("Changeing state of PR: "+pr+" to "+stateprs[pr]);
		if(!gnats.replace(pr, "State", stateprs[pr], "CVS Commit"))
			handle_error();
	}
}

function handle_error()
{
        log(gnats.error);
        gnats.close();
        exit(1);
}
