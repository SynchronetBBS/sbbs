load("sbbsdefs.js");
load("gnatslib.js");

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
console.print("YOne-line synopsis of the problem: W");
pr.Synopsis = console.getstr();
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
pr.Release=system.version_notice
pr.ReleaseNote="\t"+system.os_version+"\r\n\t"+system.js_version+"\r\n\t"+system.socket_lib+"\r\n\t"+system.msgbase_lib+"\r\n\tCompiled: "+system.compiled_when+' with '+system.compiled_with;
console.print("\nYPrecise description of the problem (Blank line ends)W\n");
pr.Description = '';
do {
	var line=console.getstr();
	pr.Description += "\t" + line + "\r\n";
} while (line != '');
console.print("YSteps to reproduce the problem (Blank line ends)W\n");
pr.HowToRepeat = '';
do {
	var line=console.getstr();
	pr.HowToRepeat += "\t" + line + "\r\n";
} while (line != '');
console.print("YFix/Workaround if known (Blank line ends)W\n");
pr.Fix = '';
do {
	var line=console.getstr();
	pr.Fix += "\t" + line + "\r\n";
} while (line != '');

var body='';
body += "To: bugs\r\n";
body += "CC:\r\n";
body += "Subject: "+pr.Synopsis+"\r\n";
body += "From: "+user.name+" <"+user.alias+"@"+system.inet_addr+">\r\n";
body += "Reply-To: "+user.name+" <"+user.alias+"@"+system.inet_addr+">\r\n";
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
body += ">Release-Note:\r\n" + pr.ReleaseNote + "\r\n";
body += ">Description:\r\n" + pr.Description;
body += ">How-To-Repeat:\r\n" + pr.HowToRepeat;
body += ">Fix:\r\n" + pr.Fix;

if(!gnats.submit(body)) {
	writeln(gnats.error);
	console.pause();
}
gnats.close();
