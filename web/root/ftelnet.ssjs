/* $Id: ftelnet.ssjs,v 1.6 2018/03/18 20:30:47 ree Exp $ */

load("../web/lib/template.ssjs");

var ftelnethelperloaded = false;
try {
  load("ftelnethelper.js");
  ftelnethelperloaded = true;
} catch (e) {
  log(LOG_ERR, e);
}

var sub='';

template.title = system.name + " - fTelnet";

if (!ftelnethelperloaded) {
    templatefile = "ftelnet_disabled.inc";
    if (user.security.level >= 90) {
        template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Check out the latest <b><a href='http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/ftelnethelper.js'>exec/load/ftelnethelper.js</a></b> file from CVS</li></ul>";
    }
} else if (!IsWebSocketServiceEnabled(false) || !IsWebSocketServiceEnabled(true)) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the WS and WSS services<ul><li>To do this, add this block to your <b>sbbs/ctrl/services.ini file<pre>;WebSocket service (for fTelnet loaded via http://).\r\n;For troubleshooting, please see https://www.ftelnet.ca/synchronet/\r\n[WS]\r\nPort=1123\r\nOptions=NO_HOST_LOOKUP\r\nCommand=websocketservice.js\r\n\r\n;WebSocket Secure service (for fTelnet loaded via https://).\r\n;For troubleshooting, please see https://www.ftelnet.ca/synchronet/\r\n[WSS]\r\nPort=11235\r\nOptions=NO_HOST_LOOKUP | TLS\r\nCommand=websocketservice.js</pre></li></ul><strong>NOTE:</strong> Don't forget to open ports 1123 and 11235 on your firewall and/or add the correct port forwarding rules, if necessary.";
	}
} else {
	templatefile = "ftelnet.inc";
	template.HostName = http_request.vhost; // system.host_name;
    template.TelnetPort = GetTelnetPort();
	template.WSPort = GetWebSocketServicePort(false);
	template.WSSPort = GetWebSocketServicePort(true);
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