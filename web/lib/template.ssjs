/*****************************************************************************/
/* SSJS templating library													 */
/* %%name%% is replaced with the HTML encoded value of template.name		 */
/* ^^name^^ is replaced with the URI encoded value of template.name			 */
/* @@name@@ is replaced with the value if template.name						 */
/* 																			 */
/* @@name:sname@@ is replaced with the value of template.name.sname			 */
/* (^^ and %% are also supported)											 */
/* 																			 */
/* @@JS:js_expression@@ is replaced with the return value of js_expression 	 */
/* (^^ and %% are also supported)											 */
/* 																			 */
/* <<REPEAT name>>															 */
/* ... @@name:sname@@ ...													 */
/* <<END REPEAT name>>														 */
/* 																			 */
/* Iterates over the array/object template.name and replaces name:sname with */
/* the value of template.name.sname.										 */
/* (^^ and %% are also supported)											 */
/*																			 */
/*****************************************************************************/

/* $Id$ */

template=new Object;
load("html_inc/html_themes.ssjs");

function write_template(filename)  {
	var inc=new File(system.text_dir+"html_templates"+'/'+Themes[CurrTheme].dir+'/'+filename);
	if(!inc.open("r",true,1024)) {
		horrible_error("Cannot open template file "+system.text_dir+"html_templates"+'/'+Themes[CurrTheme].dir+'/'+filename+"!");
	}
	var file='';
	while(! inc.eof)  {
		file=file += inc.read(1024);
	}
	inc.close();
	file=file.replace(/<<REPEAT (.*?)>>([\x00-\xff]*?)<<END REPEAT \1>>/g,
		function(matched, objname, bit, offset, s) {
			var ret='';
			for(obj in template[objname]) {
				ret += parse_regular_bit(bit, objname, template[objname][obj]);
			}
			return(ret);
		});
	file=parse_regular_bit(file, "", template);
	file=file.replace(/\<\!-- Magical Synchronet ([\^%@])-code --\>/g,'$1');
	write(file);
}

function parse_regular_bit(bit, objname, obj) {
	if(objname=="JS")
		return(bit);
	if(objname=='') {
		bit=bit.replace(/([%^@])\1([^^%@\r\n]*?)\:([^:%^@\r\n]*?)\1\1/g,
			function (matched, start, objname, prop, offset, s) {
				var res=matched;
				if(template[objname]!=undefined)
					res=escape_match(start, template[objname][prop]);
				return(res);
			});
		bit=bit.replace(/([%^@])\1([^:^%@\r\n]*?)\1\1/g,
			function (matched, start, exp, offset, s) {
				var res=escape_match(start, template[exp]);
				return(res);
			});
		bit=bit.replace(/([%^@])\1JS:([\x00-\xff]*?)\1\1/g,
			function (matched, start, exp, offset, s) {
				var res=escape_match(start, eval(exp));
				return(res);
			});
		bit=bit.replace(/([%^@])\1(.*?)\1\1/g,'');
	}
	else {
		bit=bit.replace(new RegExp('([%^@])\\1'+regex_escape(objname)+':([^^%@\r\n]*?)\\1\\1',"g"),
			function (matched, start, exp, offset, s) {
				var res=matched;
				if(obj[exp]!=undefined)
					res=escape_match(start, obj[exp]);
				return(res);
			});
	}
	return bit;
}

function escape_match(start, exp, end)  {
	if(exp==undefined)
		exp='';
	exp=exp.toString();
	if(start=="%")
		exp=html_encode(exp,false,false,false,false);
	if(start=="^")
		exp=encodeURIComponent(exp);
	exp=exp.replace(/\@/g,'<!-- Magical Synchronet @-code -->');
	exp=exp.replace(/\^/g,'<!-- Magical Synchronet ^-code -->');
	exp=exp.replace(/\%/g,'<!-- Magical Synchronet %-code -->');
	return(exp);
}

function error(message)  {
	template.title="Error";
	template.err=message;
	write_template("header.inc");
	write_template("error.inc");
	write_template("footer.inc");
	exit();
}

function horrible_error(message) {
	write("<html><head><title>ERROR</error></head><body><p>"+message+"</p></body></html>");
}

function regex_escape(str)
{
	str=str.replace(/([\\\^\$\*\+\?\.\(\)\{\}\[\]])/g,"\\$1");
	return(str);
}
