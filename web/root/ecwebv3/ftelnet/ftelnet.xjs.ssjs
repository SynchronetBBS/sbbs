
	var f = new File(system.ctrl_dir + 'services.ini');
	if(f.open("r")) {
		var servicesIni = f.iniGetObject('Socket-Policy');
		f.close();
		var fspPort = servicesIni.Port;
	} else {
		var fspPort = 843;
	}
	f = new File(system.ctrl_dir + 'sbbs.ini');
	if(f.open("r")) {
		var sbbsIni = f.iniGetObject("BBS");
		f.close();
		var rloginPort = sbbsIni.RLoginPort;
		var telnetPort = sbbsIni.TelnetPort;
	} else {
		var rloginPort = 513;
		var telnetPort = 23;
	}
;

writeln((new String("<html>")));

writeln((new String("<head>")));
writeln((new String("<script type=\"text/javascript\">")));
writeln((new String("var ClientVars = {")));
writeln((new String("\tAlertDialogX : 0,")));
writeln((new String("\tAlertDialogY : 0,")));
writeln((new String("\tAutoConnect : 0,")));
writeln((new String("\tBitsPerSecond : 115200,")));
writeln((new String("\tCodePage : \"437\",")));
writeln((new String("\tConnectButtonX : -0.08,")));
writeln((new String("\tConnectButtonY : 0.30,")));
writeln((new String("\tEnter : \"\\r\",")));
writeln((new String("\tFontWidth : 9,")));
writeln((new String("\tFontHeight : 16,")));
writeln((new String("\tLocalEcho : 0,")));
writeln((new String("\tRIP : 0,")));
writeln((new String("\tRIPIconPath : \"/ripicon\",")));
writeln((new String("\tRLogin : 0,")));
write((new String("\tRLoginHostName : \"")));write(system.inet_addr); ;writeln((new String("\",")));
write((new String("\tRLoginPort : ")));write(rloginPort); ;writeln((new String(",")));
writeln((new String("\tRLoginClientUserName : \"\",")));
writeln((new String("\tRLoginServerUserName : \"\",")));
writeln((new String("\tRLoginTerminalType : \"\",")));
writeln((new String("\tScreenColumns : 80,")));
writeln((new String("\tScreenRows : 25,")));
writeln((new String("\tSendOnConnect : \"\",")));
write((new String("\tServerName : \"")));write(system.name); ;writeln((new String("\",")));
write((new String("\tSocketPolicyPort : ")));write(fspPort); ;writeln((new String(",")));
write((new String("\tTelnetHostName : \"")));write(system.inet_addr); ;writeln((new String("\",")));
write((new String("\tTelnetPort : ")));write(telnetPort); ;writeln((new String(",")));
writeln((new String("\tVirtualKeyboardWidth : 1,")));
writeln((new String("\tVT : 0,")));
write((new String("\tWebSocketHostName : \"")));write(system.inet_addr); ;writeln((new String("\",")));
writeln((new String("\tWebSocketPort : 1123")));
writeln((new String("}")));
writeln((new String("</script>")));
writeln((new String("<script type=\"text/javascript\" src=\"./ClientFuncs.js\"></script>")));
writeln((new String("<script type=\"text/javascript\" src=\"./swfobject.js\"></script>")));
writeln((new String("<script type=\"text/javascript\" src=\"./HtmlTerm.compiled.js\"></script>")));
writeln((new String("<script type=\"text/javascript\" src=\"./HtmlTerm.font2.js\"></script>")));
writeln((new String("<script type=\"text/javascript\" src=\"./HtmlTerm.fontamiga.js\"></script>")));
writeln((new String("<script type=\"text/javascript\" src=\"./HtmlTerm.fontatari.js\"></script>")));
writeln((new String("</head>")));

writeln((new String("<body>")));
writeln((new String("<p style=\"text-align: center;\">")));
writeln((new String("<div id=\"ClientContainer\"></div>")));
writeln((new String("</p>")));
writeln((new String("<script type=\"text/javascript\">")));
writeln((new String("\tswfobject.embedSWF(")));
writeln((new String("\t\t\"fTelnet.swf\",")));
writeln((new String("\t\t\"ClientContainer\",")));
writeln((new String("\t\t\"100%\",")));
writeln((new String("\t\t\"100%\",")));
writeln((new String("\t\t\"10.2.0\",")));
writeln((new String("\t\t\"playerProductInstall.swf\",")));
writeln((new String("\t\tClientVars,")));
writeln((new String("\t\t{")));
writeln((new String("\t\t\tallowfullscreen: \"true\",")));
writeln((new String("\t\t\tallowscriptaccess: \"sameDomain\",")));
writeln((new String("\t\t\tbgcolor: \"#ffffff\", quality: \"high\"")));
writeln((new String("\t\t},")));
writeln((new String("\t\t{")));
writeln((new String("\t\t\talign: \"middle\",")));
writeln((new String("\t\t\tid: \"fTelnet\",")));
writeln((new String("\t\t\tname: \"fTelnet\",")));
writeln((new String("\t\t\tswliveconnect: \"true\"")));
writeln((new String("\t\t},")));
writeln((new String("\t\tfunction (callbackObj) {")));
writeln((new String("\t\t\tif (!callbackObj.success) {")));
writeln((new String("\t\t\t\tif (!HtmlTerm.Init(\"ClientContainer\", ClientVars))")));
writeln((new String("\t\t\t\t\talert(\"Sorry, I wasn't able to load either fTelnet or HtmlTerm\\n\\nTry again with Flash 10+ installed (for fTelnet), or with an HTML5 capable browser (for HtmlTerm)\");")));
writeln((new String("\t\t\t}")));
writeln((new String("\t\t}")));
writeln((new String("\t);")));
writeln((new String("</script>")));
writeln((new String("</body>")));

writeln((new String("</html>")));
