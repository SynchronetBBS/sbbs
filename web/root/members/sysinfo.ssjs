load("html_inc/template.ssjs");

template.title="System Information";

template.sysinfo="";
f=new File(system.text_dir+"system.msg");
if(f.open("rb",true)) {
	template.sysinfo=f.readAll().join('\r\n');
	template.sysinfo=html_encode(template.sysinfo,true,true,true,true);
}

template.logon="";
f=new File(system.text_dir+"menu/logon.ans");
if(f.open("r",true))
	template.logon=f.readAll().join('\r\n');
else {
	f=new File(system.text_dir+"menu/logon.asc");
	if(f.open("r",true))
		template.logon=f.readAll().join('\r\n');
}
template.logon=html_encode(template.logon,true,false,true,true);

write_template("header.inc");
write_template("sysinfo.inc");
write_template("footer.inc");
