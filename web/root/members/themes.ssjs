/* $Id: themes.ssjs,v 1.9 2006/02/25 21:40:35 runemaster Exp $ */

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
	load(topnav_html);
if(do_leftnav)
	load(leftnav_html);
if(do_rightnav)	
	write_template("rightnav.inc");
write_template("themes.inc");
if(do_footer)
	write_template("footer.inc");
