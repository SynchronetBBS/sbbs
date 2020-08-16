/* $Id: html_themes.ssjs,v 1.15 2006/02/25 21:39:34 runemaster Exp $ */

/* List of HTML theme packs installed */

Themes=new Object;

load('../web/templates/html_themes.ssjs');

prefs_dir=system.data_dir + 'user/';

var CurrTheme=DefaultTheme;

var do_header = true;
var do_topnav = true;
var do_leftnav = true;
var do_rightnav = true;
var do_footer = true;
var do_extra = false;

/* Read in current users selected theme if it exists */
if(file_exists(prefs_dir +format("%04d.html_prefs",user.number))) {
  prefsfile=new File(prefs_dir +format("%04d.html_prefs",user.number));
  if(prefsfile.open("r",false)) {
	  CurrTheme=prefsfile.iniGetValue('WebTheme', 'CurrTheme');
  prefsfile.close();
  }
}

if(Themes[CurrTheme] == undefined || Themes[CurrTheme].theme_dir == undefined)
	CurrTheme=DefaultTheme;

do_header = Themes[CurrTheme].do_header;
do_topnav = Themes[CurrTheme].do_topnav;
do_leftnav = Themes[CurrTheme].do_leftnav;
do_rightnav = Themes[CurrTheme].do_rightnav;
do_footer = Themes[CurrTheme].do_footer;
do_extra = Themes[CurrTheme].do_extra;
template.image_dir = Themes[CurrTheme].image_dir;
var lib_dir = Themes[CurrTheme].lib_dir;

/* Auto discovery of Theme Specific lib files */

if(file_exists(lib_dir + '/global_defs.ssjs'))
	var global_defs = lib_dir + '/global_defs.ssjs';
else
	var global_defs = '../web/lib/global_defs.ssjs';

if(file_exists(lib_dir + '/leftnav_html.ssjs'))
	var leftnav_html = lib_dir + '/leftnav_html.ssjs';
else
	var leftnav_html = '../web/lib/leftnav_html.ssjs';

if(file_exists(lib_dir + '/leftnav_nodelist.ssjs'))
	var leftnav_nodelist = lib_dir + '/leftnav_nodelist.ssjs';
else
	var leftnav_nodelist = '../web/lib/leftnav_nodelist.ssjs';

if(file_exists(lib_dir + '/main_nodelist.ssjs'))
	var main_nodelist = lib_dir + '/main_nodelist.ssjs';
else
	var main_nodelist = '../web/lib/main_nodelist.ssjs';

if(file_exists(lib_dir + '/msgsconfig.ssjs'))
	var msgsconfig = lib_dir + '/msgsconfig.ssjs';
else
	var msgsconfig = '../web/lib/msgsconfig.ssjs';

if(file_exists(lib_dir + '/siteutils.ssjs'))
	var siteutils = lib_dir + '/siteutils.ssjs';
else
	var siteutils = '../web/lib/siteutils.ssjs';

if(file_exists(lib_dir + '/topnav_html.ssjs'))
	var topnav_html = lib_dir + '/topnav_html.ssjs';
else
	var topnav_html = '../web/lib/topnav_html.ssjs';


if(http_request.query.force_ssjs_theme != undefined) {
    if(http_request.query.force_ssjs_theme[0] != undefined && Themes[http_request.query.force_ssjs_theme[0]] != undefined)
        CurrTheme=http_request.query.force_ssjs_theme[0];
}
