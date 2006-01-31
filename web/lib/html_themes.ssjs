/* $Id$ */

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
template.image_dir = Themes[CurrTheme].image_dir;

if(http_request.query.force_ssjs_theme != undefined) {
    if(http_request.query.force_ssjs_theme[0] != undefined && Themes[http_request.query.force_ssjs_theme[0]] != undefined)
        CurrTheme=http_request.query.force_ssjs_theme[0];
}
