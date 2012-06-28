/* fTelnet.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* This should configure and embed the fTelnet flash app for most people.
   Should be modified to use exec/load/ftelnethelper.js, but I don't feel
   like doing that today. */

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

print("<p id='ClientContainer' style='text-align: center;'></p>");
print("<script type='text/javascript' src='" + eval(webIni.webUrl) + "fTelnet/ClientFuncs.js'></script>");
print("<script type='text/javascript' src='" + eval(webIni.webUrl) + "fTelnet/swfobject.js'></script>");
print("<script type='text/javascript'>");
print("var ClientVars = { AutoConnect:0, BitsPerSecond:115200, Blink:1, BorderStyle:'FlashTerm', CodePage:'437', Enter:'\\r', FontHeight:16, FontWidth:9, ScreenColumns:80, ScreenRows:25, SendOnConnect:'' };");
print("ClientVars.ConnectAnsi = '" + eval(webIni.webUrl) + "fTelnet/img/sync.an1';"); // You could point this to the location of another background ANSI file.
//print("ClientVars.ServerName='" + system.name + "';"); // No matter what I do, anything with an apostrophe in it here breaks fTelnet, so this is commented out.
print("ClientVars.ServerName='BBS';");
print("ClientVars.SocketPolicyPort = " + fspPort + ";"); // If you're running your Flash Socket Policy server on a nonstandard port, edit this line.
print("ClientVars.TelnetHostName='" + system.inet_addr + "';");
print("ClientVars.TelnetPort=23;"); // If your telnet server is on a nonstandard port, edit this line.
print("swfobject.embedSWF('" + eval(webIni.webUrl) + "fTelnet/fTelnet.swf','ClientContainer',(location.href.indexOf('file:') === 0) ? 740 : '100%',(location.href.indexOf('file:') === 0) ? 442 : '100%','10.0.0','playerProductInstall.swf',ClientVars,{ allowfullscreen: 'true', allowscriptaccess: 'sameDomain', bgcolor: '#ffffff', quality: 'high' },{ align: 'middle', id: 'fTelnet', name: 'fTelnet', swliveconnect: 'true' },function (callbackObj) {if (!callbackObj.success) { } } );");
print("</script>");
