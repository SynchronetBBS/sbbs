// nodelist-html.js

// Synchronet Service for sending a periodic HTML node list

var refresh_rate=10	/* seconds */
var start=new Date();

load("nodedefs.js");

var include_age_gender=true;
var include_location=false;
var include_statistics=false;

// Parse arguments
for(i=0;i<argc;i++)
	switch(argv[i].toLowerCase()) {
		case "-r":
			refresh_rate=Number(argv[++i]);
			break;
		case "-n":
			include_age_gender=false;
			break;
		case "-l":
			include_location=true;
			break;
		case "-s":	/* statistics */
			include_statistics=true;
			break;
	}

if((this.http_request==undefined) && this.server && this.client) {

	// Write a string to the client socket
	function write(str)
	{
		client.socket.send(str);
	}

	function writeln(str)
	{
		if(str)
			write(str + "\r\n");
		else
			write("\r\n");
	}
}

function xtrn_name(code)
{
	if(this.xtrn_area==undefined)
		return(code);

	for(s in xtrn_area.sec_list)
		for(p in xtrn_area.sec_list[s].prog_list)
			if(xtrn_area.sec_list[s].prog_list[p].code.toLowerCase()==code.toLowerCase())
				return(xtrn_area.sec_list[s].prog_list[p].name);
	return(code);
}

// Get HTTP Request
while(this.client!=undefined && client.socket.data_waiting) {
	request = client.socket.recvline(512 /*maxlen*/, 3 /*timeout*/);

	if(request==null) 
		break;

//	log(format("client request: '%s'",request));
}

if(this.server==undefined) {	/* CGI, so send CGI/HTTP headers */
	writeln("Content-Type: text/html");
	writeln();
}
// HTML Header
writeln("<html>");
writeln("<head>");
writeln(format("<title>%s - Node List</title>",system.name));
writeln(format("<meta http-equiv=refresh content=%d>",refresh_rate));
writeln("</head>");

writeln("<body bgcolor=teal text=white link=yellow vlink=lime alink=white>");
writeln("<font face=Arial,Helvetica,sans-serif>");

// Login Button - Modified by RuneMaster of RuneKeep BBS
writeln("<table border=0 width=100%>");
writeln("<tr>");
writeln("<td align=left>");
writeln(format("<h1>%s - Node List</h1>",system.name.italics()).fontcolor("lime"));
writeln("</td>");
writeln("<td align=right>");
writeln("<form>");
writeln("<input type=button value='Login' onClick='location=\"telnet://"
        + system.host_name + "\";'>");
writeln("</form>");
writeln("</td>");
writeln("</tr>");
writeln("</table>");

font_color = "<font color=black>";

if(include_statistics) {
	total	= time()-system.uptime;
	days	= Math.floor(total/(24*60*60));
	if(days) 
		total%=(24*60*60);
	hours	= Math.floor(total/(60*60));
	min	= (Math.floor(total/60))%60;
	sec	= total%60;

	// Table
	writeln("<table border=1 width=100%>");
	writeln("<td>Up Time<td>" 
		+ format("%u days, %u:%02u:%02u",days,hours,min,sec));
	writeln("<td>Logons Today<td>" + system.stats.logons_today);
	writeln("<td>Posts Today<td>" + system.stats.messages_posted_today);
	writeln("<td>Uploads Today<td>" 
		+ format("%lu bytes in %lu files"
			,system.stats.bytes_uploaded_today
			,system.stats.files_uploaded_today));
	writeln("<tr>");
	writeln("<td>Time-on Today<td>" + system.stats.timeon_today);
	writeln("<td>New Users Today<td>" + system.stats.new_users_today);
	writeln("<td>Emails Today<td>" + system.stats.email_sent_today);
	writeln("<td>Downloads Today<td>" 
		+ format("%lu bytes in %lu files"
			,system.stats.bytes_downloaded_today
			,system.stats.files_downloaded_today));
	writeln("</table>");
	writeln("<br>");
}

// Table
writeln("<table border=0 width=100%>");

// Header
writeln("<thead>");
writeln("<tr bgcolor=white>");

write("<th align=center width=7%>" + font_color + "Node");
write("<th align=center width=20%>" + font_color + "User");
write("<th align=left>" + font_color + "Action/Status");
if(include_location) 
	write("<th align=left>" +font_color+ "Location");
if(include_age_gender) {
	write("<th align=center width=7%>" + font_color + "Age");
	write("<th align=center width=10%>" + font_color + "Gender\r\n");
}
write("<th align=center width=10%>" + font_color + "Time\r\n");
writeln("</thead>");

writeln("<tbody>");
var u = new User(0);
for(n=0;n<system.node_list.length;n++) {
	write("<tr>");
	write(format("<td align=right><font size=-1>%d",n+1));
	if(system.node_list[n].status==NODE_INUSE) {
		u.number=system.node_list[n].useron;
		if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
			action=format("running %s",xtrn_name(u.curxtrn));
		else
			action=format(NodeAction[system.node_list[n].action]
				,system.node_list[n].aux);
		write(format(
			"<td align=center><a href=mailto:%s>%s</a>"
			,u.email
			,u.alias
			));
		write(format(
			"<td><font color=yellow>%s"
			,action
			));
		if(include_location)
			write(format(
				"<td align=left>%s"
				,u.location
				));
		if(include_age_gender) 
			write(format(
				"<td align=center>%d<td align=center>%s"
				,u.age
				,u.gender
				));
		t=time()-u.logontime;
        if(t&0x80000000) t=0;
		write(format(
			"<td align=center>%u:%02u:%02u"
			,Math.floor(t/(60*60))
			,Math.floor(t/60)%60
			,t%60
			));
	} else {
		action=format(NodeStatus[system.node_list[n].status],system.node_list[n].aux);
		write(format("<td><td>%s",action));
	}
	write("\r\n");
}
writeln("</tbody>");
writeln("</table>");

writeln("<p><font color=silver><font size=-2>");
writeln(format("Auto-refresh in %d seconds",refresh_rate));
write(format("<br>Dynamically generated in %lu milliseconds "
	  ,new Date().valueOf()-start.valueOf()));
write("by ");
if(this.server)
	write(server.version + " and ");
write(system.version_notice.link("http://www.synchro.net"));
writeln("<br>" + system.timestr());

writeln("</body>");
writeln("</html>");
/* End of nodelist-html.js */
