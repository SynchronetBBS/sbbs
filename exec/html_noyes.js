// $Id: html_noyes.js,v 1.5 2007/07/29 04:53:35 deuce Exp $

load("sbbsdefs.js");
load("text.js");
load("asc2htmlterm.js");

if(user.settings & USER_HTML) {
	var os = bbs.sys_status;
	bbs.sys_status |= SS_PAUSEOFF;
	bbs.sys_status &= ~SS_PAUSEON;

	console.write("\2\2<html><head><title>"+strip_ctrl(console.question)+"</title></head>");
	console.write('<body bgcolor="black" text="#a8a8a8">');
	console.write('&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;<br>&nbsp;<br>');
	console.write('<center><h1>'+asc2htmlterm(console.question,false,true).replace(/(?:&nbsp;)*<br>/g,'').replace(/nowrap/g,'').replace(/(?:&nbsp;)/g,' ')+'</h1></center>');
	console.write('&nbsp;<br>&nbsp;<br>&nbsp;<br>');
	console.write('<table width="100%"><tr><td align="center" width="50%">');
	console.write('<h3><font color="#a8a8a8"><a href="Y">Yes</a></h3>');
	console.write('</td><td align="center" width="50%">');
	console.write('<h2><font color="#a8a8a8"><a href="N">No</a></h2>');
	console.write('</td></tr></table>');
	console.write('</body></html>\2');
	bbs.sys_status=os;
}
else
	console.putmsg("@EXEC:noyesbar@");
