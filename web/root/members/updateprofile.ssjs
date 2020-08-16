/* $Id: updateprofile.ssjs,v 1.6 2006/02/25 21:40:35 runemaster Exp $ */

load("../web/lib/template.ssjs");

var is_sysop=false;

if(http_request.header.referer!=undefined) {
var p = http_request.header.referer.indexOf("/members/");
var virtual_referer=http_request.header.referer.slice(p)
}

if(user.number==1 || user.security.level>=90)
	is_sysop=true;

var sub = '';

template.title=system.name + " - Profile Updated";

if(is_sysop) {

	var usr = new User(http_request.query.edituser[0]);

	if(http_request.query["name"]!=usr.name)
		usr.name=http_request.query["name"];
	if(http_request.query["alias"]!=usr.alias)
		usr.alias=http_request.query["alias"];
	if(http_request.query["handle"]!=usr.handle)
	usr.handle=http_request.query["handle"];

if(http_request.query["email"]!=usr.netmail)
	usr.netmail=http_request.query["email"];
	
if(http_request.query["location"]!=usr.location)
	usr.location=http_request.query["location"];
	
if(file_exists(prefs_dir +format("%04d.html_prefs",usr.number))) {
	prefsfile=new File(prefs_dir + format("%04d.html_prefs",usr.number));
	if(prefsfile.open("r+",false)) {
			if(http_request.query["icq"]!=prefsfile.iniGetValue('Profile', 'ICQ'));
				prefsfile.iniSetValue('Profile', 'ICQ', http_request.query["icq"]);
			if(http_request.query["msn"]!=prefsfile.iniGetValue('Profile', 'MSN'));
				prefsfile.iniSetValue('Profile', 'MSN', http_request.query["msn"]);
			if(http_request.query["yahoo"]!=prefsfile.iniGetValue('Profile', 'Yahoo'));
				prefsfile.iniSetValue('Profile', 'Yahoo', http_request.query["yahoo"]);
			if(http_request.query["aim"]!=prefsfile.iniGetValue('Profile', 'AIM'));
				prefsfile.iniSetValue('Profile', 'AIM', http_request.query["aim"]);
			if(http_request.query["homepage"]!=prefsfile.iniGetValue('Profile', 'Homepage'));
				prefsfile.iniSetValue('Profile', 'Homepage', http_request.query["homepage"]);
			if(http_request.query["hobbies"]!=prefsfile.iniGetValue('Profile', 'Hobbies'));
				prefsfile.iniSetValue('Profile', 'Hobbies', http_request.query["hobbies"]);
			if(http_request.query["picture"]!=prefsfile.iniGetValue('Profile', 'Picture'));
				prefsfile.iniSetValue('Profile', 'Picture', http_request.query["picture"]);
			if(http_request.query["avatar"]!=prefsfile.iniGetValue('Profile', 'Avatar'));
				prefsfile.iniSetValue('Profile', 'Avatar', http_request.query["avatar"]);
		prefsfile.close();
	}
}

} else {

	if(http_request.query["handle"]!=user.handle)
		user.handle=http_request.query["handle"];

	if(http_request.query["email"]!=user.netmail)
		user.netmail=http_request.query["email"];
	
	if(http_request.query["location"]!=user.location)
		user.location=http_request.query["location"];
	
	if(file_exists(prefs_dir +format("%04d.html_prefs",user.number))) {
		prefsfile=new File(prefs_dir + format("%04d.html_prefs",user.number));
		if(prefsfile.open("r+",false)) {
				if(http_request.query["icq"]!=prefsfile.iniGetValue('Profile', 'ICQ'));
					prefsfile.iniSetValue('Profile', 'ICQ', http_request.query["icq"]);
				if(http_request.query["msn"]!=prefsfile.iniGetValue('Profile', 'MSN'));
					prefsfile.iniSetValue('Profile', 'MSN', http_request.query["msn"]);
				if(http_request.query["yahoo"]!=prefsfile.iniGetValue('Profile', 'Yahoo'));
					prefsfile.iniSetValue('Profile', 'Yahoo', http_request.query["yahoo"]);
				if(http_request.query["aim"]!=prefsfile.iniGetValue('Profile', 'AIM'));
					prefsfile.iniSetValue('Profile', 'AIM', http_request.query["aim"]);
				if(http_request.query["homepage"]!=prefsfile.iniGetValue('Profile', 'Homepage'));
					prefsfile.iniSetValue('Profile', 'Homepage', http_request.query["homepage"]);
				if(http_request.query["hobbies"]!=prefsfile.iniGetValue('Profile', 'Hobbies'));
					prefsfile.iniSetValue('Profile', 'Hobbies', http_request.query["hobbies"]);
				if(http_request.query["picture"]!=prefsfile.iniGetValue('Profile', 'Picture'));
					prefsfile.iniSetValue('Profile', 'Picture', http_request.query["picture"]);
				if(http_request.query["avatar"]!=prefsfile.iniGetValue('Profile', 'Avatar'));
					prefsfile.iniSetValue('Profile', 'Avatar', http_request.query["avatar"]);
			prefsfile.close();
		}
	}
}

if(virtual_referer=="/members/viewprofile.ssjs")
	template.backurl='<a href="' +virtual_referer+ '">Back</a>';
else
	template.backurl='<a href="/members/info.ssjs">Back</a>';

template.update_message="Profile has been Updated."

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("updateprofile.inc");
if(do_footer)
	write_template("footer.inc");

