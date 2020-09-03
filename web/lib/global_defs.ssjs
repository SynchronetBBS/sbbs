/* $Id: global_defs.ssjs,v 1.8 2006/02/06 10:41:26 runemaster Exp $ */

/*            Global Definitions              */
/*            Place globals here              */
/* Remember, the more global definitions here */
/* the longer all pages will take to display! */

/* User Changable Variables */

var show_gender=true;
var show_location=true;
var show_age=true;

/* If User #1 or is.operator and show_ip is set to true, */
/* Then the Author info will post the last known IP from */
/* user.note                                             */

var show_ip=true;

/*       If you want to remove QWk FTP downloads       */
/* change this to false.  Moved from leftnav_html.ssjs */

var doQWK = true;

/* End of User Changable Variables */

/* Need to move SortDate to [Messaging] tag in prefs file  */

prefs_dir=system.data_dir + 'user/';

if(user.number!=0) {
	if(file_exists(prefs_dir + format("%04d.html_prefs",user.number))) {
		prefsfile=new File(prefs_dir + format("%04d.html_prefs",user.number));
		if(prefsfile.open("r",false)) {
			if(prefsfile.iniGetValue(null, 'SortDate', '')!='')
				var SortDate=prefsfile.iniGetValue(null, 'SortDate', '');
			prefsfile.close();
		}
	}
		prefsfile=new File(prefs_dir + '/'+format("%04d.html_prefs",user.number));
		if(!file_exists(prefs_dir + format("%04d.html_prefs",user.number))) {
			SortDate="descending";
			if(prefsfile.open("w+",false)) {
				prefsfile.iniSetValue('User Info', 'Alias', user.alias);
				prefsfile.iniSetValue('User Info', 'Name', user.name);
				prefsfile.iniSetValue('Messaging', 'SortDate', SortDate);
       			prefsfile.close();
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
			template.user_greeting="Happy Birthday, "+user.alias+ "! <br /> Can you believe that you are " + user.age + " years old!!";
		else
        	template.user_greeting="Welcome, "+user.alias+ ". <br /> You last visited on " +strftime("%A, %B %d, %Y",user.stats.laston_date);
		} else
        template.user_greeting="Welcome, "+user.alias+ ".";

/* Sets the hostname */

/* Gives number prepended with ":" for URI's */

if(telnet_port=="23")
telnet_port="";
else
telnet_port = ":" + telnet_port;
if(rlogin_port=="513")
rlogin_port="";
else 
rlogin_port = ":" + rlogin_port;
if(http_port=="80")
http_port='';
else
http_port = ":" + http_port;
if(ftp_port=="21")
ftp_port="";
else 
ftp_port = ":" + ftp_port;
if(irc_port=="6667")
irc_port="";
else 
irc_port = ":" + irc_port;
if(nntp_port=="119")
nntp_port="";
else
nntp_port = ":" + nntp_port;
if(gopher_port=="70")
gopher_port='';
else 
gopher_port = ":" + gopher_port;
if(finger_port=="79")
finger_port='';
else 
finger_port = ":" + finger_port;
if(pop3_port=="110")
pop3_port='';
else 
pop3_port = ":" + pop3_port;
if(smtp_port=="25")
smtp_port='';
else 
smtp_port = ":" + smtp_port;

template.host=host;
template.telnet_port=telnet_port;
template.rlogin_port=rlogin_port;
template.http_port=http_port;
template.ftp_port=ftp_port;
template.smtp_port=smtp_port;
template.pop3_port=pop3_port;
template.irc_port = irc_port;
template.nntp_port = nntp_port;
template.gopher_port = gopher_port;
template.finger_port = finger_port;


