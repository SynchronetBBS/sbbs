/* 
 * Write new theme file BEFORE loading the template lib so the
 * new theme is used here
 */

var sq="'";
var dq='"';
var pl='+';
themefile=new File(system.data_dir+'user/'+format("%04d.html_theme",user.number));
themefile.open("w",false);
ctheme=http_request.query.theme[0];
ctheme=ctheme.replace(/"/g,dq+pl+sq+dq+sq+pl+dq);	/* "+'"'+" */
themefile.writeln('CurrTheme="'+ctheme+'";');
themefile.close();

load('html_inc/template.ssjs');
template.theme=Themes[CurrTheme];

write_template("header.inc");
write_template("picktheme.inc");
write_template("footer.inc");
