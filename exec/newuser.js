// newuser.js

// New user login module

// $Id: newuser.js,v 1.30 2019/04/28 23:25:44 rswindell Exp $

// @format.tab-size 8, @format.use-tabs true

load("sbbsdefs.js");

const QWK_ID_PATTERN = /^[A-Z]\w{1,7}$/;

var options;
options=load("modopts.js","newuser");
if(!options)
	options = {};

qnet=false;

if(bbs.sys_status&SS_RLOGIN)
	options.ask_qnet=false;

if(options.qwk_settings)
	user.qwk_settings = eval(options.qwk_settings);

if(options.send_newuser_welcome)	// backwards compatibility hack
	options.send_welcome = true;

console.clear();

if(!user.address.length && user.number>1) {
	print("\1y\1hWhere did you hear about this BBS?");
	user.address=console.getstr(30,K_LINE);
}

if(options.ask_qnet) {
	if(options.qnet_name==undefined)
		options.qnet_name="DOVE-Net";
	if(!console.noyes(format("\r\nIs this account to be used for QWK Networking (%s)\1b", options.qnet_name))
		&& !console.noyes("\r\n\1bARE YOU \1wPOSITIVE\1n\1h\1b (If you're unsure, press '\1wN\1b')"))
		qnet=true;
}

if(!qnet && (options.avatar || options.avatar_file)) {
	var avatar_lib = load({}, 'avatar_lib.js');
	if(options.avatar_file)
		avatar_lib.import_file(user.number, options.avatar_file, options.avatar_offset);
	else
		avatar_lib.update_localuser(user.number, options.avatar);
}	

function chk_qwk_id(str)
{
	if(str.search(QWK_ID_PATTERN) != 0)
		return false;

	var userfound=system.matchuser(str,true);
	if(userfound && userfound!=user.number)
		return(false);

	if(system.trashcan(str))
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

if(options.ask_sysop 
	&& !console.noyes("\r\n\1bAre you a sysop of a \1wSynchronet\1b BBS (unsure, hit '\1wN\1b')")) {
	user.security.flags1|=UFLAG_S;
	if(qnet) {
		user.qwk_settings|=(QWK_RETCTLA|QWK_NOINDEX|QWK_NOCTRL|QWK_VIA|QWK_TZ|QWK_MSGID);
		user.qwk_settings&=~QWK_FILES;
	}
	else if(console.yesno("\r\nDo you wish to access the Synchronet BBS List database"))
		bbs.exec_xtrn("SBBSLIST");
}

/********************************/
/* Send New User Welcome E-mail */
/********************************/
welcome_msg = system.text_dir + "welcome.msg"; 
if(options.send_welcome && file_exists(welcome_msg) && !qnet && user.number>1)
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
		attr: MSG_KILLREAD,
		subject: "Welcome to " + system.name + "!" 
	};

	if(!msgbase.save_msg(hdr, msgtxt))
		log(LOG_ERR,"!ERROR " + msgbase.last_error + "saving mail message");

	log(LOG_INFO,"Sent new user welcome e-mail");

	msgbase.close();
}
