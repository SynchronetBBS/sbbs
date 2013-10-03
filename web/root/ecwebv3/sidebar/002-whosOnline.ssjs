var update = 30000; // Milliseconds between updates

load("nodedefs.js");
if(typeof http_request.query.action != "undefined" && http_request.query.action == "show") {

	print("<b>Who's online</b><br><br>");
	print("<table border=0 cellpadding=0 cellspacing=0 class='font'>");
	for(n = 0; n < system.node_list.length; n++) {
		if(typeof system.node_list[n] == "undefined")
			continue;
		print("<tr><td>Node " + (n + 1) + ":&nbsp;</td>");
		if(system.node_list[n].status == 3)
				print(
					format(
						"<td>%s</td></tr><tr><td>&nbsp;</td><td style='font:italic;'>%s</td></tr>",
						system.username(system.node_list[n].useron),
						NodeAction[system.node_list[n].action]
					)
				);
		else if(system.node_list[n].status == 4)
				print("<td style=font-style:italic;>" + NodeStatus[0] + "</td></tr>"); // Pretend to be	WFC if in use but quiet
		else
				print("<td style=font-style:italic;>" + NodeStatus[system.node_list[n].status] + "</td></tr>");
	}
	print("</table>");

} else {

	print("<div id='whosonline'></div>");
	print("<script type='text/javascript'>");
	print("function xhrwo() {");
	print("var XMLReq = new XMLHttpRequest();");
	print("XMLReq.open('GET', './sidebar/002-whosOnline.ssjs?action=show', true);");
	print("XMLReq.send(null);");
	print("XMLReq.onreadystatechange = function() { if(XMLReq.readyState == 4) { document.getElementById('whosonline').innerHTML = XMLReq.responseText; } }");
	print("}");
	print("setInterval('xhrwo()', " + update + ");");
	print("xhrwo();");
	print("</script>");

}