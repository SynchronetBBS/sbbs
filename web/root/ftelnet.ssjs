/* $Id$ */

load("../web/lib/template.ssjs");

var ftelnethelperloaded = false;
try {
  load("ftelnethelper.js");
  ftelnethelperloaded = true;
} catch (e) {
  // Ignore, we'll display an error below
}

var sub='';

template.title = system.name + " - fTelnet";

if (!ftelnethelperloaded) {
    templatefile = "ftelnet_disabled.inc";
    if (user.security.level >= 90) {
        template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Check out the latest <b><a href='http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/exec/load/ftelnethelper.js'>exec/load/ftelnethelper.js</a></b> file from CVS</li></ul>";
    }
} else if (!IsWebSocketToTelnetServiceEnabled()) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the WebSocket to Telnet Proxy Service<ul><li>To do this, add this block to your <b>sbbs/ctrl/services.ini file<pre>[WebSocket-Telnet]\r\nPort=1123\r\nOptions=NO_HOST_LOOKUP\r\nCommand=websocketservice.js localhost " + GetTelnetPort() + "</pre></li></ul><strong>NOTE:</strong> You may need to tweak the Command= line if your server is listening on a specific IP address (ie change 'localhost' to the correct IP).";
	}
} else {
	templatefile = "ftelnet.inc";
	template.HostName = system.host_name;
	template.Port = GetWebSocketToTelnetPort();
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