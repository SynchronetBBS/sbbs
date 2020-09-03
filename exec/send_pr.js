// $Id: send_pr.js,v 1.24 2005/08/09 22:25:22 deuce Exp $

load("sbbsdefs.js");
load("gnatslib.js");

const REVISION = "$Revision: 1.24 $".split(' ')[1];

const MAX_LINE_LEN = 78

console.clear();
console.center(format("Synchronet Bug Submission Module %s\r\n", REVISION));
console.crlf();

gnats=new GNATS("bugs.synchro.net","guest");
if(!gnats.connect()) {
	writeln(gnats.error);
	exit();
}

pr = new Object;
var tmp;

pr.Originator = user.name;
pr.Confidential = console.noyes("Confidential")?'no':'yes';
if(console.aborted)
	exit();
console.print("\r\n\1y\1hOne-line synopsis of the problem:\r\n");
pr.Synopsis = truncsp(console.getstr(MAX_LINE_LEN, K_LINE));
if(console.aborted || !pr.Synopsis.length)
	exit();
severity = gnats.get_valid("Severity");
for(i=0; i<severity.length; i++) {
	console.uselect(i,"Severity",severity[i]);
}
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Severity=severity[tmp];
priority = gnats.get_valid("Priority");
for(i=0; i<priority.length; i++) {
	console.uselect(i,"Priority",priority[i]);
}
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Priority=priority[tmp];
allcats=gnats.get_list("Categories");
cats=new Array();
for(i=0; i<allcats.length; i++) {
	var flds=allcats[i].split(/:/);
	cats.push(flds[0]);
	console.uselect(i, "Category", flds[1], "");
}
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Category=cats[tmp];
cls = gnats.get_valid("Class");
for(i=0; i<cls.length; i++) {
	if(!gnats.cmd("ADMV","class",cls[i]))
		continue;
	if(!gnats.expect("ADMV",350))
		continue;
	var flds=gnats.response.message.split(/:/);
	console.uselect(i, "Class", flds[2], "");
}
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Class=cls[tmp];
pr.Version=system.version_notice+system.revision+" Compiled: "+system.compiled_when+" with "+system.compiled_with;
pr.Environment=system.os_version+"\r\n"+system.js_version+"\r\n"+system.socket_lib+"\r\n"+system.msgbase_lib;
console.print("\r\n\1y\1hPrecise description of the problem (Blank line ends):\r\n");
pr.Description = '';
do {
	var line=truncsp(console.getstr(MAX_LINE_LEN, K_WRAP | K_LINE));
	if(console.aborted)
		exit();
	pr.Description += line + "\r\n";
} while (line != '' && bbs.online);
console.print("\1y\1hSteps to reproduce the problem (Blank line ends):\r\n");
pr.HowToRepeat = '';
do {
	var line=truncsp(console.getstr(MAX_LINE_LEN, K_WRAP | K_LINE));
	if(console.aborted)
		exit();
	pr.HowToRepeat += line + "\r\n";
} while (line != '' && bbs.online);
console.print("\1y\1hFix/Workaround if known (Blank line ends):\r\n");
pr.Fix = '';
do {
	var line=truncsp(console.getstr(MAX_LINE_LEN, K_WRAP | K_LINE));
	if(console.aborted)
		exit();
	pr.Fix += line + "\r\n";
} while (line != '' && bbs.online);

var body='';
body += "To: bugs\r\n";
body += "CC:\r\n";
body += "Subject: "+pr.Synopsis+"\r\n";
body += "From: "+user.name+" <"+user.email+">\r\n";
body += "Reply-To: "+user.name+" <"+user.email+">\r\n";
body += "X-Send-Pr-Version: Synchronet send_pr.js\r\n";
body += "\r\n";
body += ">Originator:\t" + pr.Originator + "\r\n";
body += ">Confidential:\t" + pr.Confidential + "\r\n";
body += ">Synopsis:\t" + pr.Synopsis + "\r\n";
body += ">Severity:\t" + pr.Severity + "\r\n";
body += ">Priority:\t" + pr.Priority + "\r\n";
body += ">Category:\t" + pr.Category + "\r\n";
body += ">Class:\t" + pr.Class + "\r\n";
body += ">Version:\t" + pr.Version + "\r\n";
body += ">Environment:\r\n" + pr.Environment + "\r\n";
body += ">Description:\r\n" + pr.Description;
body += ">How-To-Repeat:\r\n" + pr.HowToRepeat;
body += ">Fix:\r\n" + pr.Fix;

// Only submit if the user is still online...
if(bbs.online) {
	if(!gnats.submit(body)) {
		alert(gnats.error);
		console.pause();
	} else {
		console.print("\1y\1hProblem Report (PR) submitted successfully.\r\n");
		console.print(gnats.message);
	}
}
gnats.close();
