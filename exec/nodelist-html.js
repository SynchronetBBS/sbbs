// nodelist-html.js

// Synchronet Service for sending a periodic HTML node list

var refresh_rate=10	/* seconds */
var start=new Date();

load("nodedefs.js");

// Parse arguments
for(i=0;i<argc;i++)
	if(argv[i].toLowerCase()=="-r")
		refresh_rate=Number(argv[++i]);

// Write a string to the client socket
function write(str)
{
	client.socket.send(str);
}

function writeln(str)
{
	write(str + "\r\n");
}

// Get HTTP Request
while(client.socket.data_waiting) {
	request = client.socket.recvline(128 /*maxlen*/, 3 /*timeout*/);

	if(request==null) 
		break;

//	log(format("client request: '%s'",request));
}

// HTML Header
writeln("<html>");
writeln("<head>");
writeln(format("<title>%s BBS - Node List</title>",system.name));
writeln(format("<meta http-equiv=refresh content=%d>",refresh_rate));
writeln("</head>");

writeln("<font face=Arial,Helvetica,sans-serif>");
writeln("<body bgcolor=teal text=white link=yellow vlink=lime alink=white>");

// Login Button
writeln("<form>");
writeln("<table align=right>");
writeln("<td><input type=button value='Login' onClick='location=\"telnet://" 
	+ system.inetaddr + "\";'>"); 
writeln("</table>");
writeln("</form>");

// Table
writeln("<table border=0 width=100%>");
writeln("<caption align=left><font color=lime>");
writeln(format("<h1><i>%s BBS - Node List</i></h1>",system.name));
writeln("</caption>");

// Header
writeln("<thead>");
writeln("<tr bgcolor=white>");
font_color = "<font color=black>";

write(format("<th align=center width=7%>%sNode",font_color));
write(format("<th align=center width=20%>%sUser",font_color));
write(format("<th align=left>%sAction/Status",font_color));
write(format("<th align=center width=7%>%sAge",font_color));
write(format("<th align=center width=10%>%sGender\r\n",font_color));
writeln("</thead>");

writeln("<tbody>");
var user = new User(1);
for(n=0;n<system.node_list.length;n++) {
	write("<tr>");
	write(format("<td align=right><font size=-1>%d",n+1));
	if(system.node_list[n].status==NODE_INUSE) {
		user.number=system.node_list[n].useron;
		if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
			action=format("running external program (door) #%d",system.node_list[n].aux);
		else
			action=format(NodeAction[system.node_list[n].action]
				,system.node_list[n].aux);
		write(format(
			"<td align=center><a href=mailto:%s>%s</a>"
			,user.email
			,user.alias
			));
		write(format(
			"<td><font color=yellow>%s<td align=center>%d<td align=center>%s"
			,action
			,user.age
			,user.gender
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
write("by <a href=http://www.synchro.net>" + system.version_notice + "</a>");
writeln("<br>" + system.timestr());

writeln("</body>");
writeln("</html>");
sleep(1000);
/* End of nodelist-html.js */
