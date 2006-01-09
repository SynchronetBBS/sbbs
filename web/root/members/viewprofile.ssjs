/* $Id: & */

load("../web/lib/template.ssjs");

var sub = '';

var is_sysop=false;

if(user.number==1 || user.security.level>=90)
	is_sysop=true;

var u = new User(http_request.query.showuser[0]);

usr=new Object;

if(system.newuser_questions & UQ_REALNAME)
	usr.name=u.name.toString();
if(system.newuser_questions & UQ_ALIASES)
	usr.alias=u.alias.toString();
if(system.newuser_questions & UQ_HANDLE)
	usr.handle=u.handle.toString();
if(system.newuser_questions & UQ_LOCATION)	
	usr.location=u.location.toString();
	usr.netmail=u.netmail.toString();
if(system.newuser_questions & UQ_PHONE)
	usr.phone=u.phone.toString();
	usr.connection=u.connection.toString();
	usr.logon=strftime("%b-%d-%y",u.stats.laston_date);
	usr.laston=0-u.stats.laston_date;

if(is_sysop)
template.title = system.name + " - View/Edit Profile for: " + usr.alias;
else
template.title = system.name + " - View Profile for: " + usr.alias;

template.profile = new Array;

if(file_exists(prefs_dir +format("%04d.html_prefs",u.number))) {
	prefsfile=new File(prefs_dir + format("%04d.html_prefs",u.number));
	if(prefsfile.open("r",false)) {
				usr.icq = prefsfile.iniGetValue('Profile', 'ICQ');
				if(usr.icq==undefined)
					usr.icq='';
				usr.msn = prefsfile.iniGetValue('Profile', 'MSN');
				if(usr.msn==undefined)
					usr.msn='';
				usr.yahoo = prefsfile.iniGetValue('Profile', 'Yahoo');
				if(usr.yahoo==undefined)
					usr.yahoo='';
				usr.aim = prefsfile.iniGetValue('Profile', 'AIM');
				if(usr.aim==undefined)
					usr.aim='';
				usr.homepage = prefsfile.iniGetValue('Profile', 'Homepage');
				if(usr.homepage==undefined)
					usr.homepage='';
				usr.hobbies = prefsfile.iniGetValue('Profile', 'Hobbies');
				if(usr.hobbies==undefined)
					usr.hobbies='';
				usr.picture = prefsfile.iniGetValue('Profile', 'Picture');
				if(usr.picture==undefined)
					usr.picture='';
				usr.avatar = prefsfile.iniGetValue('Profile', 'Avatar');
				if(usr.avatar==undefined)
					usr.avatar='';
		prefsfile.close();
	}
}

