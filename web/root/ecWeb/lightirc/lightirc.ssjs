/* lightirc.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* Configures and embeds the lightIRC flash app.  Should work for most sysops
   who are running the Synchronet IRCd (or have another IRCd listening at the
   same address as their BBS) and have a Flash Socket Policy server running
   (and configured to allow traffic to port 6667.) */

/* If you're running some other socket policy server on some other port, you
   could comment the following if..else block out and just set var fspPort to
   whichever port you like. */
var f = new File(system.ctrl_dir + 'services.ini');
if(f.open("r",true)) {
	var servicesIni = f.iniGetObject('FlashPolicy');
	f.close();
	if(servicesIni)
		var fspPort = servicesIni.Port;
	else
		fspPort = 843;
} else {
	var fspPort = 843;
}

print('<script type="text/javascript" src="' + eval(webIni.webUrl) + 'lightirc/swfobject.js"></script>');
print("<div id=lightIRC></div>");
print('<script type="text/javascript">');
print("var params = {};");
print("params.host = '" + system.inet_addr + "';");
print("params.policyPort = " + fspPort + ";");
print("params.language = 'en';");
print("params.nickselect = 'yes';");
print("params.nick = '" + user.alias + "';");
print("params.autojoin = '#bbs';"); // You could set a different default channel here.
print("swfobject.embedSWF('" + eval(webIni.webUrl) + "lightirc/lightIRC.swf', 'lightIRC', '720', '420', '9.0.0', null, params, null);"); // 720 x 420 seems to match the fTelnet embed in my browsers. YMMV; edit this line if so.
print("</script>");
