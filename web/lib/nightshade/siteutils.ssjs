/* $Id: siteutils.ssjs,v 1.2 2006/05/18 18:01:42 rswindell Exp $ */

/* Small utilitiy to pull non-standard ports from */
/*  ini files.  Used for URI's in web interface   */

/*  Currently this relies on file names being   */
/* sbbs.ini and services.ini in system.ctrl_dir */

var portnum="";

template.name_logo=system.name;

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
    template.name_logo='<div id="siteName"><img src="' + template.image_dir + '/logo.gif" style="float: left;" alt="Synchronet" title="Synchronet" /></div>';

/*  System Stats */

    total=time()-system.uptime;
    days   = Math.floor(total/(24*60*60));
    if(days) 
        total%=(24*60*60);
    hours  = Math.floor(total/(60*60));
    min     =(Math.floor(total/60))%60;
    sec=total%60;
    
    template.uptime = format("%u days, %u:%02u:%02u",days,hours,min,sec);
    template.logons_today = system.stats.logons_today;
    template.posted_today = system.stats.messages_posted_today;
    template.uploaded_today = format("%lu bytes in %lu files" ,system.stats.bytes_uploaded_today ,system.stats.files_uploaded_today);
    template.timeon_today = system.stats.timeon_today;
    template.new_users_today = system.stats.new_users_today;
    template.email_sent_today = system.stats.email_sent_today;
    template.downloaded_today = format("%lu bytes in %lu files" ,system.stats.bytes_downloaded_today ,system.stats.files_downloaded_today);
    template.total_timeon = addcommas(system.stats.total_timeon);
    template.total_logons = addcommas(system.stats.total_logons);
    template.total_messages = addcommas(system.stats.total_messages);
    template.total_users = addcommas(system.stats.total_users);
    template.total_email = addcommas(system.stats.total_email);
    template.total_files = addcommas(system.stats.total_files);

	template.additional_services ='[' + ("java telnet".link("telnet/")) + '] ';
    template.additional_services+='[' + ("telnet".link("telnet://"+host +telnet_port)) + '] ';
    template.additional_services+='[' + ("rlogin".link("rlogin://"+host +rlogin_port)) + '] ';
    template.additional_services+='[' + ("ftp".link("ftp://"+host +ftp_port)) + '] ';
    template.additional_services+='[' + ("irc".link("irc://"+host +irc_port)) + '] ';
    template.additional_services+='[' + ("news".link("news://"+host +nntp_port)) + '] ';
    template.additional_services+='[' + ("gopher".link("gopher://"+host +gopher_port)) + '] ';

	function addcommas(num)
{
    num = '' + num;
    if (num.length > 3) {
        var mod = num.length % 3;
        var output = (mod > 0 ? (num.substring(0,mod)) : '');
        for (i=0 ; i < Math.floor(num.length / 3); i++) {
            if ((mod == 0) && (i == 0))
                output += num.substring(mod+ 3 * i, mod + 3 * i + 3);
            else
                output+= ',' + num.substring(mod + 3 * i, mod + 3 * i + 3);
        }
        return (output);
    }
    else
        return num;
}