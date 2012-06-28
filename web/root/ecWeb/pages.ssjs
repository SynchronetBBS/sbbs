/* pages.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

// Handles loading and layout of static pages from the 'pages' directory.

load('webInit.ssjs');
load(webIni.webRoot + '/themes/' + webIni.theme + "/layout.ssjs");

if(!http_request.query.hasOwnProperty("page")) {
	openPage("No Page Specified");
	print("No page was specified.");
	closePage();
	exit();
}

openPage(system.name);

var d = directory(webIni.webRoot + "/pages/*");
for(f in d) {
	if(file_isdir(d[f]) || file_getname(d[f]) != http_request.query.page.toString()) continue;
        if(file_getext(d[f]).toUpperCase() == ".SSJS") {
			load(d[f]);
			break;
        }
        if(file_getext(d[f]).toUpperCase() == ".HTML") {
	        var g = new File(d[f]);
			g.open("r",true);
	        h = g.read();
	        g.close();
			print(h);
			break;
        }
        if(file_getext(d[f]).toUpperCase() == ".TXT") {
            var g = new File(d[f]);
            g.open("r",true);
			h = g.read();
			g.close();
//			print(h); // Uncomment this line if you'd rather not have your text files appear within <pre /> elements.
			print("<pre class=textFile>" + h + "</pre>"); // Comment out this line if you don't want your text files to appear within <pre /> elements.
			break;
        }
}

closePage();
