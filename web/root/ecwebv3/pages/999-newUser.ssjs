//HIDDEN:New User
load('../web/lib/captchaLib.ssjs');
print("<span class=title>New User Registration</span><br /><br />");

if(http_request.query.hasOwnProperty('font')) {
	captchaHelp(http_request.query.font);
} else if((user.number==0 || user.alias == webIni.WebGuest) && http_request.query.hasOwnProperty('newuser')) {

	var failString = '';
	var newUserObject = {};

	if(system.newuser_questions&UQ_ALIASES) {
		if(!http_request.query.hasOwnProperty('alias') || http_request.query.alias.toString().length < 1)
			failString = '- Alias not provided<br />';
		else if(system.trashcan('name', http_request.query.alias))
			failString += '- Invalid alias supplied<br />';
		else if(system.matchuser(http_request.query.alias.toString()))
			failString += '- Alias already in use<br />';
		else
			newUserObject.alias = http_request.query.alias.toString();
	}

	if(
		!http_request.query.hasOwnProperty('password1')
		||
		!http_request.query.hasOwnProperty('password2')
		||
		http_request.query.password1.toString().toUpperCase() != http_request.query.password2.toString().toUpperCase()
		|| http_request.query.password1.toString().length < 4
	) {
		failString += '- Invalid or mismatched passwords supplied<br />';
	} else {
		newUserObject.password = http_request.query.password1.toString().toUpperCase();
	}

	if(system.newuser_questions&UQ_REALNAME) {
		if(!http_request.query.hasOwnProperty('realName') || http_request.query.realName.toString().length < 1)
			failString += '- Real name not provided<br />';
		else if(system.trashcan('name', http_request.query.realName))
			failString += '- Invalid real name supplied<br />';
		else if(system.newuser_questions&UQ_DUPREAL && system.matchuser(http_request.query.realName.toString()))
			failString += '- Real name already in use<br />';
		else
			newUserObject.name = http_request.query.realName.toString();
	}

	if(system.newuser_questions&UQ_HANDLE) {
		if(!http_request.query.hasOwnProperty('handle') || http_request.query.handle.toString().length < 1)
			failString += '- Chat handle not provided<br />';
		else if(system.trashcan('name', http_request.query.handle))
			failString += '- Invalid chat handle supplied<br />';
		else if(system.newuser_questions&UQ_DUPHAND && system.matchuser(http_request.query.handle.toString()))
			failString += '- Chat handle already in use<br />';
		else
			newUserObject.handle = http_request.query.handle.toString();
	}

	if(system.newuser_questions&UQ_LOCATION) {
		if(!http_request.query.hasOwnProperty('location') || http_request.query.location.toString().length < 1)
			failString += '- Location not provided<br />';
		else
			newUserObject.location = http_request.query.location.toString();
	}

	if(system.newuser_questions&UQ_ADDRESS) {
		if(!http_request.query.hasOwnProperty('streetAddress') || http_request.query.streetAddress.toString().length < 6)
			failString += '- Address not provided<br />';
		else
			newUserObject.address = http_request.query.streetAddress.toString();
	}

	if(system.newuser_questions&UQ_PHONE) {
		if(!http_request.query.hasOwnProperty('phone') || http_request.query.phone.length < 1)
			failString += '- Phone number not provided<br />';
		else if(system.trashcan('phone', http_request.query.phone))
			failString += '- Invalid phone number suplied<br />';
		else
			newUserObject.phone = http_request.query.phone.toString();
	}

	if(system.newuser_questions&UQ_SEX) {
		if(!http_request.query.hasOwnProperty('sex') || (http_request.query.sex.toString() != 'm' && http_request.query.sex.toString() != 'f'))
			failString += '- Sex not provided (lol)<br />';
		else
			newUserObject.gender = http_request.query.sex.toString().toUpperCase();
	}

	if(system.newuser_questions&UQ_BIRTH) {
		if(!http_request.query.hasOwnProperty('birthDate') || http_request.query.birthDate.toString().match(/\d\d\/\d\d\/\d\d/) == null)
			failString += '- Birthdate not provided<br />';
		else
			newUserObject.birthdate = http_request.query.birthDate.toString();
	}

	if(system.newuser_questions&UQ_COMPANY) {
		if(!http_request.query.hasOwnProperty('company') || http_request.query.company.length < 1)
			failString += '- Company name not provided<br />';
		// Not sure if this value is actually used
	}

	if(system.newuser_questions&UQ_NONETMAIL == 0) {
		if(!http_request.query.hasOwnProperty('netmail') || !http_request.query.netmail.toString().match(/\w+\@\w+/))
			failString += '- Invalid email/netmail address provided<br />';
		else
			newUserObject.netmail = http_request.query.netmail.toString();
	}

	if(!http_request.query.hasOwnProperty('letters1') || !http_request.query.hasOwnProperty('letters2'))
		failString += '- CAPTCHA missing<br />';
	else if(md5_calc(http_request.query.letters1.toString().toUpperCase(), hex=true) != http_request.query.letters2.toString())
		failString += '- CAPTCHA mismatch<br />';

	if(
		system.newuser_password != ""
		&&
		(
			!http_request.query.hasOwnProperty('nup')
			||
			http_request.query.nup.toString().toUpperCase() != system.newuser_password.toUpperCase()
		)
	) {
		failString += '- Incorrect or no newuser password supplied<br />';
	}

	if(failString.length > 0) {
		print("Your registration failed for the following reasons:<br /><br />" + failString);
	} else {
		var makeNewUser = system.new_user(newUserObject.alias);
		for(property in newUserObject) {
			if(property == 'alias')
				continue;
			if(property == 'password') {
				makeNewUser.security.password = newUserObject.password;
				continue;
			}
			makeNewUser[property] = newUserObject[property];
		}
		print("User account created.");
	}

} else if(user.number==0 || user.alias == webIni.WebGuest) {

	print("<form name='newUser' id='newUser' action='./index.xjs?page=999-newUser.ssjs' method='post' onsubmit='return validateNewUserForm()'>");
	print("<input class='border font' type='hidden' name='newuser' value='true' />");
	if(system.newuser_questions&UQ_ALIASES)
		print("Alias:<br /><input class='border font' type='text' size='30' name='alias' id='alias' /> <span id='aliasError'></span><br /><br />");
	print("Password:<br /><input class='border font' type='password' size='30' name='password1' id='password1' /> <span id='password1Error'></span><br /><br />");
	print("Password again:<br /><input class='border font' type='password' size='30' name='password2' id='password2' /> <span id='password2Error'></span><br /><br />");
	if(system.newuser_questions&UQ_REALNAME)
		print("Real Name:<br /><input class='border font' type='text' size='30' name='realName' id='realName' /> <span id='realNameError'></span><br /><br />");
	if(system.newuser_questions&UQ_HANDLE)
		print("Chat Handle:<br /><input class='border font' type='text' size='30' name='handle' id='handle' /> <span id='handleError'></span><br /><br />");
	if(system.newuser_questions&UQ_LOCATION)
		print("Location:<br /><input class='border font' type='text' size='30' name='location' id='location' /> <span id='locationError'></span><br /><br />");
	if(system.newuser_questions&UQ_ADDRESS)
		print("Street Address:<br /><input class='border font' type='text' size='30' name='streetAddress' id='streetAddress' /> <span id='streetAddressError'></span><br /><br />");
	if(system.newuser_questions&UQ_PHONE)
		print("Phone Number:<br /><input class='border font' type='text' size='30' name='phone' id='phone' /> <span id='phoneError'></span><br /><br />");
	if(system.newuser_questions&UQ_SEX)
		print("Sex: <input class='border font' type='radio' name='sex' id='sex' value='m' />M <input class='border font' type='radio' name='sex' id='sex' value='f' />F <span id='sexError'></span><br /><br />"); // lol
	if(system.newuser_questions&UQ_BIRTH)
		print(format("Birthdate (%s):<br /><input class='border font' type='text' size='8' name='birthDate' id='birthDate' /> <span id='birthDateError'></span><br /><br />", (system.settings&SYS_EURODATE) ? "DD/MM/YY" : "MM/DD/YY"));
	if(system.newuser_questions&UQ_COMPANY)
		print("Company:<br /><input class='border font' type='text' size='30' name='company' id='company' /> <span id='companyError'></span><br /><br />");
	if(system.newuser_questions&UQ_NONETMAIL == 0)
		print("Email/Netmail:<br /><input class='border font' type='text' size='30' name='netmail' id='netmail' /> <span id='netmailError'></span><br /><br />");
	print("<div id='captcha'>");
	insertCaptcha();
	print("</div>");
	print("<input class='border font' type='submit' value='Submit' style='clear:both;' /><br />");
	if(system.newuser_password != "")
		print("Please supply the new user password below.<br /><input class='border font' type='password' size='25' name='nup'><br /><br />");
	print("</form>");

} else {

	print("You're already logged in with a valid user account.  At least try logging out first.");

}
