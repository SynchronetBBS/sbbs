/* $Id$ */

/* 
 * Write new theme file BEFORE loading the template lib so the
 * new theme is used here
 */

prefs_dir=system.data_dir + 'user/';

var sub='';

var sq="'";
var dq='"';
var pl='+';
prefsfile=new File(prefs_dir +format("%04d.html_prefs",user.number));
if(prefsfile.open("r+",false)) {
  ctheme=http_request.query.theme[0];
  ctheme=ctheme.replace(/"/g,dq+pl+sq+dq+sq+pl+dq);   /* "+'"'+" */
  prefsfile.iniSetValue('WebTheme', 'CurrTheme', ctheme);
  prefsfile.close();
} else {
if(prefsfile.open("w+",false)) {
  ctheme=http_request.query.theme[0];
  ctheme=ctheme.replace(/"/g,dq+pl+sq+dq+sq+pl+dq);   /* "+'"'+" */
  prefsfile.iniSetValue('WebTheme', 'CurrTheme', ctheme);
  prefsfile.close();
  }
}

load('../web/lib/template.ssjs');
template.theme=Themes[CurrTheme];

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
write_template("picktheme.inc");
write_template("footer.inc");
