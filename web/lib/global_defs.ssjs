/* $Id$ */

/*            Global Definitions              */
/*            Place globals here              */
/* Remember, the more global definitions here */
/* the longer all pages will take to display! */

/* User Changable Variables */

var show_gender=true;
var show_location=true;
var show_age=true;

/*       If you want to remove QWk FTP downloads       */
/* change this to false.  Moved from leftnav_html.ssjs */

var doQWK = false;

/* End of User Changable Variables */

/* Need to move SortDate to [Messaging] tag in prefs file  */

prefs_dir=system.data_dir + 'user/';

if(user.number!=0) {
	if(file_exists(prefs_dir + format("%04d.html_prefs",user.number))); {
		prefsfile=new File(prefs_dir + format("%04d.html_prefs",user.number));
		if(prefsfile.open("r",false)) {
			if(prefsfile.iniGetValue(null, 'SortDate', '')!='');
				var SortDate=prefsfile.iniGetValue(null, 'SortDate', '');
			prefsfile.close();
		}
		prefsfile=new File(prefs_dir + '/'+format("%04d.html_prefs",user.number));
		if(SortDate!='') {
			if(prefsfile.open("w+",false)) {
				prefsfile.iniSetValue('Messaging', 'SortDate', SortDate);
       			prefsfile.close();
			}
       	}
	}
}

/* Set default template.info */

if(user.number!=0) {
	template.user_alias=user.alias;
	template.user_handle=user.handle;
	template.user_email=user.netmail;
	template.user_rn=user.name;
	template.user_address=user.address;
	template.user_location=user.location;
	template.user_zip=user.zipcode;
}

/* User Greeting */

if(user.number==0)
    template.user_greeting="Welcome, Guest.";
else
	if(!(user.security.restrictions&UFLAG_G)) {
	
/* If it is the users Birthday, display a quick Happy Birthday */
/* instead of the standard greeting. */

    	var     birthday = user.birthdate.substring(0,5);
		var today = system.datestr().substring(0,5);
		if(birthday==today)
			template.user_greeting="Happy Birthday, "+user.alias+ "! <span class=\"editprofile\">(<a href=\"/members/editprofile.ssjs\">edit profile</a>)</span><br /> Can you believe that you are " + user.age + " years old!!";
		else
        	template.user_greeting="Welcome, "+user.alias+ ". <span class=\"editprofile\">(<a href=\"/members/editprofile.ssjs\">edit profile</a>)</span><br /> You last visited on " +strftime("%A, %B %d, %Y",user.stats.laston_date);
		} else
        template.user_greeting="Welcome, "+user.alias+ ".";

/* Gives RAW port number - must be prepended with ":" for URI's */

template.http_port = http_port;
template.irc_port = irc_port;
template.ftp_port = ftp_port;
template.nntp_port = nntp_port;
template.gopher_port = gopher_port;
template.finger_port = finger_port;
template.telnet_port = telnet_port;
template.rlogin_port = rlogin_port;
template.smtp_port = smtp_port;
template.pop3_port = pop3_port;

