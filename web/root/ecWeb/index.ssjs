/* index.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */
	
/* This placeholder file is equivalent to pages.ssjs?page=000-home.ssjs.  If
   you want to make changes to the 'home' page of your site, make them in
   ~pages/000-home.ssjs rather than editing this file. */

load('webInit.ssjs');
load(webIni.webRoot + '/themes/' + webIni.theme + "/layout.ssjs");

var pageTitle = "Home";
openPage("Home");
load(webIni.webRoot + '/pages/000-home.ssjs');
closePage();