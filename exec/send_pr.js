load("sbbsdefs.js");

pr = new Object;
var tmp;

pr.SubmitterId = 'default';
pr.Originator = user.name;
pr.Organization = system.name;
pr.Confidential = console.noyes("Confidential")?'no':'yes';
console.print("YOne-line synopsis of the problem: W");
pr.Synopsis = console.getstr();
severity = new Array('critical','serious','non-critical');
console.uselect(0,"Severity","Critical","SYSOP");
console.uselect(1,"Severity","Serious","");
console.uselect(2,"Severity","Non-Critical","");
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Severity=severity[tmp];
priority = new Array('high','medium','low');
console.uselect(0,"Priority","High","SYSOP");
console.uselect(1,"Priority","Medium","");
console.uselect(2,"Priority","Low","");
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Priority=priority[tmp];
cats = new Array('build-bcc','build-nix','build-vcc','ciolib','console','doc','ftp','irc','ircd','js','menuedit','newslink','nntp','pop3','scfg','services','smblib','smtp','syncterm','telnet','uedit','uifc','umonitor','useredit','webif','websrvr','win32-gui','xpdev');
// console.uselect(0, "Category", "Win32 Build System (BCC)", "SYSOP");
// console.uselect(1, "Category", "*nix Build System", "SYSOP");
// console.uselect(2, "Category", "Win32 Build System (VCC)", "SYSOP");
// console.uselect(3, "Category", "CONIO emulation library", "");
console.uselect(4, "Category", "Synchronet Console (sbbs)", "SYSOP");
console.uselect(5, "Category", "Documentation Bug", "");
console.uselect(6, "Category", "FTP Server", "");
console.uselect(7, "Category", "IRC Client", "");
console.uselect(8, "Category", "IRC Daemon", "");
console.uselect(9, "Category", "JavaScript Integration", "");
// console.uselect(10, "Category", "Menu Editor", "SYSOP");
console.uselect(11, "Category", "NewsLink", "");
console.uselect(12, "Category", "NNTP Daemon", "");
console.uselect(13, "Category", "POP3 (Outgoing mail) server", "");
console.uselect(14, "Category", "Synchronet Configuration Utility", "SYSOP");
console.uselect(15, "Category", "Services", "SYSOP");
// console.uselect(16, "Category", "SMB Library", "");
console.uselect(17, "Category", "SMTP (Incoming mail) server", "");
// console.uselect(18, "Category", "SyncTERM", "");
console.uselect(19, "Category", "Telnet Interface", "");
console.uselect(20, "Category", "Synchronet External User Editor", "SYSOP");
// console.uselect(21, "Category", "UIFC library", "");
console.uselect(22, "Category", "User Monitor", "SYSOP");
console.uselect(23, "Category", "Synchronet GUI User Editor", "SYSOP");
console.uselect(24, "Category", "Web Interface", "");
console.uselect(25, "Category", "Web Server", "");
console.uselect(26, "Category", "Win32 Synchronet GUI", "SYSOP");
// console.uselect(27, "Category", "XPDEV library", "");
tmp=console.uselect();
if(tmp == -1)
	exit();
pr.Category=cats[tmp];
cls = new Array('sw-bug','doc-bug','support','change-request');
console.uselect(0, "Category", "Problem requiring a correction to software.", "");
console.uselect(1, "Category", "Problem requiring a correction or improvement in documentation.", "");
console.uselect(2, "Category", "A support problem or question.", "");
console.uselect(3, "Category", "Suggested change in functionality.", "");
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

var hdrs = new Object;
hdrs.to_net_type=NET_INTERNET;
hdrs.to_net_addr='bugs@bbsdev.net';
var body='';
body += ">Submitter-Id:\t" + pr.SubmitterId + "\r\n";
body += ">Originator:\t" + pr.Originator + "\r\n";
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

hdrs.from=user.alias;
hdrs.from_ext=user.number;
hdrs.to='bugs@bbsdev.net';
hdrs.subject=pr.Synopsis;

var msgbase = new MsgBase('mail');
if(msgbase.open!=undefined && msgbase.open()==false) {
	writeln("Cannot send bug report (open error)!");
	exit();
}
if(!msgbase.save_msg(hdrs, client, body)) {
	writeln("Cannot send bug report (save error)!");
	exit();
}
