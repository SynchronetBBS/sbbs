/* $Id$ */

/* Small utilitiy to pull non-standard ports from */
/*  ini files.  Used for URI's in web interface   */

/*  Currently this relies on file names being   */
/* sbbs.ini and services.ini in system.ctrl_dir */

var portnum="";

var http_port = 80;
var irc_port = 6667;
var ftp_port = 21;
var nntp_port = 119;
var gopher_port = 70;
var finger_port = 79;
var udp_finger_port = 79;
var telnet_port = 23;
var rlogin_port = 513;
var smtp_port = 25;
var pop3_port = 110;

var file = new File(file_cfgname(system.ctrl_dir, "sbbs.ini"));
 if(file.open("r")) {
 http_port = file.iniGetValue("web","port",portnum);
 ftp_port = file.iniGetValue("ftp","port",portnum);
 telnet_port = file.iniGetValue("bbs","telnetport",portnum);
 rlogin_port = file.iniGetValue("bbs","rloginport",portnum);
 smtp_port = file.iniGetValue("mail","smtpport",portnum);
 pop3_port = file.iniGetValue("bbs","pop3port",portnum);
 file.close();
}
 
var file = new File(file_cfgname(system.ctrl_dir, "services.ini"));
 if(file.open("r")) {
 nntp_port = file.iniGetValue("nntp","port",portnum);
 irc_port = file.iniGetValue("irc","port",portnum);
 gopher_port = file.iniGetValue("gopher","port",portnum);
 finger_port = file.iniGetValue("finger","port",portnum);
 udp_finger_port = file.iniGetValue("udp-finger","port",portnum);
 file.close();
} 

  
 
   