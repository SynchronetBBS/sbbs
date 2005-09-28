// newuser.js

// New user login module

// $Id$

// @format.tab-size 8, @format.use-tabs true

load("sbbsdefs.js");

send_newuser_welcome = true; // Set to false to disable the new user welcome msg
ask_qnet=false;		// Set to true to enable QWKnet Node question
ask_sysop=false;	// Set to true to enable Synchronet Sysop question
qnet=false;

if(system.name=="Vertrauen") {
	ask_qnet=true;
	ask_sysop=true;
}

console.clear();

if(!user.address.length && user.number>1) {
	printf("\1y\1hWhere did you hear about this BBS? ");
	user.address=console.getstr(30,K_LINE);
}

if(ask_qnet 
	&& !console.noyes("\r\nIs this account to be used for QWK Networking (DOVE-Net)\1b")
	&& !console.noyes("\r\n\1bARE YOU \1wPOSITIVE\1n\1h\1b (If you're unsure, press '\1wN\1b')"))
	qnet=true;

function chk_qwk_id(str)
{
	if(str.length<2 || str.length>8)
		return(false);

	if(Number(str)>0 || Number(str)<0)
		return(false);

	/* I know this could be much smaller using regexp */
	if(str.indexOf(' ')>=0)
		return(false);
	if(str.indexOf('.')>=0)
		return(false);
	if(str.indexOf(':')>=0)
		return(false);
	if(str.indexOf(';')>=0)
		return(false);
	if(str.indexOf('\\')>=0)
		return(false);
	if(str.indexOf('/')>=0)
		return(false);
	if(str.indexOf('|')>=0)
		return(false);
	if(str.indexOf('+')>=0)
		return(false);
	if(str.indexOf('"')>=0)
		return(false);
	if(str.indexOf('&')>=0)
		return(false);

	if(system.matchuser(str,true)!=user.number || system.trashcan(str))
		return(false);

	return(true);
}

if(qnet) {
	alias = user.alias.toUpperCase();
	while(!chk_qwk_id(alias) && bbs.online) {
		console.crlf();
		printf("\1n\1g\1h o \1wYour logon name must match your BBS's QWK ID.\r\n");
		printf("\1g o\1w Your logon name is currently \"\1y%s\1w\"\r\n\r\n"
			,user.alias.toUpperCase());
		printf("This is an invalid QWK ID. Your QWK ID MUST be ");
		printf("between 2 and 8 characters in\r\n");
		printf("length, must begin with a letter and contain only valid ");
		printf("DOS filename characters.\r\n\r\n");
		printf("\1y\1hYour correct QWK ID (as configured in your ");
		printf("BBS software) is: ");
		alias=console.getstr(8,K_UPPER|K_LINE|K_NOEXASC);
	}
	if(!bbs.online)
		exit();
	user.alias=alias;
	user.security.restrictions|=UFLAG_Q;
	user.security.exemptions|=UFLAG_L;
	user.security.exemptions|=UFLAG_T;
	user.security.exemptions|=UFLAG_D;
	user.security.exemptions|=UFLAG_M;
}

if(ask_sysop 
	&& !console.noyes("\r\n\1bAre you a sysop of a \1wSynchronet\1b BBS (unsure, hit '\1wN\1b')")) {
	user.security.flags1|=UFLAG_S;
	if(qnet) {
		user.qwk_settings|=(QWK_RETCTLA|QWK_NOINDEX|QWK_NOCTRL|QWK_VIA|QWK_TZ|QWK_MSGID);
		user.qwk_settings&=~QWK_FILES;
	}
	else if(console.yesno("\r\nDo you wish to access the Synchronet BBS List database"))
		bbs.exec_xtrn("SBL");
}

/********************************/
/* Send New User Welcome E-mail */
/********************************/
welcome_msg = system.text_dir + "welcome.msg"; 
if(send_newuser_welcome && file_exists(welcome_msg) && !qnet && user.number>1)
	send_newuser_welcome_msg(welcome_msg);

function send_newuser_welcome_msg(fname)
{
	file = new File(fname);
	if(!file.open("rt")) {
		log(LOG_ERR,"!ERROR " + errno_str + " opening " + fname);
		return(false);
	}
	msgtxt = lfexpand(file.read(file.length));
	file.close();
	delete file;

	msgbase = new MsgBase("mail");
	if(msgbase.open()==false) {
		log(LOG_ERR,"!ERROR " + msgbase.last_error);
		return(false);
	}

	hdr = { 
		to: user.alias, 
		to_ext: String(user.number), 
		from: system.operator, 
		from_ext: "1",
		subject: "Welcome to " + system.name + "!" 
	};

	if(!msgbase.save_msg(hdr, msgtxt))
		log(LOG_ERR,"!ERROR " + msgbase.last_error + "saving mail message");

	log(LOG_INFO,"Sent new user welcome e-mail");

	msgbase.close();
}