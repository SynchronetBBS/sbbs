/* $Id: externals.ssjs,v 1.8 2018/03/18 20:30:49 ree Exp $ */

load("../web/lib/template.ssjs");

var sub='';

template.title = system.name + " - External Program Section";
template.html = "";

var options = null;
try {
  load("ftelnethelper.js");
  options = load("modopts.js", "logon");
} catch (e) {
  log(LOG_ERR, e);
}

if (!options) {
    templatefile = "ftelnet_disabled.inc";
    if (user.security.level >= 90) {
        template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Check out the latest <b><a href='http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/ftelnethelper.js'>exec/load/ftelnethelper.js</a></b>, <b><a href='http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/modopts.js'>exec/load/modopts.js</a></b> and <b><a href='http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/ctrl/modopts.ini'>ctrl/modopts.ini</a></b> files from CVS</li></ul>";
    }
} else if (!options.rlogin_auto_xtrn) {
    templatefile = "ftelnet_disabled.inc";
    if (user.security.level >= 90) {
        template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the rlogin_auto_xtrn feature of the logon module<ul><li>To do this, ensure that the <b>rlogin_auto_xtrn=</b> line in the <b>[logon]</b> section of <b>sbbs/ctrl/modopts.ini</b> is set to <b>true</b></li><li>(Currently, it's set to <b>" + options.rlogin_auto_xtrn + "</b>)</ul>";
    }
} else if (!IsWebSocketServiceEnabled(false) || !IsWebSocketServiceEnabled(true)) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the WS and WSS services<ul><li>To do this, add this block to your <b>sbbs/ctrl/services.ini file<pre>;WebSocket service (for fTelnet loaded via http://).\r\n;For troubleshooting, please see https://www.ftelnet.ca/synchronet/\r\n[WS]\r\nPort=1123\r\nOptions=NO_HOST_LOOKUP\r\nCommand=websocketservice.js\r\n\r\n;WebSocket Secure service (for fTelnet loaded via https://).\r\n;For troubleshooting, please see https://www.ftelnet.ca/synchronet/\r\n[WSS]\r\nPort=11235\r\nOptions=NO_HOST_LOOKUP | TLS\r\nCommand=websocketservice.js</pre></li></ul><strong>NOTE:</strong> Don't forget to open ports 1123 and 11235 on your firewall and/or add the correct port forwarding rules, if necessary.";
	}
} else if (!IsRLoginEnabled()) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Allow RLogin connections<ul><li>To do this, ensure that the <b>Options=</b> line in the <b>[BBS]</b> section of <b>sbbs/ctrl/sbbs.ini</b> includes the <b>ALLOW_RLOGIN</b> option.</li><li>Or, if you're running Windows, open the Synchronet Control Panel and click <b>Terminal -> Configure -> RLogin -> Enabled</b> and then click <b>OK</b>.</li></ul></li></ul>";
	}
} else {
	if (http_request.query.code === undefined) {
		templatefile = "externals.inc";

		for(s in xtrn_area.sec_list) {
			var section = xtrn_area.sec_list[s];
			template.html += "<h3>" + section.name + "</h3><ol class='externals'>";
			
			for (p in section.prog_list) {
			  var program = section.prog_list[p];
			  
			  template.html += "<li>";
			  if (program.can_run) template.html += "<a href='externals.ssjs?code=" + program.code.replace(/[']/i, "&apos;") + "'>";
			  template.html += program.name;
			  if (program.can_run) template.html += "</a>";
			  template.html += "</li>";
			}
			
			template.html += "</ol>";
		}
	} else {
		templatefile = "ftelnet_external.inc";
		template.ClientUserName = user.security.password;
		template.ServerUserName = user.alias;
		template.TerminalType = "xtrn=" + http_request.query.code;
		template.HostName = http_request.vhost; // system.host_name;
        template.RLoginPort = GetRLoginPort();
        template.WSPort = GetWebSocketServicePort(false);
        template.WSSPort = GetWebSocketServicePort(true);
	}
}

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
	load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template(templatefile);
if(do_footer)
	write_template("footer.inc");