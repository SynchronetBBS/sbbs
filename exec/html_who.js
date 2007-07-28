// html_nodelist.js

load("nodedefs.js");

var include_age_gender=false;
var include_location=false;
var include_statistics=false;

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

writeln("<font face=Arial,Helvetica,sans-serif>");

font_color = "<font color=black>";

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
	if(system.node_list[n].status==NODE_INUSE) {
		write("<tr>");
		write(format("<td align=right><font size=-1>%d",n+1));
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
		write("\r\n");
	}
}
writeln("</tbody>");
writeln("</table>");
writeln("</font>");
writeln("</font>");
