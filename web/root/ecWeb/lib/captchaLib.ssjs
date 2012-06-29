/* captchaLib.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* A cheesy ASCII art CAPTCHA.  You can add your own fonts by placing a sub-
   directory full of .asc or .ans files within ~lib/captchaAnsis/ (one file
   per letter of the alphabet.)

   You can use the insertCaptcha() function to add a CAPTCHA to any form.  Just
   call the function at some point before your closing </form> tag. It will add
   two inputs to the form - one hidden, named 'letters2', which contains the
   md5 sum of the letters of the CAPTCHA, and a text input named 'letters1' to
   take input from the user. When the form is submitted, find the md5 sum of
   'letters1' and compare it to 'letters2' - if they match, the user passed the
   test. */

function insertCaptcha() {
	var d = directory(webIni.webRoot + "/lib/captchaAnsis/*");
	var randomFont;
	while(randomFont==undefined) {
		randomFont = Math.floor(Math.random() * (d.length));
		if(!file_isdir(d[randomFont]))
			randomFont=undefined;
	}
	while(d[randomFont].match(/CVS/) != null) randomFont = Math.floor(Math.random() * (d.length));
	var f = directory(d[randomFont] + "*.a??"); // We're looking for .asc and .ans files.
	var captchaString = "";
	print("<div style='background-color:black;height:100px;float:left;'>");
	for(i = 0; i < webIni.captchaLength; i++) {
		var randomLetter = Math.floor(Math.random() * (f.length));
		var g = new File(f[randomLetter]);
		g.open("r",true);
		var h = g.read();
		g.close();
		h = html_encode(h);
		print("<pre style='font-face:monospace;font-family:courier new,courier,fixedsys,monospace;background-color:black;float:left;padding-right:5px;'>" + h + "</pre>");
		captchaString = captchaString + file_getname(f[randomLetter]).replace(file_getext(f[randomLetter]), "")
	}
	print("</div><br style=clear:both><br>");
	print("<input class='border font' type=text size=" + webIni.captchaLength + " name=letters1> Enter the letters shown above (<a class=link href='./lib/captchaLib.ssjs?font=" + randomFont + "' target=_blank>Help</a>)<br><br>");
	print("<input type=hidden name=letters2 value=" + md5_calc(captchaString.toUpperCase(), hex=true) + ">");
}

if(http_request.query.hasOwnProperty("font")) {
	load('webInit.ssjs');
	load(webIni.webRoot + '/themes/' + webIni.theme + '/layout.ssjs');
	openPage("Captcha Help"); 
	print("<span class=headingFont>CAPTCHA Help</span><br><br>");
	print("Having trouble reading the CAPTCHA? Compare what you see in the CAPTCHA box to the letters in the alphabet below.<br>(Note: this CAPTCHA uses letters, not numbers, and is not case sensitive.)<br><br>");
	var d = directory(webIni.webRoot + "/lib/captchaAnsis/*");
	if(parseInt(http_request.query.font) < d.length) {
		var f = directory(d[parseInt(http_request.query.font)] + "/*");
		for(g in f) {
			if(file_isdir(f[g]))
				continue;
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
