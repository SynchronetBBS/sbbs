/* $Id$ */

/* This is for the Web Interface Only. */
/*  There is no equivalent in Telnet.  */

load("../web/lib/template.ssjs");

var sub = '';

template.title=system.name +" - Edit Your Profile";

template.profile = new Array;

template.profile.push({html: '<table class="newuser" cellpadding="0" cellspacing="2">' });
if(system.newuser_questions & UQ_REALNAME)
	template.profile.push({html: '<tr><td class="newuser" align="right">Real Name:</td><td class="newuser" align="left"><input type="text" name="name" size="25" maxlength="25" value="' + user.name + '" disabled="disabled" /></td></tr>' });
if(system.newuser_questions & UQ_ALIASES)
	template.profile.push({html: '<tr><td class="newuser" align="right">Alias:</td><td class="newuser" align="left"><input type="text" name="alias" size="25" maxlength="25" value="' + user.alias + '" disabled="disabled" /></td></tr>' });
if(system.newuser_questions & UQ_HANDLE)
	template.profile.push({html: '<tr><td class="newuser" align="right">Chat Handle:</td><td class="newuser" align="left"><input type="text" name="handle" size="8" maxlength="8" value="' + user.handle + '" /></td></tr>' });
template.profile.push({html: '<tr><td class="newuser" align="right">E-Mail Address:</td><td class="newuser" align="left"><input type="text" name="email" size="50" maxlength="60" value="' + user.netmail + '" /></td></tr>' });
if(system.newuser_questions & UQ_ADDRESS)
	template.profile.push({html: '<tr><td class="newuser" align="right">Address:</td><td class="newuser" align="left"><input type="text" name="address" size="50" maxlength="50" value="' + user.address + '" /></td></tr>' });
if(system.newuser_questions & UQ_LOCATION)
	template.profile.push({html: '<tr><td class="newuser" align="right">City, State/Prov:</td><td class="newuser" align="left"><input type="text" name="location" size="50" maxlength="50" value="' + user.location + '" /></td></tr>' });
if(system.newuser_questions & UQ_ADDRESS)
	template.profile.push({html: '<tr><td class="newuser" align="right">Zip/Postal Code:</td><td class="newuser" align="left"><input type="text" name="zipcode" size="50" maxlength="50" value="' + user.zipcode + '" /></td></tr>' });
template.profile.push({html: '</table>' });	
	

if(file_exists(prefs_dir +format("%04d.html_prefs",user.number))) {
	prefsfile=new File(prefs_dir + format("%04d.html_prefs",user.number));
	if(prefsfile.open("r",false)) {
				template.user_icq = prefsfile.iniGetValue('Profile', 'ICQ');
				template.user_msn = prefsfile.iniGetValue('Profile', 'MSN');
				template.user_yahoo = prefsfile.iniGetValue('Profile', 'Yahoo');
				template.user_aim = prefsfile.iniGetValue('Profile', 'AIM');
				template.user_homepage = prefsfile.iniGetValue('Profile', 'Homepage');
				template.user_hobbies = prefsfile.iniGetValue('Profile', 'Hobbies');
				template.user_picture = prefsfile.iniGetValue('Profile', 'Picture');
				template.user_avatar = prefsfile.iniGetValue('Profile', 'Avatar');
		prefsfile.close();
	}
}

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");

write_template("editprofile.inc");

write_template("footer.inc");
