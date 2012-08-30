// ASCII Art Captcha for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com
// (Carried over from ecWeb v2)
load('webInit.ssjs');

function insertCaptcha() {
	var d = directory("../web/lib/captchaAnsis/*");
	var randomFont = Math.floor(Math.random() * (d.length));
	while(d[randomFont].match(/CVS/) != null) {
		randomFont = Math.floor(Math.random() * (d.length));
	}
	var f = directory(d[randomFont] + "*.a??");
	var captchaString = "";
	var out = "<div style='background-color:black;height:100px;float:left;'>";
	for(i = 0; i < webIni.captchaLength; i++) {
		var randomLetter = Math.floor(Math.random() * (f.length));
		var g = new File(f[randomLetter]);
		g.open("r");
		var h = g.read();
		g.close();
		out += "<pre style='font-face:monospace;font-family:courier new,courier,fixedsys,monospace;background-color:black;float:left;padding-right:5px;'>";
		out += html_encode(h);
		out += "</pre>";
		captchaString = captchaString + file_getname(f[randomLetter]).replace(file_getext(f[randomLetter]), "")
	}
	out += "</div>";

	out += "<div id='captchaHelp' style='clear:both;display:none;'><span class=headingFont>CAPTCHA Help</span><br /><br />";
	out += "Having trouble reading the CAPTCHA? Compare what you see in the CAPTCHA box to the letters in the alphabet below.<br />(Note: this CAPTCHA uses letters, not numbers, and is not case sensitive.)<br /><br />";
	var d = directory("../web/lib/captchaAnsis/*");
	var f = directory(d[randomFont] + "/*");
	for(g in f) {
		var h = new File(f[g]);
		h.open("r");
		i = h.read();
		h.close();
		i = html_encode(i);
		out += "<pre style=\'font-face:monospace;font-family:courier new,courier,fixedsys,monospace;background-color:black;height:100px;padding-right:5px;float:left;\'>" + i + "</pre>";
	}
	out += "</div><br style='clear:both;' /><br />";
	out += "<input class='border font' type='text' size=" + webIni.captchaLength + " name='letters1'> Enter the letters shown above (<a onclick='toggleVisibility(\"captchaHelp\")'>Help</a>)<br /><br />";
	
	// Low risk, but this should probably be written to a file somewhere instead. -ec
	out += "<input type='hidden' name='letters2' value='" + md5_calc(captchaString.toUpperCase(), hex=true) + "' />";
	print(out);
	return randomFont;
}