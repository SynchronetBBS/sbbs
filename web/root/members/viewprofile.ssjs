/* $Id: viewprofile.ssjs,v 1.12 2009/02/03 20:30:48 deuce Exp $ */

load("../web/lib/template.ssjs");
load("../web/lib/profile_config.ssjs");

var sub = '';

var is_sysop=false;

if(user.number==1 || user.security.level>=90)
	is_sysop=true;

var usr=new HTML_Profile(http_request.query.showuser[0]);

if(is_sysop)
template.title = system.name + " - View/Edit Profile for: " + usr.alias;
else
template.title = system.name + " - View Profile for: " + usr.alias;

template.profile = new Array;

if(is_sysop) {
	template.profile.push({html: '<h1>Edit/View Profile</h1>' });
	template.profile.push({html: '<p>Personal Information</p>' });
	template.profile.push({html: '<form action="/members/updateprofile.ssjs?edituser=' + u.number + '" method="post">' });
	template.profile.push({html: '<table class="userstats2" cellpadding="0" cellspacing="1">' });
	if(system.newuser_questions & UQ_REALNAME)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Real Name: </td><td class="userstats" align="left"><input type="text" name="name" size="25" maxlength="25" value="' + usr.name + '" /></td></tr>' });
	if(system.newuser_questions & UQ_ALIASES)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Alias: </td><td class="userstats" align="left"><input type="text" name="alias" size="25" maxlength="25" value="' + usr.alias + '" /></td></tr>' });
	if(system.newuser_questions & UQ_HANDLE)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Chat Handle: </td><td class="userstats" align="left"><input type="text" name="handle" size="8" maxlength="8" value="' + usr.handle + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">E-Mail Address: </td><td class="userstats" align="left"><input type="text" name="email" size="50" maxlength="60" value="' + usr.netmail + '" /></td></tr>' });
	if(system.newuser_questions & UQ_ADDRESS)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Address: </td><td class="userstats" align="left"><input type="text" name="address" size="50" maxlength="50" value="' + usr.address + '" /></td></tr>' });
	if(system.newuser_questions & UQ_LOCATION)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">City, State/Prov: </td><td class="userstats" align="left"><input type="text" name="location" size="50" maxlength="50" value="' + usr.location + '" /></td></tr>' });
	if(system.newuser_questions & UQ_ADDRESS)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Zip/Postal Code: </td><td class="userstats" align="left"><input type="text" name="zipcode" size="50" maxlength="50" value="' + usr.zipcode + '" /></td></tr>' });
	if(system.newuser_questions & UQ_PHONE)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Phone: </td><td class="userstats" align="left"><input type="text" name="phone" size="50" maxlength="50" value="' + usr.phone + '" /></td></tr>' });
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Last Connected Via: </td><td class="userstats" align="left"><input type="text" name="phone" size="50" maxlength="50" value="' + usr.connection + '" disabled="disabled" /></td></tr>' });
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Last Logon Date: </td><td class="userstats" align="left"><input type="text" name="phone" size="50" maxlength="50" value="' + usr.logon + '" disabled="disabled" /></td></tr>' });
	template.profile.push({html: '</table>' });
	template.profile.push({html: '<h3>Web Profile</h3>'});
	template.profile.push({html: '<p>Changes here are only visible in the Web Interface.<br /><br /></p>' });
	template.profile.push({html: '<table class="userstats2" cellpadding="0" cellspacing="1">' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">ICQ :</td><td class="userstats" align="left"><input type="text" name="icq" size="10" maxlength="10" value="' + usr.icq + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">MSN :</td><td class="userstats" align="left"><input type="text" name="msn" size="50" maxlength="50" value="' + usr.msn + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Yahoo :</td><td class="userstats" align="left"><input type="text" name="yahoo" size="50" maxlength="50" value="' + usr.yahoo + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">AIM :</td><td class="userstats" align="left"><input type="text" name="aim" size="50" maxlength="50" value="' + usr.aim + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Homepage :</td><td class="userstats" align="left"><input type="text" name="homepage" size="50" maxlength="50" value="' + usr.homepage + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Hobbies :</td><td class="userstats" align="left"><input type="text" size="50" name="hobbies" value="' + usr.hobbies + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Picture (Link): </td><td class="userstats" align="left"><input type="text" size="50" name="picture" value="' + usr.picture + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Avatar (Link): </td><td class="userstats" align="left"><input type="text" size="50" name="avatar" value="' + usr.avatar + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright">&nbsp;</td><td class="userstats"><br /><input type="submit" value="Update Profile" /></td></tr></table></form><br />' });	

} else {

	template.profile.push({html: '<h1>View Profile</h1>' });
	template.profile.push({html: '<table class="userstats2" cellpadding="2" cellspacing="1">' });
	if(system.newuser_questions & UQ_REALNAME)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Real Name: </td><td class="userstats" align="left">' + usr.name + '</td></tr>' });
	if(system.newuser_questions & UQ_ALIASES)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Alias: </td><td class="userstats" align="left">' + usr.alias + '</td></tr>' });
	if(system.newuser_questions & UQ_HANDLE)
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Chat Handle: </td><td class="userstats" align="left">' + usr.handle + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">E-Mail Address: </td><td class="userstats" align="left">' + usr.netmail + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Last Connected Via: </td><td class="userstats" align="left">' + usr.connection + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Last Logon Date: </td><td class="userstats" align="left">' + usr.logon + '</td></tr>' });

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
	template.profile.push({html: '<table class="userstats2" cellpadding="2" cellspacing="1">' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">ICQ: </td><td class="userstats" align="left">' + usr.icq + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">MSN: </td><td class="userstats" align="left">' + usr.msn + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Yahoo: </td><td class="userstats" align="left">' + usr.yahoo + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">AIM: </td><td class="userstats" align="left">' + usr.aim + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Homepage: </td><td class="userstats" align="left">' + usr.homepage + '</td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Hobbies: </td><td class="userstats" align="left">' + usr.hobbies + '</td></tr>' });
	if(usr.picture=='')
	template.profile.push({html: '<tr><td class="userstatsright" align="right" width="60%">Picture (Link): </td><td class="userstats" align="left">' + usr.picture + '</td></tr>' });
	else
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Picture (Link): </td><td class="userstats" align="left"><a href="' + usr.picture + '">View Picture</a></td></tr>' });
	if(usr.avatar=='')
		template.profile.push({html: '<tr><td class="userstatsright" align="right">Avatar (Link): </td><td class="userstats" align="left">' + usr.avatar + '</td></tr>' });
	else
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Avatar (Link): </td><td class="userstats" align="left"><a href="' + usr.avatar + '">View Avatar</a></td></tr>' });
	template.profile.push({html: '</table><br />' });
}

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("profile.inc");
if(do_footer)
	write_template("footer.inc");

