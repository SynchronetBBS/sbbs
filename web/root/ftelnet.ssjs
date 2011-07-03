/* $Id$ */

load("../web/lib/template.ssjs");
load("ftelnethelper.js");

var sub='';

template.title = system.name + " - fTelnet (Flash Telnet)";

if (!IsFlashSocketPolicyServerEnabled()) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the Flash Socket Policy Service<ul><li>To do this, add this block to your <b>sbbs/ctrl/services.ini file<pre>[FlashPolicy]\r\nPort=843\r\nOptions=NO_HOST_LOOKUP\r\nCommand=flashpolicyserver.js</pre></li></ul>";
	}
} else {
	templatefile = "ftelnet.inc";
	template.HostName = system.inet_addr;
	template.Port = GetTelnetPort();
	template.ServerName = system.name;
	template.SocketPolicyPort = GetFlashSocketPolicyServicePort();
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