function openPage(pageTitle) {
	// Print the initial HTML tags
	print('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">');
	print("<html>");
	print("<head>");
	print('<meta http-equiv="Content-Type" content="text/html;charset=utf-8" >');
	print('<link rel="stylesheet" type="text/css" href="' + eval(webIni.webUrl) + "/themes/" + webIni.theme + '/style.css">');
	print("<title>" + pageTitle + "</title>");
	print('<script type="text/javascript" src="' + eval(webIni.webUrl) + '/lib/clientLib.js"></script>');
	print("</head>");
	print('<body class="standardFont backdropColor">');
	print('<div style="text-align: center">');

	// Open the container
	print('<div id="container" class="standardBorder standardColor">');

	// Draw the header
	print('<div id="header" class="standardBorder titleFont">');
	print('<div class="standardMargin">');
	print(eval(webIni.headerText));
	print("</div>");
	print("</div>");

	// Draw the sidebar
	print('<div id="sidebar">');
	print('<div style="margin:10px;">'); // Uh oh - a hard-coded value :|
	// Load the sidebar widgets
	var c = 0;
	var d = directory(webIni.webRoot + "/sidebar/*");
	for(var f in d) {
		if(file_isdir(d[f])) continue;
		print('<div class="sidebarBox standardBorder standardPadding underMargin">');
		if(file_getext(d[f]).toUpperCase() == ".SSJS" || file_getext(d[f]).toUpperCase() == ".JS") load(d[f]);
		if(file_getext(d[f]).toUpperCase() == ".TXT" || file_getext(d[f]).toUpperCase() == ".HTML") {
			var handle = new File(d[f]);
			handle.open("r",true);
			var printme = handle.read();
			handle.close();
			if(file_getext(d[f]).toUpperCase() == ".TXT") printme = "<pre>" + printme + "</pre>";
			print(printme);
		}
		c++;
		print("</div>");
	}
	print("</div>");
	print("</div>");
	
	// Main content box opens here
	print('<div id="content">');
	print('<div class="standardMargin">');
}

function closePage() {
	print("</div>");
	print("</div>");
	// Close the main content box

	// Draw the footer
	print('<div id="footer" class="standardBorder">');
	print('<div class="standardMargin">');
	print(eval(webIni.footerText));
	print("</div>");
	print("</div>");

	print("</div>");
	// Close the container

	// Close out the remaining HTML
	print("</div>");
	print("</body>");
	print("</html>");
}
