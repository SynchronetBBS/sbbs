<!-- $Id: editprofile.inc,v 1.1 2006/02/22 02:16:17 runemaster Exp $ -->

<!-- Main Content -->  


<div id="contentcontainer">
	<div id="content2">
    
	<div align="center">  	


<h1>Edit Profile</h1>
<p>Personal Information
<form action="/members/updateprofile.ssjs?edituser=' + user.number + '" method="post">
<table class="userstats2" cellpadding="2" cellspacing="1">
@@JS:if(system.newuser_questions & UQ_REALNAME)'<tr><td class="userstats" align="right">Real Name: </td><td class="userstats" align="left"><input type="text" name="name" size="25" maxlength="25" value="@@usr_name@@" @@disabled@@ /></td></tr>';@@
@@JS:if(system.newuser_questions & UQ_ALIASES)'<tr><td class="userstatsright" align="right">Alias: </td><td class="userstats" align="left"><input type="text" name="alias" size="25" maxlength="25" value="' + usr.alias@@" @@disabled@@ /></td></tr>';@@
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
	template.profile.push({html: '</table>' });
	template.profile.push({html: '<h3>Web Profile</h3>'});
	template.profile.push({html: '<p>Changes here are only visible in the Web Interface.<br /><br /></p>' });
	template.profile.push({html: '<table class="userstats2" cellpadding="2" cellspacing="1">' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">ICQ: </td><td class="userstats" align="left"><input type="text" name="icq" size="10" maxlength="10" value="' + usr.icq + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">MSN: </td><td class="userstats" align="left"><input type="text" name="msn" size="50" maxlength="50" value="' + usr.msn + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Yahoo: </td><td class="userstats" align="left"><input type="text" name="yahoo" size="50" maxlength="50" value="' + usr.yahoo + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">AIM: </td><td class="userstats" align="left"><input type="text" name="aim" size="50" maxlength="50" value="' + usr.aim + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Homepage: </td><td class="userstats" align="left"><input type="text" name="homepage" size="50" maxlength="50" value="' + usr.homepage + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Hobbies: </td><td class="userstats" align="left"><input type="text" size="50" name="hobbies" value="' + usr.hobbies + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Picture (Link): </td><td class="userstats" align="left"><input type="text" size="50" name="picture" value="' + usr.picture + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstatsright" align="right">Avatar (Link): </td><td class="userstats" align="left"><input type="text" size="50" name="avatar" value="' + usr.avatar + '" /></td></tr>' });
	template.profile.push({html: '<tr><td class="userstats">&nbsp;</td><td class="userstats"><br /><input type="submit" value="Update Profile" /></td></tr></table></form><br />' });


		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

    <!-- end Main Content -->
    
