load("html_inc/template.ssjs");

template.theme_list='<select name="theme">';
for(tname in Themes) {
	template.theme_list+='<option value="'+html_encode(tname,true,true,false,false)+'"';
	if(tname==CurrTheme)
		template.theme_list+=' selected=\"selected\"';
	template.theme_list+='>'+html_encode(Themes[tname].desc,true,true,false,false)+'</option>';
}
template.theme_list+='</select>';
write_template("header.inc");
write_template("themes.inc");
write_template("footer.inc");
