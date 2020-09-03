/* $Id: siteutils.ssjs,v 1.14 2006/06/06 19:00:16 runemaster Exp $ */

/* Small utilitiy to pull non-standard ports from */
/*  ini files.  Used for URI's in web interface   */

/*  Currently this relies on file names being   */
/* sbbs.ini and services.ini in system.ctrl_dir */

var portnum="";

template.name_logo='<h1 id="SiteName">' + system.name + '</h1>';

if((host = http_request.vhost)==undefined)
    host = http_request.host;
if(host==undefined || !host.length)
    host = system.host_name;
var port = host.indexOf(':');
if(port>=0)
    host=host.slice(0,port);

var http_port = 80;
var irc_port = 6667;
var ftp_port = 21;
var nntp_port = 119;
var gopher_port = 70;
var finger_port = 79;
var telnet_port = 23;
var rlogin_port = 513;
var smtp_port = 25;
var pop3_port = 110;

var file = new File(file_cfgname(system.ctrl_dir, "sbbs.ini"));
if(file.open("r")) {
    http_port = file.iniGetValue("web","port",http_port);
    ftp_port = file.iniGetValue("ftp","port",ftp_port);
    telnet_port = file.iniGetValue("bbs","telnetport",telnet_port);
    rlogin_port = file.iniGetValue("bbs","rloginport",rlogin_port);
    smtp_port = file.iniGetValue("mail","smtpport",smtp_port);
    pop3_port = file.iniGetValue("bbs","pop3port",pop3_port);
    file.close();
}
 
var file = new File(file_cfgname(system.ctrl_dir, "services.ini"));
if(file.open("r")) {
    nntp_port = file.iniGetValue("nntp","port",nntp_port);
    irc_port = file.iniGetValue("irc","port",irc_port);
    gopher_port = file.iniGetValue("gopher","port",gopher_port);
    finger_port = file.iniGetValue("finger","port",finger_port);
    file.close();
} 

if(this.web_root_dir!=undefined && file_exists(web_root_dir + template.image_dir + "/logo.gif"))
    template.name_logo='<div id="siteLogo"><img src="' + template.image_dir + '/logo.gif" alt="Synchronet" title="Synchronet" /></div>';
