load("../web/lib/template.ssjs");

var sub='';

http_reply.header.pragma='no-cache';
http_reply.header.expires='0';
http_reply.header['cache-control']='must-revalidate';

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");

/* Main Page Stats */

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
    template.total_timeon = system.stats.total_timeon;
    template.total_logons = system.stats.total_logons;
    template.total_messages = system.stats.total_messages;
    template.total_users = system.stats.total_users;
    template.total_email = system.stats.total_email;
    template.total_files = system.stats.total_files;

write_template("main.inc");
write_template("footer.inc");
