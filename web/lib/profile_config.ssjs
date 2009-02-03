if(js.global.UQ_REALNAME===undefined)
	load("sbbsdefs.js");
if(js.global.prefs_dir===undefined)
	load("../web/lib/global_defs.ssjs");

function HTML_Profile(user_num)
{
	var html_profile_fields = [
		{
			iniKey:'ICQ',		// REQUIRED - Key in INI file
			property:'icq',		// OPTIONAL - Property name in object (for backwards compat)
			displayName:'ICQ',	// OPTIONAL - Name to display (defaults to iniKey)
			defaultValue:'',	// OPTIONAL - Default value (defaults to '')
		},
		{
			property:'msn',
			iniKey:'MSN',
		},
		{
			property:'yahoo',
			iniKey:'Yahoo',
		},
		{
			property:'aim',
			iniKey:'AIM',
		},
		{
			property:'homepage',
			iniKey:'Homepage',
		},
		{
			property:'hobbies',
			iniKey:'Hobbies',
		},
		{
			property:'picture',
			iniKey:'Picture',
			displayName:'Picture (Link)',
		},
		{
			property:'avatar',
			iniKey:'Avatar',
			displayName:'Avatar (Link)',
		},
	];
	var u,i,o,prefsfile;

	if(user_num === undefined)
		u=user;
	else
		u=new User(user_num);

	if(system.newuser_questions & UQ_REALNAME)
		this.name=u.name.toString();
	if(system.newuser_questions & UQ_ALIASES)
		this.alias=u.alias.toString();
	if(system.newuser_questions & UQ_HANDLE)
		this.handle=u.handle.toString();
	if(system.newuser_questions & UQ_LOCATION)
		this.location=u.location.toString();
	this.netmail=u.netmail.toString();
	if(system.newuser_questions & UQ_PHONE)
		this.phone=u.phone.toString();
	this.connection=u.connection.toString();
	this.logon=strftime("%b-%d-%y",u.stats.laston_date);
	this.laston=0-u.stats.laston_date;

	prefsfile=new File(prefs_dir + format("%04d.html_prefs",u.number));
	prefsfile.open("r");

	this.values=[];
	for(i=0; i<html_profile_fields.length; i++) {
		o={};
		o.iniKey=html_profile_fields[i].iniKey;
		if(o.iniKey===undefined)
			continue;
		o.displayName=html_profile_fields[i].displayName;
		if(o.displayName===undefined)
			o.displayName=o.iniKey;
		o.defaultValue=html_profile_fields[i].defaultValue;
		if(o.defaultValue===undefined)
			o.defaultValue='';
		if(prefsfile.is_open)
			o.value=prefsfile.iniGetValue('Profile', o.iniKey, o.defaultValue);
		if(html_profile_fields[i].property != undefined)
			this[html_profile_fields[i].property]=o.value;
		this.values.push(o);
	}

	if(prefsfile.is_open)
		prefsfile.close();
}
