/* $Id$ */

load("../web/lib/template.ssjs");
load("ftelnethelper.js");

var sub='';

template.title = system.name + " - External Program Section";
template.html = "";

var webexternals = load("modopts.js", "webexternals");
if (!webexternals.enabled) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the webexternals javascript module<ul><li>To do this, ensure that the <b>enabled=</b> line in the <b>[webexternals]</b> section of <b>sbbs/ctrl/modopts.ini</b> is set to true</pre></li></ul>";
	}
} else if (!IsFlashSocketPolicyServerEnabled()) {
	templatefile = "ftelnet_disabled.inc";
	if (user.security.level >= 90) {
		template.SysOpMessage = "Actually, it looks like you're the SysOp, so here's what you can do to enable it:<br /><ul><li>Enable the Flash Socket Policy Service<ul><li>To do this, add this block to your <b>sbbs/ctrl/services.ini file<pre>[FlashSocketPolicyService]\r\nPort=843\r\nOptions=NO_HOST_LOOKUP\r\nCommand=flashsocketpolicyservice.js</pre></li></ul>";
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
		template.UserName = UsingSecondRLoginName() ? user.alias : user.security.password;
		template.Password = UsingSecondRLoginName() ? user.security.password : user.alias;
		template.HostName = system.inet_addr;
		template.Port = GetRLoginPort();
		template.External = http_request.query.code;
		template.ServerName = system.name;
		template.SocketPolicyPort = GetFlashSocketPolicyServicePort();
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