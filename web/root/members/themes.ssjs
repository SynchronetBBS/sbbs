/* $Id$ */

load("../web/lib/template.ssjs");

var sub='';

template.theme_list='<select name="theme">';
for(tname in Themes) {
	template.theme_list+='<option value="'+html_encode(tname,true,true,false,false)+'"';
	if(tname==CurrTheme)
		template.theme_list+=' selected=\"selected\"';
	template.theme_list+='>'+html_encode(Themes[tname].desc,true,true,false,false)+'</option>';
}
template.theme_list+='</select>';

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load("../web/lib/topnav_html.ssjs");
if(do_leftnav)
	load("../web/lib/leftnav_html.ssjs");
if(do_rightnav)	
	write_template("rightnav.inc");
write_template("themes.inc");
if(do_footer)
	write_template("footer.inc");
