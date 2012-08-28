// ASCII Art Captcha for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com
// (Carried over from ecWeb v2)

function insertCaptcha() {
	var d = directory("../web/lib/captchaAnsis/*");
	var randomFont = Math.floor(Math.random() * (d.length));
	while(d[randomFont].match(/CVS/) != null) {
		randomFont = Math.floor(Math.random() * (d.length));
	}
	var f = directory(d[randomFont] + "*.a??");
	var captchaString = "";
	print("<div style='background-color:black;height:100px;float:left;'>");
	for(i = 0; i < webIni.captchaLength; i++) {
		var randomLetter = Math.floor(Math.random() * (f.length));
		var g = new File(f[randomLetter]);
		g.open("r");
		var h = g.read();
		g.close();
		h = html_encode(h);
		print("<pre style='font-face:monospace;font-family:courier new,courier,fixedsys,monospace;background-color:black;float:left;padding-right:5px;'>" + h + "</pre>");
		captchaString = captchaString + file_getname(f[randomLetter]).replace(file_getext(f[randomLetter]), "")
	}
	print("</div><br style=clear:both;/><br />");
	print("<input class='border font' type=text size=" + webIni.captchaLength + " name=letters1> Enter the letters shown above (<a class=link href=./lib/captchaLib.ssjs?font=" + randomFont + " target=_blank>Help</a>)<br /><br />");
	print("<input type=hidden name=letters2 value=" + md5_calc(captchaString.toUpperCase(), hex=true) + ">");
}

if(http_request.query.hasOwnProperty("font")) {
	load('webInit.ssjs');
	openPage("Captcha Help"); 
	print("<span class=headingFont>CAPTCHA Help</span><br /><br />");
	print("Having trouble reading the CAPTCHA? Compare what you see in the CAPTCHA box to the letters in the alphabet below.<br />(Note: this CAPTCHA uses letters, not numbers, and is not case sensitive.)<br /><br />");
	var d = directory("../web/lib/captchaAnsis/*");
	if(parseInt(http_request.query.font) < d.length) {
		var f = directory(d[parseInt(http_request.query.font)] + "/*");
		for(g in f) {
			var h = new File(f[g]);
			h.open("r");
			i = h.read();
			h.close();
			i = html_encode(i);
			print("<pre style='font-face:monospace;font-family:courier new,courier,fixedsys,monospace;background-color:black;height:100px;padding-right:5px;float:left;'>" + i + "</pre>");
		}
	}
	closePage();
}
