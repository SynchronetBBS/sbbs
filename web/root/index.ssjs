load("../web/lib/template.ssjs");

load("sbbsdefs.js");

var sub='';
var start=new Date();
load("nodedefs.js");

var include_statistics=true;

write_template("header.inc");
write_template("topnav.inc");
write_template("leftnav.inc");
write_template("main.inc");

if(include_statistics) {
    total=time()-system.uptime;
	days   = Math.floor(total/(24*60*60));
	if(days) 
		total%=(24*60*60);
	hours  = Math.floor(total/(60*60));
	min     =(Math.floor(total/60))%60;
    sec=total%60;

    // Table
	writeln("<table class=\"main_stats\">");
	writeln("<tr>");
    writeln("<td class=\"main_stats\">Up Time</td><td class=\"main_stats_bold\">" 
		+ format("%u days, %u:%02u:%02u",days,hours,min,sec) + "</td>");
	writeln("<td class=\"main_stats\">Logons</td><td class=\"main_stats_bold\">" + system.stats.logons_today + "</td>");
	writeln("<td class=\"main_stats\">Posts</td><td class=\"main_stats_bold\">" + system.stats.messages_posted_today + "</td>");
	writeln("<td class=\"main_stats\">Uploads</td><td class=\"main_stats_bold\">" 
		+ format("%lu bytes in %lu files"
			,system.stats.bytes_uploaded_today
			,system.stats.files_uploaded_today) + "</td>");
	writeln("</tr>");
	writeln("<tr>");
	writeln("<td class=\"main_stats\">Time-on</td><td class=\"main_stats_bold\">" + system.stats.timeon_today + "</td>");
	writeln("<td class=\"main_stats\">New Users</td><td class=\"main_stats_bold\">" + system.stats.new_users_today + "</td>");
	writeln("<td class=\"main_stats\">Emails</td><td class=\"main_stats_bold\">" + system.stats.email_sent_today + "</td>");
	writeln("<td class=\"main_stats\">Downloads</td><td class=\"main_stats_bold\">" 
		+ format("%lu bytes in %lu files"
			,system.stats.bytes_downloaded_today
			,system.stats.files_downloaded_today) + "</td>");
	
    writeln("</tr>");
    writeln("</table><br />");
   writeln("</td></tr><tr>");
    writeln("<td align=\"center\">All Time<br /><br />");
    
    writeln("<table class=\"main_stats\">");
    writeln("<tr>");
    writeln("<td class=\"main_stats\">Time-on</td><td class=\"main_stats_bold\">" + system.stats.total_timeon + "</td>");writeln("<td class=\"main_stats\">Logons</td><td class=\"main_stats_bold\">" + system.stats.total_logons + "</td>");
    writeln("<td class=\"main_stats\">Messages</td><td class=\"main_stats_bold\">" + system.stats.total_messages + "</td>");
    writeln("</tr>");
    writeln("<tr>");
    writeln("<td class=\"main_stats\">Users</td><td class=\"main_stats_bold\">" + system.stats.total_users + "</td>");
    writeln("<td class=\"main_stats\">Emails</td><td class=\"main_stats_bold\">" + system.stats.total_email + "</td>");
    writeln("<td class=\"main_stats\">Files</td><td class=\"main_stats_bold\">"  + system.stats.total_files + "</td>");
    writeln("</tr>");
    writeln("</table>");
    writeln("</td>");
    writeln("</tr>");
	writeln("</table>");
	writeln("<br />");
}

write_template("footer.inc");
