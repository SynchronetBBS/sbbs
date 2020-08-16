/* $Id: index.ssjs,v 1.36 2019/01/01 12:38:40 rswindell Exp $ */

load("../web/lib/template.ssjs");

var sub='';

http_reply.header.pragma='no-cache';
http_reply.header.expires='0';
http_reply.header['cache-control']='must-revalidate';

if(do_header)
    write_template("header.inc");
if(do_topnav)        
    load(topnav_html);
if(do_leftnav)        
    load(leftnav_html);

/* Main Page Stats - might move to global_defs.ssjs */

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

    if((host = http_request.vhost)==undefined)
        host = http_request.host;
    if(host==undefined || !host.length)
        host = system.host_name;
	  
	template.additional_services ='[' + ("web telnet".link("ftelnet.ssjs")) + '] ';
//	template.additional_services+='[' + ("java telnet".link("telnet/")) + '] ';
    template.additional_services+='[' + ("telnet".link("telnet://"+host +telnet_port)) + '] ';
//    template.additional_services+='[' + ("rlogin".link("rlogin://"+host +rlogin_port)) + '] ';
    template.additional_services+='[' + ("ftp".link("ftp://"+host +ftp_port)) + '] ';
//	template.additional_services+='[' + ("java irc".link("irc/")) + '] ';
    template.additional_services+='[' + ("irc".link("irc://"+host +irc_port)) + '] ';
    template.additional_services+='[' + ("news".link("news://"+host +nntp_port)) + '] ';
    template.additional_services+='[' + ("gopher".link("gopher://"+host +gopher_port)) + '] ';

if(do_rightnav)
        write_template("rightnav.inc");
write_template("main.inc");
if(do_footer)    
    write_template("footer.inc");

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
