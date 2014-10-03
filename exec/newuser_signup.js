/*
 * New user creation routine... basically a copy of newuser.cpp
 */

load("text.js");
load("nodedefs.js");
load("sbbsdefs.js");

function create_newuser()
{
	var node = system.node_list[bbs.node_num-1];
	var useron = {
		name:'', 
		handle:'', 
		alias:'',
		zipcode:'',
		phone:'',
		stats:{
			laston_date:0, 
			firston_date:0, 
			total_logons:0, 
			logons_today:0,
			total_timeon:0,
			timeon_today:0,
			timeon_last_logon:0,
			total_posts:0,
			total_emails:0,
			total_feedbacks:0,
			email_today:0,
			posts_today:0,
			bytes_uploaded:0,
			files_uploaded:0,
			bytes_downloaded:0,
			files_downloaded:0,
			leech_attempts:0,
			mail_waiting:0,
			mail_pending:0
		},
		security:{},
		limits:{}
	};
	var newuser;
	var usa = false;
	var attempts;
	var str, tmp;
	var dupe_user;
	var i;
	var kmode;
	var editors;
	var orig_at = js.auto_terminate;
	js.auto_terminate = false;

	function logline(level, code, str)
	{
		log(level, str);
	}

	function delallmail(usernumber, which, permanent)
	{
		if (permanent === undefined)
			permanent = true;
		var mail = new MsgBase('mail');
		var i;
		var hdr;
		if (mail == undefined)
			return;
		if (!mail.open())
			return;
		for (i=mail.first_msg; i<=mail.last_msg; i++) {
			hdr = mail.get_msg_header(i);
			if (hdr === null || hdr.number == 0)
				continue;
			if (hdr.addr & MSG_DELETE)
				continue;
			switch(which) {
				case MAIL_YOUR:
					if (hdr.to != usernumber)
						continue;
					break;
				case MAIL_SENT:
					if (hdr.from != usernumber)
						continue;
					break;
				case MAIL_ANY:
					if (hdr.to != usernumber && hdr.from != usernumber)
						continue;
				case MAIL_ALL:
					break;
				default:
					continue;
			}
			if(!permanent) {
				if (hdr.attr & MSG_PERMANENT)
					continue;
			}
			hdr.attr |= MSG_DELETE;
			hdr.attr &= ~MSG_PERMANENT;
			mail.put_msg_header(i, hdr);
		}
	}

	function copy_user_template_to_user(template, touser) {
		function copy_level(tmp, usr, prefix) {
			var prop;

			for (prop in usr) {
				if (tmp[prop] == undefined)
					continue;
				if (typeof(usr[prop])=='object') {
					copy_level(tmp[prop], usr[prop], prefix+prop+'.');
					delete tmp[prop];
				}
				else {
					usr[prop] = tmp[prop];
					delete tmp[prop];
				}
			}
		}

		function check_level(tmp, prefix) {
			var prop;

			for (prop in tmp) {
				if (typeof(tmp[prop]=='object'))
					check_level(tmp[prop], prefix+prop+'.');
				else
					log(LOG_DEBUG, "New user creation: template has extra property "+prefix+prop);
			}
		}
		copy_level(template, touser, "User.");
		check_level(template, "User.");
	}

	function text_print(text)
	{
		text = text.replace(/@([^@]+)@/g, function(m, code) {
			return bbs.atcode(code);
		});
		console.print(text);
	}

	function kill_user(message)
	{
		if (user != undefined && user.number) {
			user.comment = message;
			user.settings |= USER_DELETED;
		}
	}

	function check_online(message)
	{
		if (!bbs.online) {
			kill_user(message);
			return false;
		}
		return true;
	}

	text_print(bbs.text(StartingNewUserRegistration));
	if (node.misc & NODE_LOCK) {
		text_print(bbs.text(NodeLocked));
		// TODO: No logline function!
		logline(LOG_WARNING, "N!","New user locked node logon attempt");
		bbs.hangup();
		js.auto_terminate = orig_at;
		return false;
	}

	if (system.settings & SYS_CLOSED) {
		text_print(bbs.text(NoNewUsers));
		bbs.hangup();
		js.auto_terminate = orig_at;
		return false;
	}

	node.status = NODE_NEWUSER;
	node.connection = 30000;	// TODO: sbbs_t->node_connection is not available to JS
	if (system.newuser_password.length > 0 && bbs.online == ON_REMOTE) {
		str = '';
		for(attempts = 1; attempts < 4; attempts++) {
			text_print(bbs.text(NewUserPasswordPrompt));
			str = console.getstr(str, 40, K_UPPER);
			if (str === system.newuser_password)
				break;
			logline(LOG_NOTICE, "N!", "NUP Attempted: '"+str+"'");
		}
		if(attempts==4) {
			if(file_exists(system.text_dir+"nupguess.msg"))
				console.printfile(system.text_dir+"nupguess.msg");
			bbs.hangup();
			js.auto_terminate = orig_at;
			return false;
		}
	}

	/* Sets defaults per sysop config */
	useron.settings |= (system.newuser_settings &~ (USER_INACTIVE|USER_QUIET|USER_NETMAIL));
	useron.qwk=QWK_FILES|QWK_ATTACH|QWK_EMAIL|QWK_DELMAIL;
	useron.stats.firston_date = useron.stats.laston_date = useron.security.password_date = time();
	if (system.newuser_expiration_days > 0) {
		useron.security.expiration_date = time()+system.newuser_expiration_days*24*60*60;
	}
	else {
		useron.security.expiration_date = 0;
	}
	useron.gender = ' ';
	useron.download_protocol = system.newuser_download_protocol;
	useron.host_name = client.host_name;
	useron.ip_address = client.ip_address;
	if ((dupe_user = system.matchuserdata(U_NOTE, client.ip_address)) != 0)
		logline(LOG_NOTICE, "N!", "Warning: same IP address as user #"+dupe_user+" "+system.username(dupe_user));
	useron.connection = client.protocol;

	if (system.lastuser==0) { /* Automatic sysop access for first user NOTE: Breaks after failed attempt! */
		console.print("Creating sysop account... System password required.\r\n");
		if (!console.check_syspass()) {
			js.auto_terminate = orig_at;
			return false;
		}
		useron.security.level=99;
		useron.security.exemptions = useron.security.flags1 = useron.security.flags2 = 0xffffffff;
		useron.security.flags3 = useron.security.flags4 = 0xffffffff;
		useron.security.restrictions = 0;
	}
	else {
		useron.security.level = system.newuser_level;
		useron.security.exemptions = system.newuser_exemptions;
		useron.security.flags1 = system.newuser_flags1;
		useron.security.flags2 = system.newuser_flags2;
		useron.security.flags3 = system.newuser_flags3;
		useron.security.flags4 = system.newuser_flags4;
		useron.security.restrictions = system.newuser_restrictions;
	}

	useron.security.credits = system.newuser_credits
	useron.security.minutes = system.newuser_minutes
	/* TODO: No way to do this from JavaScript */
	/*
	useron.freecdt=cfg.level_freecdtperday[useron.level];
	*/

	/* TODO: No way to do this in JavaScript? */
	/*
	if(cfg.total_fcomps)
		SAFECOPY(useron.tmpext,cfg.fcomp[0]->ext);
	else
		SAFECOPY(useron.tmpext,"ZIP");
	*/

	useron.command_shell = system.newuser_command_shell;

	kmode=(system.newuser_questions&UQ_NOEXASC)|K_EDIT|K_AUTODEL;
	if(!(system.newuser_questions&UQ_NOUPRLWR))
		kmode |= K_UPRLWR;

	while (bbs.online) {
		if (console.autoterm || (bbs.text(AutoTerminalQ).length > 0 && console.yesno(bbs.text(AutoTerminalQ)))) {
			useron.settings |= USER_AUTOTERM;
			useron.settings |= console.autoterm; 
		}
		else
			useron.settings &= ~USER_AUTOTERM;

		if (!(useron.settings & USER_AUTOTERM)) {
			if (bbs.text(AnsiTerminalQ).length > 0 && console.yesno(bbs.text(AnsiTerminalQ)))
				useron.settings |= USER_ANSI; 
			else
				useron.settings &= ~USER_ANSI;
		}

		if(useron.settings & USER_ANSI) {
			useron.screen_rows=0;	/* Auto-rows */
			if(!(system.newuser_questions & UQ_COLORTERM)
					|| useron.settings & (USER_RIP|USER_WIP|USER_HTML)
					|| bbs.text(ColorTerminalQ).length == 0
					|| console.yesno(bbs.text(ColorTerminalQ)))
				useron.settings |= USER_COLOR; 
			else
				useron.settings &= ~USER_COLOR;
		}
		else
			useron.screen_rows=24;
		if(bbs.text(ExAsciiTerminalQ).length > 0 && !console.yesno(bbs.text(ExAsciiTerminalQ)))
			useron.settings |= USER_NO_EXASCII;
		else
			useron.settings &= ~USER_NO_EXASCII;

		if(bbs.rlogin_name.length > 0)
			useron.alias = bbs.rlogin_name;

		while(bbs.online) {
			if(system.newuser_questions & UQ_ALIASES)
				text_print(bbs.text(EnterYourAlias));
			else
				text_print(bbs.text(EnterYourRealName));
			var tempAlias = console.getstr(useron.alias, LEN_ALIAS, kmode);
			truncsp(tempAlias);
			if(!system.check_name(tempAlias)
				|| (!(system.newuser_questions & UQ_ALIASES) && tempAlias.indexOf(' ') == -1)) {
				text_print(bbs.text(YouCantUseThatName));
				if(bbs.text(ContinueQ).length > 0 && !console.yesno(bbs.text(ContinueQ))) {
					kill_user("Did not continue after duplicate alias");
					js.auto_terminate = orig_at;
					return false;
				}
				continue;
			}
			useron.alias = tempAlias;
			break; 
		}
		if (!check_online("Disconnected after selecting alias")) {
			js.auto_terminate = orig_at;
			return false;
		}

		if (newuser === undefined) {
			text_print(bbs.text(CheckingSlots));
			try {
				newuser = (system.new_user(useron.alias, client));
			}
			catch(e) {
				console.print("Sorry, a user with your alias was created while you signed up!");
				logline(LOG_ERROR, "N!", "New user couldn't be created (user created while signing up)");
				bbs.hangup();
				js.auto_terminate = orig_at;
				return false;
			}
			if (typeof newuser === 'number') {
				logline(LOG_ERROR, "N!", "New user couldn't be created (error code "+newuser+")");
				bbs.hangup();
				js.auto_terminate = orig_at;
				return false;
			}
			tmp = newuser.security.password;
			newuser.security.password = '';
			bbs.login(newuser.alias, bbs.text(PasswordPrompt));
			user.security.password = tmp;

			logline(LOG_INFO, '', "Created user record #"+user.number+": "+user.alias);
			copy_user_template_to_user(useron, user);
			useron = user;
		}

		if((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME)) {
			while(bbs.online) {
				text_print(bbs.text(EnterYourRealName));
				var tempName = console.getstr(user.name, LEN_NAME, kmode);
				if (!system.check_name(tempName)
					|| tempName.indexOf(' ') == -1
					|| ((system.newuser_questions & UQ_DUPREAL)
						&& system.matchuser(tempName))) {
					text_print(bbs.text(YouCantUseThatName));
				} else {
					user.name = tempName;
					break;
				}
				if(bbs.text(ContinueQ).length > 0 && !console.yesno(bbs.text(ContinueQ))) {
					kill_user("Did not continue after duplicate real name");
					js.auto_terminate = orig_at;
					return false;
				}
			} 
		}
		else if(system.newuser_questions & UQ_COMPANY) {
			text_print(bbs.text(EnterYourCompany));
			user.name = console.getstr(user.name, LEN_NAME, (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL); 
		}
		if(user.name.length == 0)
			user.name = user.alias.substr(0, LEN_ALIAS);
		if(!check_online("Hung up after entering real name")) {
			js.auto_terminate = orig_at;
			return false;
		}
		if(user.handle.length == 0)
			user.handle = user.alias.substr(0, LEN_HANDLE);
		while((system.newuser_questions & UQ_HANDLE) && bbs.online) {
			text_print(bbs.text(EnterYourHandle));
			if((user.handle = console.getstr(
						user.handle,
						LEN_HANDLE,
						K_LINE|K_EDIT|K_AUTODEL|(system.newuser_questions&UQ_NOEXASC))) == null
					|| user.handle.indexOf('\xff') != -1
					|| ((system.newuser_questions & UQ_DUPHAND)
						&& system.matchuser(user.handle))
					|| system.trashcan("name", user.handle))
				text_print(bbs.text(YouCantUseThatName));
			else
				break; 
			if(bbs.text(ContinueQ).length > 0 && !console.yesno(bbs.text(ContinueQ))) {
				kill_user("Did not continue after duplicate handle");
				js.auto_terminate = orig_at;
				return false;
			}
		}
		if(!check_online("Hung up after entering handle")) {
			js.auto_terminate = orig_at;
			return false;
		}
		if(system.newuser_questions & UQ_ADDRESS)
			while(bbs.online) { 	   /* Get address and zip code */
				text_print(bbs.text(EnterYourAddress));
				if((user.address = console.getstr(user.address,LEN_ADDRESS,kmode)) != null)
					break; 
			}
		if(!check_online("Hung up after entering address")) {
			js.auto_terminate = orig_at;
			return false;
		}
		while((system.newuser_questions & UQ_LOCATION) && bbs.online) {
			text_print(bbs.text(EnterYourCityState));
			if((user.location = console.getstr(user.location, LEN_LOCATION, kmode)) != null
				&& ((system.newuser_questions & UQ_NOCOMMAS) || user.location.indexOf(',') != -1))
				break;
			text_print(bbs.text(CommaInLocationRequired));
			user.location = ''; 
		}
		if(system.newuser_questions & UQ_ADDRESS)
			while(bbs.online) {
				text_print(bbs.text(EnterYourZipCode));
				if(console.getstr(user.zipcode,LEN_ZIPCODE
					,K_UPPER|(system.newuser_questions & UQ_NOEXASC)|K_EDIT|K_AUTODEL))
					break; 
			}
		if(!check_online("Hung up after entering ZIP/postal code")) {
			js.auto_terminate = orig_at;
			return false;
		}
		if(system.newuser_questions & UQ_PHONE) {
			if(bbs.text(CallingFromNorthAmericaQ).length > 0)
				usa=console.yesno(bbs.text(CallingFromNorthAmericaQ));
			else
				usa=false;
			while(bbs.online && bbs.text(EnterYourPhoneNumber).length > 0) {
				text_print(bbs.text(EnterYourPhoneNumber));
				if(!usa) {
					if((user.phone = console.getstr(user.phone,LEN_PHONE
						,K_UPPER|K_LINE|(system.newuser_questions & UQ_NOEXASC)|K_EDIT|K_AUTODEL)).length < 5)
						continue; 
				}
				else {
					/* TODO: cfg.sys_phonefmt isn't available! */
					if((user.phone = console.gettemplate(/*cfg.sys_phonefmt*/'!!!!!!!!!!!!', user.phone
						,K_LINE|(system.newuser_questions & UQ_NOEXASC)|K_EDIT)).length < /*cfg.sys_phonefmt*/'!!!!!!!!!!!!'.length)
						continue; 
				}
				if(!system.trashcan("phone", user.phone))
					break; 
			}
		}
		if(!check_online("Hung up after entering phone number")) {
			js.auto_terminate = orig_at;
			return false;
		}
		if(system.newuser_questions & UQ_SEX) {
			text_print(bbs.text(EnterYourSex));
			user.gender=console.getkeys("MF");
		}
		while((system.newuser_questions & UQ_BIRTH) && bbs.online) {
			text_print(format(bbs.text(EnterYourBirthday)
				,system.settings & SYS_EURODATE ? "DD/MM/YY" : "MM/DD/YY"));
			if ((user.birthdate = console.gettemplate("nn/nn/nn", user.birthdate, K_EDIT)).length==8)
				user.cached = false;
				if (user.age > 0)
					break;
		}
		if(!check_online("Hung up after entering birth date")) {
			js.auto_terminate = orig_at;
			return false;
		}
		while(!(system.newuser_questions & UQ_NONETMAIL) && bbs.online) {
			text_print(bbs.text(EnterNetMailAddress));
			if((user.netmail = console.getstr(user.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE)) != null
				&& !system.trashcan("email", user.netmail))
			break;
		}
		if(user.netmail.length > 0 && (system.settings & SYS_FWDTONET) && bbs.text(ForwardMailQ).length > 0 && console.yesno(bbs.text(ForwardMailQ)))
			user.settings |= USER_NETMAIL;
		else 
			user.settings &= ~USER_NETMAIL;
		if(bbs.text(UserInfoCorrectQ).length > 0 && console.yesno(bbs.text(UserInfoCorrectQ)))
			break; 
	}
	if(!check_online("Hung up after entering user info")) {
		js.auto_terminate = orig_at;
		return false;
	}
	logline(LOG_INFO, "N", "New user: "+user.alias);
	if(!check_online("Hung up after new user log entry made")) {
		js.auto_terminate = orig_at;
		return false;
	}
	console.clear();
	
	console.printfile(system.text_dir+"sbbs.msg", P_NOABORT);
	if(console.line_counter)
		console.pause();
	console.clear();
	console.printfile(system.text_dir+"system.msg", P_NOABORT);
	if(console.line_counter)
		console.pause();
	console.clear();
	console.printfile(system.text_dir+"newuser.msg", P_NOABORT);
	if(console.line_counter)
		console.pause();
	console.clear();
	bbs.answer_time = time();		/* could take 10 minutes to get this far */

	/* Default editor (moved here, after terminal type setup Jan-2003) */
	if (bbs.compare_ars(xtrn_area.editor[system.newuser_editor.toLowerCase()].ars))
		user.editor = system.newuser_editor;
	else
		newuser.editor = '';

	if (xtrn_area.editor.length > 0 && (system.newuser_questions & UQ_XEDIT) && bbs.text(UseExternalEditorQ).length > 0) {
		if(console.yesno(bbs.text(UseExternalEditorQ))) {
			editors=[];
			for(str in xtrn_area.editor)
				editors.push(str);
			for(i=0; i<editors.length; i++)
				console.uselect(i+1,bbs.text(ExternalEditorHeading),xtrn_area.editor[editors[i]].name,xtrn_area.editor[editors[i]].ars);
			// TODO: console.uselect doesn't allow setting a default!
			if((i=console.uselect())>=0)
				user.editor = editors[i-1] 
		} else
			user.editor = '';
	}

	// TODO: No way to do this!
	/*
	if(cfg.total_shells>1 && (cfg.uq&UQ_CMDSHELL)) {
		for(i=0;i<cfg.total_shells;i++)
			uselect(1,i,text[CommandShellHeading],cfg.shell[i]->name,cfg.shell[i]->ar);
		if((int)(i=uselect(0,newuser.shell,0,0,0))>=0)
			newuser.shell=i; 
	}
	*/
	bbs.select_shell();
	newuser.cached = false;

	// TODO: Doesn't take the user argument...
	if(bbs.rlogin_password.length > 0 && bbs.good_password(bbs.rlogin_password)) {
		console.crlf();
		/* passwords are case insensitive, but assumed (in some places) to be uppercase in the user database */
		newuser.security.password = bbs.rlogin_password.toUpperCase();
	}
	else {
		newuser.security.password = '';
		for (i=0; i<LEN_PASS; ) {	/* Create random password... TODO: this can be improved */
			newuser.security.password += ascii(random(43)+ascii('0'));
			if (newuser.security.password.substr(-1, 1).search(/[0-9A-Za-z]/) == 0)
				i++;
			else
				newuser.security.password = newuser.security.password.substr(0, i);
		}

		text_print(format(bbs.text(YourPasswordIs),newuser.security.password));

		if(system.settings & SYS_PWEDIT && bbs.text(NewPasswordQ).length > 0 && console.yesno(bbs.text(NewPasswordQ)))
			while(bbs.online) {
				text_print(bbs.text(NewPassword));
				str = console.getstr('',LEN_PASS,K_UPPER|K_LINE);
				str = truncsp(str);
				if(bbs.good_password(str)) {
					newuser.security.password = str;
					console.crlf();
					text_print(format(bbs.text(YourPasswordIs),newuser.security.password));
					break; 
				}
				console.crlf();
			}

		i=0;
		while(bbs.online) {
			text_print(bbs.text(NewUserPasswordVerify));
			console.status |= CON_R_ECHOX;
			str = '';
			str = console.getstr(str,LEN_PASS*2,K_UPPER);
			console.status &= ~(CON_R_ECHOX|CON_L_ECHOX);
			if(str == newuser.security.password)
				break;
			if(system.settings & SYS_ECHO_PW)
				tmp = newuser.alias + " FAILED Password verification: '"+str+"' instead of '"+newuser.security.password+"'";
			else
				tmp = newuser.alias + " FAILED Password verification";
			logline(LOG_NOTICE,'',tmp);
			if(++i==4) {
				logline(LOG_NOTICE,"N!","Couldn't figure out password.");
				newuser.comment = "Unable to validate password";
				newuser.settings |= USER_DELETED;
				bbs.hangup();
				js.auto_terminate = orig_at;
				return false;
			}
			text_print(bbs.text(IncorrectPassword));
			text_print(format(bbs.text(YourPasswordIs),newuser.security.password));
		}
	}

	if(!check_online("Hung up after validating password")) {
		js.auto_terminate = orig_at;
		return false;
	}
	if(system.newuser_magic_word.length > 0) {
		text_print(bbs.text(MagicWordPrompt));
		str = '';
		str = console.getstr(str,50,K_UPPER);
		if(str != system.newuser_magic_word) {
			newuser.comment = "Failed to guess the magic word";
			newuser.settings |= USER_DELETED;
			text_print(bbs.text(FailedMagicWord));
			logline(LOG_INFO, "N!",newuser.alias+" failed magic word: '"+str+"'");
			bbs.hangup();
			js.auto_terminate = orig_at;
			return false;
		}
		if(!check_online("Hung up after guessing magic word")) {
			js.auto_terminate = orig_at;
			return false;
		}
	}

	/* TODO: No way to do this in Javascript */
	/*
	if(cfg.new_sif[0]) {
		SAFEPRINTF2(str,"%suser/%4.4u.dat",cfg.data_dir,newuser.number);
		create_sif_dat(cfg.new_sif,str); 
	}
	*/

	/* TODO: delallmail() isn't available... do it the hard way! */
	delallmail(newuser.number, MAIL_ANY);

	if(!(system.newuser_questions & UQ_NODEF)) {
		bbs.user_config();
		user.cached = false;
	}

	if(newuser.number != 1 && bbs.node_val_user) {
		console.clear();
		console.printfile(system.text_dir+"feedback.msg", P_NOABORT);
		str = format(bbs.text(NewUserFeedbackHdr)
			,'',newuser.age,newuser.gender,newuser.birthdate
			,newuser.name,newuser.phone,newuser.computer,newuser.connection);

		bbs.email(bbs.node_val_user,str,"New User Validation",WM_EMAIL|WM_SUBJ_RO|WM_FORCEFWD);
		newuser.cached = false;
		if(!newuser.stats.total_feedbacks && !newuser.stats.total_emails) {
			if(bbs.online) {						/* didn't hang up */
				text_print(format(bbs.text(NoFeedbackWarning),system.username(bbs.node_val_user)));
				bbs.email(bbs.node_val_user,str,"New User Validation",WM_EMAIL|WM_SUBJ_RO|WM_FORCEFWD);
			} /* give 'em a 2nd try */
			newuser.cached = false;
			if(!newuser.stats.total_feedbacks && !newuser.stats.total_emails) {
        		text_print(format(bbs.text(NoFeedbackWarning),system.username(bbs.node_val_user)));
				logline(LOG_NOTICE,"N!","Aborted feedback");
				kill_user("Didn't leave feedback");
				bbs.hangup();
				js.auto_terminate = orig_at;
				return false;
			}
		}
	}

	bbs.answer_time=bbs.start_time=time();	  /* set answertime to now */

	/* TODO: No way to do this in Javascript? */
	/*
	if(cfg.newuser_mod[0])
		exec_bin(cfg.newuser_mod,&main_csi);
	*/
	bbs.user_event(EVENT_NEWUSER);
	bbs.user_sync();	// In case event(s) modified user data
	logline(LOG_INFO, "N+","Successful new user logon");
	bbs.sys_status |= SS_NEWUSER;

	js.auto_terminate = orig_at;
	return true;
}
