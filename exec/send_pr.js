// $Id$

load("sbbsdefs.js");
load("gnatslib.js");

const REVISION = "$Revision$".split(' ')[1];

console.clear();
console.center(format("Synchronet Bug Submission Module %s\r\n", REVISION));
console.crlf();

gnats=new GNATS("gnats.bbsdev.net","guest");
if(!gnats.connect()) {
	writeln(gnats.error);
	exit();
}

pr = new Object;
var tmp;

pr.SubmitterId = 'default';
pr.Originator = user.name;
pr.Organization = system.name;
pr.Confidential = console.noyes("Confidential")?'no':'yes';
if(console.aborted)
	exit();
console.print("\r\n\1YOne-line synopsis of the problem\r\n:\1W ");
pr.Synopsis = console.getstr();
if(console.aborted)
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
pr.Release=system.version_notice+system.revision+" Compiled: "+system.compiled_when+" with "+system.compiled_with;
pr.Environment="\t"+system.os_version+"\r\n\t"+system.js_version+"\r\n\t"+system.socket_lib+"\r\n\t"+system.msgbase_lib;
console.print("\r\n\1YPrecise description of the problem (Blank line ends):\1W\r\n");
pr.Description = '';
do {
	var line=console.getstr();
	if(console.aborted)
		exit();
	pr.Description += "\t" + line + "\r\n";
} while (line != '');
console.print("\1YSteps to reproduce the problem (Blank line ends):\1W\r\n");
pr.HowToRepeat = '';
do {
	var line=console.getstr();
	if(console.aborted)
		exit();
	pr.HowToRepeat += "\t" + line + "\r\n";
} while (line != '');
console.print("\1YFix/Workaround if known (Blank line ends):\1W\r\n");
pr.Fix = '';
do {
	var line=console.getstr();
	if(console.aborted)
		exit();
	pr.Fix += "\t" + line + "\r\n";
} while (line != '');

var body='';
body += "To: bugs\r\n";
body += "CC:\r\n";
body += "Subject: "+pr.Synopsis+"\r\n";
body += "From: "+user.name+" <"+user.email+">\r\n";
body += "Reply-To: "+user.name+" <"+user.email+">\r\n";
body += "X-Send-Pr-Version: Synchronet send_pr.js\r\n";
body += "\r\n";
body += ">Submitter-Id:\t" + pr.SubmitterId + "\r\n";
body += ">Originator:\t" + pr.Originator + "\r\n";
body += ">Organization:\r\n\t" + pr.Organization + "\r\n";
body += ">Confidential:\t" + pr.Confidential + "\r\n";
body += ">Synopsis:\t" + pr.Synopsis + "\r\n";
body += ">Severity:\t" + pr.Severity + "\r\n";
body += ">Priority:\t" + pr.Priority + "\r\n";
body += ">Category:\t" + pr.Category + "\r\n";
body += ">Class:\t" + pr.Class + "\r\n";
body += ">Release:\t" + pr.Release + "\r\n";
body += ">Environment:\r\n" + pr.Enviroment + "\r\n";
body += ">Description:\r\n" + pr.Description;
body += ">How-To-Repeat:\r\n" + pr.HowToRepeat;
body += ">Fix:\r\n" + pr.Fix;

if(!gnats.submit(body)) {
	writeln(gnats.error);
	console.pause();
}
gnats.close();