if(is_sysop) {
	template.profile.push({html: '<h1>Edit/View Profile</h1>' });
	template.profile.push({html: '<p>Personal Information</p>' });
	template.profile.push({html: '<form action="/members/updateprofile.ssjs?edituser=' + u.number + '" method="post">' });
	template.profile.push({html: '<table class="userstats" cellpadding="0" cellspacing="2">' });
	if(system.newuser_questions & UQ_REALNAME)
		template.profile.push({html: '<tr><td class="userstats" align="right">Real Name: </td><td class="userstats" align="left"><input type="text" name="name" size="25" maxlength="25" value="' + usr.name + '" /></td></tr>' });
	if(system.newuser_questions & UQ_ALIASES)
		template.profile.push({html: '<tr><td class="userstats" align="right">Alias: </td><td class="userstats" align="left"><input type="text" name="alias" size="25" maxlength="25" value="' + usr.alias + '" /></td></tr>' });
	if(system.newuser_questions & UQ_HANDLE)
		template.profile.push({html: '<tr><td class="userstats" align="right">Chat Handle: </td><td class="userstats" align="left"><input type="text" name="handle" size="8" maxlength="8" value="' + usr.handle + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">E-Mail Address: </td><td class="userstats" align="left"><input type="text" name="email" size="50" maxlength="60" value="' + usr.netmail + '" /></td></tr>' });
	if(system.newuser_questions & UQ_ADDRESS)
		template.profile.push({html: '<tr><td class="userstats" align="right">Address: </td><td class="userstats" align="left"><input type="text" name="address" size="50" maxlength="50" value="' + usr.address + '" /></td></tr>' });
	if(system.newuser_questions & UQ_LOCATION)
		template.profile.push({html: '<tr><td class="userstats" align="right">City, State/Prov: </td><td class="userstats" align="left"><input type="text" name="location" size="50" maxlength="50" value="' + usr.location + '" /></td></tr>' });
	if(system.newuser_questions & UQ_ADDRESS)
		template.profile.push({html: '<tr><td class="userstats" align="right">Zip/Postal Code: </td><td class="userstats" align="left"><input type="text" name="zipcode" size="50" maxlength="50" value="' + usr.zipcode + '" /></td></tr>' });
	if(system.newuser_questions & UQ_PHONE)
		template.profile.push({html: '<tr><td class="userstats" align="right">Phone: </td><td class="userstats" align="left"><input type="text" name="phone" size="50" maxlength="50" value="' + usr.phonee + '" /></td></tr>' });
		template.profile.push({html: '<tr><td class="userstats" align="right">Last Connected Via: </td><td class="userstats" align="left"><input type="text" name="phone" size="50" maxlength="50" value="' + usr.connection + '" disabled="disabled" /></td></tr>' });
		template.profile.push({html: '<tr><td class="userstats" align="right">Last Logon Date: </td><td class="userstats" align="left"><input type="text" name="phone" size="50" maxlength="50" value="' + usr.logon + '" disabled="disabled" /></td></tr>' });
	template.profile.push({html: '</table>' });
	template.profile.push({html: '<h3>Web Profile</h3>'});
	template.profile.push({html: '<p>Changes here are only visible in the Web Interface.</p>' });
	template.profile.push({html: '<table class="userstats" cellpadding="0" cellspacing="2">' });
	template.profile.push({html: '<tr><td class="userstats" align="right">ICQ :</td><td class="userstats" align="left"><input type="text" name="icq" size="10" maxlength="10" value="' + usr.icq + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">MSN :</td><td class="userstats" align="left"><input type="text" name="msn" size="50" maxlength="50" value="' + usr.msn + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Yahoo :</td><td class="userstats" align="left"><input type="text" name="yahoo" size="50" maxlength="50" value="' + usr.yahoo + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">AIM :</td><td class="userstats" align="left"><input type="text" name="aim" size="50" maxlength="50" value="' + usr.aim + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Homepage :</td><td class="userstats" align="left"><input type="text" name="homepage" size="50" maxlength="50" value="' + usr.homepage + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Hobbies :</td><td class="userstats" align="left"><input type="textarea" size="50" name="hobbies" value="' + usr.hobbies + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Picture (Link): </td><td class="userstats" align="left"><input type="textarea" size="50" name="picture" value="' + usr.picture + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Avatar (Link): </td><td class="userstats" align="left"><input type="textarea" size="50" name="avatar" value="' + usr.avatar + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats">&nbsp;</td><td class="userstats"><br /><input type="submit" value="Update Profile" /></td></tr></table></form><br />' });	

} else {

	template.profile.push({html: '<h1>View Profile</h1>' });
	template.profile.push({html: '<table class="userstats" cellpadding="0" cellspacing="2">' });
	if(system.newuser_questions & UQ_REALNAME)
		template.profile.push({html: '<tr><td class="userstats" align="right">Real Name: </td><td class="userstats" align="left">' + usr.name + '</td></tr>' });
	if(system.newuser_questions & UQ_ALIASES)
		template.profile.push({html: '<tr><td class="userstats" align="right">Alias: </td><td class="userstats" align="left">' + usr.alias + '</td></tr>' });
	if(system.newuser_questions & UQ_HANDLE)
		template.profile.push({html: '<tr><td class="userstats" align="right">Chat Handle: </td><td class="userstats" align="left">' + usr.handle + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">E-Mail Address: </td><td class="userstats" align="left">' + usr.netmail + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Last Connected Via: </td><td class="userstats" align="left">' + usr.connection + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Last Logon Date: </td><td class="userstats" align="left">' + usr.logon + '</td></tr>' });

/*  Removed for Privacy Reasons
*  Insert into your site at your own risk of liability!
*
*	if(system.newuser_questions & UQ_ADDRESS)
*		template.profile.push({html: '<tr><td class="userstats" align="right">Address: </td><td class="userstats" align="left">' + usr.address + '</td></tr>' });
*	if(system.newuser_questions & UQ_LOCATION)
*		template.profile.push({html: '<tr><td class="userstats" align="right">City, State/Prov: </td><td class="userstats" align="left">' + usr.location + '</td></tr>' });
*	if(system.newuser_questions & UQ_ADDRESS)
*		template.profile.push({html: '<tr><td class="userstats" align="right">Zip/Postal Code: </td><td class="userstats" align="left">' + usr.zipcode + '</td></tr>' });
*		
*/
		
	template.profile.push({html: '</table>' });

	template.profile.push({html: '<h3>Web Profile</h3>'});
	template.profile.push({html: '<table class="userstats" cellpadding="0" cellspacing="2">' });
	template.profile.push({html: '<tr><td class="userstats" align="right">ICQ: </td><td class="userstats" align="left">' + usr.icq + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">MSN: </td><td class="userstats" align="left">' + usr.msn + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Yahoo: </td><td class="userstats" align="left">' + usr.yahoo + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">AIM: </td><td class="userstats" align="left">' + usr.aim + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Homepage: </td><td class="userstats" align="left">' + usr.homepage + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Hobbies: </td><td class="userstats" align="left">' + usr.hobbies + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Picture (Link): </td><td class="userstats" align="left">' + usr.picture + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstats" align="right">Avatar (Link): </td><td class="userstats" align="left">' + usr.avatar + '</td></tr>' });
	template.profile.push({html: '</table><br />' });
}

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");

write_template("profile.inc");
write_template("footer.inc");
