/*
 * New user creation routine... basically a copy of newuser.cpp
 */

load("text.js");
load("nodedefs.js");
load("sbbsdefs.js");

function logline(level, code, str)
{
	log(level, code, str);
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
		if (hdr.number == 0)
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

function create_newuser()
{
	var node = system.node_list[bbs.node_num-1];
	var newuser;
	var usa = false;
	var attempts;
	var str, tmp;
	var dupe_user;
	var i;
	var kmode;
	var editors;

	console.print(bbs.text(StartingNewUserRegistration));
	if (node.misc & NODE_LOCK) {
		console.print(bbs.text(NodeLocked));
		// TODO: No logline function!
		logline(LOG_WARNING, "N!","New user locked node l attempt");
		bbs.hangup();
		return false;
	}

	if (system.settings & SYS_CLOSED) {
		console.print(bbs.text(NoNewUsers));
		bbs.hangup();
		return false;
	}

	node.status = NODE_NEWUSER;
	node.connection = 30000;	// TODO: sbbs_t->node_connection is not available to JS
	if (system.newuser_password.length > 0 && bbs.online == ON_REMOTE) {
		str = '';
		for(attempts = 1; attempts < 4; attempts++) {
			console.print(bbs.text(NewUserPasswordPrompt));
			str = console.getstr(str, 40, K_UPPER);
			if (str === system.newuser_password)
				break;
			logline(LOG_NOTICE, "N!", "NUP Attempted: '"+str+"'");
		}
		if(attempts==4) {
			if(file_exists(system.text_dir+"nupguess.msg"))
				printfile(system.text_dir+"nupguess.msg");
			bbs.hangup();
			return false;
		}
	}

	/* Find a new user name that's available */
	console.print(bbs.text(CheckingSlots));	// Moved here from bottom since this is where it's done now...
	for(i=0; ; i++) {
		try {
			newuser = (system.new_user("New"+i, client));
			break;
		}
		catch(e) {
			continue;
		}
	}
	if (typeof newuser === 'number') {
		logline(LOG_ERROR, "N!", "New user couldn't be created (error code "+newuser+")");
		bbs.hangup();
		return false;
	}
	/* Sets defaults per sysop config */
	newuser.settings |= (system.newuser_settings &~ (USER_INACTIVE|USER_QUIET|USER_NETMAIL));
	/* Set as deleted for now so we can return early without problems */
	newuser.host_name = client.host_name;
	newuser.ip_address = client.ip_address;
	if ((dupe_user = system.matchuserdata(U_NOTE, client.ip_address)) != 0)
		logline(LOG_NOTICE, "N!", "Warning: same IP address as user #"+dupe_user+" "+system.username(dupe_user));
	newuser.connection = client.protocol;

	/* NOTE: If the sysop aborts after this, it will never happen again. */
	if (system.lastuser==1) { /* Automatic sysop access for first user */
		console.print("Creating sysop account... System password required.\r\n");
		if (!console.check_syspass()) {
			newuser.comment = "Sysop creation failed sysop password";
			newuser.settings |= USER_DELETED;
			return false;
		}
		newuser.security.level=99;
		newuser.security.exemptions = newuser.security.flags1 = newuser.security.flags2 = 0xffffffff;
		newuser.security.flags3 = newuser.security.flags4 = 0xffffffff;
		newuser.security.restrictions = 0;
	}

	newuser.alias = '';

	kmode=(system.newuser_questions&UQ_NOEXASC)|K_EDIT|K_AUTODEL;
	if(!(system.newuser_questions&UQ_NOUPRLWR))
		kmode |= K_UPRLWR;

	while (bbs.online) {
		if (console.autoterm || (bbs.text(AutoTerminalQ).length > 0 && console.yesno(bbs.text(AutoTerminalQ)))) {
			newuser.settings |= USER_AUTOTERM;
			newuser.settings |= console.autoterm; 
		}
		else
			newuser.settings &= ~USER_AUTOTERM;

		if (!(newuser.settings & USER_AUTOTERM)) {
			if (bbs.text(AnsiTerminalQ).length > 0 && console.yesno(bbs.text(AnsiTerminalQ)))
				newuser.settings |= USER_ANSI; 
			else
				newuser.settings &= ~USER_ANSI;
		}

		if(newuser.settings & USER_ANSI) {
			newuser.screen_rows=0;	/* Auto-rows */
			if(!(system.newuser_questions & UQ_COLORTERM)
					|| newuser.settings & (USER_RIP|USER_WIP|USER_HTML)
					|| bbs.text(ColorTerminalQ).length == 0
					|| console.yesno(bbs.text(ColorTerminalQ)))
				newuser.settings |= USER_COLOR; 
			else
				newuser.settings &= ~USER_COLOR;
		}
		else
			newuser.screen_rows=24;
		if(bbs.text(ExAsciiTerminalQ).length > 0 && !console.yesno(bbs.text(ExAsciiTerminalQ)))
			newuser.settings |= USER_NO_EXASCII;
		else
			newuser.settings &= ~USER_NO_EXASCII;

		if(bbs.rlogin_name.length > 0)
			newuser.alias = bbs.rlogin_name;

		while(bbs.online) {
			if(system.newuser_questions & UQ_ALIASES)
				console.print(bbs.text(EnterYourAlias));
			else
				console.print(bbs.text(EnterYourRealName));
			var tempAlias = console.getstr(newuser.alias, LEN_ALIAS, kmode);
			truncsp(tempAlias);
			if(!system.check_name(tempAlias)
				|| (!(system.newuser_questions & UQ_ALIASES) && tempAlias.indexOf(' ') == -1)) {
				console.print(bbs.text(YouCantUseThatName));
				if(bbs.text(ContinueQ).length > 0 && !console.yesno(bbs.text(ContinueQ))) {
					newuser.comment = "Coudn't make up a name";
					newuser.settings |= USER_DELETED;
					return false;
				}
				continue;
			}
			newuser.alias = tempAlias;
			break; 
		}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		if((system.newuser_questions & UQ_ALIASES) && (system.newuser_questions & UQ_REALNAME)) {
			while(bbs.online) {
				console.print(bbs.text(EnterYourRealName));
				var tempName = console.getstr(newuser.name, LEN_NAME, kmode);
				if (!system.check_name(tempName)
					|| tempName.indexOf(' ') == -1
					|| ((system.newuser_questions & UQ_DUPREAL)
						&& system.matchuserdata(U_NAME, tempName, newuser.number))) {
					console.print(bbs.text(YouCantUseThatName));
				} else {
					newuser.name = tempName;
					break;
				}
				if(bbs.text(ContinueQ).length > 0 && !console.yesno(bbs.text(ContinueQ))) {
					newuser.comment = "Failed to create alias";
					newuser.settings |= USER_DELETED;
					return false;
				}
			} 
		}
		else if(system.newuser_questions & UQ_COMPANY) {
				console.print(bbs.text(EnterYourCompany));
				newuser.name = getstr(newuser.name, LEN_NAME, (system.newuser_questions & UQ_NOEXASC) | K_EDIT | K_AUTODEL); 
		}
		if(newuser.name.length == 0)
			newuser.name = newuser.alias;
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		if(newuser.handle.length == 0)
			newuser.handle = newuser.alias;
		while((system.newuser_questions & UQ_HANDLE) && bbs.online) {
			console.print(bbs.text(EnterYourHandle));
			if((newuser.handle = console.getstr(
						newuser.handle,
						LEN_HANDLE,
						K_LINE|K_EDIT|K_AUTODEL|(system.newuser_questions&UQ_NOEXASC))) == null
					|| newuser.handle.indexOf('\xff') != -1
					|| ((system.newuser_questions & UQ_DUPHAND)
						&& system.matchuserdata(U_HANDLE,newuser.handle,newuser.number))
					|| system.trashcan("name", newuser.handle))
				console.print(bbs.text(YouCantUseThatName));
			else
				break; 
			if(bbs.text(ContinueQ).length > 0 && !console.yesno(bbs.text(ContinueQ))) {
				newuser.comment = "Failed to create handle";
				newuser.settings |= USER_DELETED;
				return false;
			}
		}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		if(system.newuser_questions & UQ_ADDRESS)
			while(bbs.online) { 	   /* Get address and zip code */
				console.print(bbs.text(EnterYourAddress));
				if((newuser.address = getstr(newuser.address,LEN_ADDRESS,kmode)) != null)
					break; 
			}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		while((system.newuser_questions & UQ_LOCATION) && bbs.online) {
			console.print(bbs.text(EnterYourCityState));
			if((newuser.location = console.getstr(newuser.location, LEN_LOCATION, kmode)) != null
				&& ((system.newuser_questions & UQ_NOCOMMAS) || newuser.location.indexOf(',') != -1))
				break;
			console.print(bbs.text(CommaInLocationRequired));
			newuser.location = ''; 
		}
		if(system.newuser_questions & UQ_ADDRESS)
			while(bbs.online) {
				console.print(bbs.text(EnterYourZipCode));
				if(console.getstr(newuser.zipcode,LEN_ZIPCODE
					,K_UPPER|(system.newuser_questions & UQ_NOEXASC)|K_EDIT|K_AUTODEL))
					break; 
			}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		if(system.newuser_questions & UQ_PHONE) {
			if(bbs.text(CallingFromNorthAmericaQ).length > 0)
				usa=console.yesno(bbs.text(CallingFromNorthAmericaQ));
			else
				usa=false;
			while(bbs.online && bbs.text(EnterYourPhoneNumber).length > 0) {
				console.print(bbs.text(EnterYourPhoneNumber));
				if(!usa) {
					if((newuser.phone = console.getstr(newuser.phone,LEN_PHONE
						,K_UPPER|K_LINE|(system.newuser_questions & UQ_NOEXASC)|K_EDIT|K_AUTODEL)).length < 5)
						continue; 
				}
				else {
					/* TODO: cfg.sys_phonefmt isn't available! */
					if((newuser.phone = console.gettempalte(/*cfg.sys_phonefmt*/'!!!!!!!!!!!!', newuser.phone
						,K_LINE|(system.newuser_questions & UQ_NOEXASC)|K_EDIT)).length < strlen(/*cfg.sys_phonefmt*/'!!!!!!!!!!!!'))
						continue; 
				}
				if(!trashcan("phone", newuser.phone))
					break; 
			}
		}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		if(system.newuser_questions & UQ_SEX) {
			console.print(bbs.text(EnterYourSex));
			newuser.sex=console.getkeys("MF");
		}
		while((system.newuser_questions & UQ_BIRTH) && bbs.online) {
			console.print(format(bbs.text(EnterYourBirthday)
				,system.settings & SYS_EURODATE ? "DD/MM/YY" : "MM/DD/YY"));
			if((newuser.birth = gettmplt(newuser.birth,"nn/nn/nn",K_EDIT)).length==8 && newuser.age)
				break; 
		}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation";
			newuser.settings |= USER_DELETED;
			return false;
		}
		while(!(system.newuser_questions & UQ_NONETMAIL) && bbs.online) {
			console.printf(bbs.text(EnterNetMailAddress));
			if((newuser.netmail = console.getstr(newuser.netmail,LEN_NETMAIL,K_EDIT|K_AUTODEL|K_LINE)) != null
				&& !system.trashcan("email", newuser.netmail))
				break;
		}
		if(newuser.netmail.length > 0 && system.settings & SYS_FWDTONET && bbs.text(ForwardMailQ).length > 0 && console.yesno(bbs.text(ForwardMailQ)))
			newuser.settings |= USER_NETMAIL;
		else 
			newuser.settings &= ~USER_NETMAIL;
		if(bbs.text(UserInfoCorrectQ).length > 0 || console.yesno(bbs.text(UserInfoCorrectQ)))
			break; 
	}
	if(!bbs.online) {
		newuser.comment = "Hung up in creation";
		newuser.settings |= USER_DELETED;
		return false;
	}
	logline(LOG_INFO, "N", "New user: "+newuser.alias);
	if(!bbs.online) {
		newuser.comment = "Hung up in creation";
		newuser.settings |= USER_DELETED;
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
	if (newuser.compare_ars(xtrn_area.editor[system.newuser_editor].ars))
		newuser.editor = system.newuser_editor;
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
				newuser.editor = editors[i-1] 
		} else
			newuser.editor = '';
	}

	// TODO: No way to do this!
	/*
	if(cfg.total_shells>1 && (cfg.uq&UQ_CMDSHELL)) {
		for(i=0;i<cfg.total_shells;i++)
			uselect(1,i,text[CommandShellHeading],cfg.shell[i]->name,cfg.shell[i]->ar);
		if((int)(i=uselect(0,useron.shell,0,0,0))>=0)
			useron.shell=i; 
	}
	*/

	// TODO: Doesn't take the user argument...
	if(bbs.rlogin_password.length > 0 && bbs.good_password(bbs.rlogin_password.length)) {
		console.crlf();
		/* passwords are case insensitive, but assumed (in some places) to be uppercase in the user database */
		newuser.security.password = bbs.rlogin_password.toUpperCase();
	}
	else {
		for (i=0; i<LEN_PASS; ) {	/* Create random password... TODO: this can be improved */
			newuser.security.password += String.fromCharCode(random(43)+'0'.charCode());
			if (newuser.security.password.substr(-1, 1).search(/[0-9A-Za-z]/) == 0)
				i++;
		}

		console.print(bbs.text(YourPasswordIs),newuser.security.password);

		if(system.settings & SYS_PWEDIT && bbs.text(NewPasswordQ).length > 0 && console.yesno(bbs.text(NewPasswordQ)))
			while(bbs.online) {
				console.print(bbs.text(NewPassword));
				str = console.getstr('',LEN_PASS,K_UPPER|K_LINE);
				str = truncsp(str);
				if(bbs.good_password(str)) {
					newuser.security.password = str;
					console.crlf();
					console.print(bbs.text(YourPasswordIs),newuser.security.password);
					break; 
				}
				console.crlf();
			}

		i=0;
		while(bbs.online) {
			console.printf(bbs.text(NewUserPasswordVerify));
			console.status |= CON_R_ECHOX;
			str = '';
			str = getstr(str,LEN_PASS*2,K_UPPER);
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
				bbs.hangup();
			}
			console.print(bbs.text(IncorrectPassword));
			console.print(format(bbs.text(YourPasswordIs),useron.pass));
		}
	}

	if(!bbs.online) {
		newuser.comment = "Hung up in creation";
		newuser.settings |= USER_DELETED;
		return false;
	}
	if(system.newuser_magic_word.length > 0) {
		console.print(bbs.text(MagicWordPrompt));
		str = '';
		str = console.getstr(str,50,K_UPPER);
		if(str != system.newuser_magic_word) {
			console.print(bbs.text(FailedMagicWord));
			SAFEPRINTF2(tmp,"%s failed magic word: '%s'",useron.alias,str);
			logline(LOG_INFO, "N!",newuser.alias+" failed magic word: '"+str+"'");
			bbs.hangup();
		}
		if(!bbs.online) {
			newuser.comment = "Hung up in creation (magic word)";
			newuser.settings |= USER_DELETED;
			return false;
		}
	}

	logline(LOG_INFO, '', "Created user record #"+newuser.number+": "+newuser.alias);

	/* TODO: No way to do this in Javascript */
	/*
	if(cfg.new_sif[0]) {
		SAFEPRINTF2(str,"%suser/%4.4u.dat",cfg.data_dir,useron.number);
		create_sif_dat(cfg.new_sif,str); 
	}
	*/
	
	/* TODO: delallmail() isn't available... do it the hard way! */
	delallmail(newuser.number, MAIL_ANY);

	/* We need to login to call user_config() */
	bbs.login(newuser.alias);
	if(!(system.newuser_questions & UQ_NODEF))
		bbs.user_config();

	if(newuser.number != 1 && bbs.node_val_user) {
		console.clear();
		console.printfile(system.text_dir+"feedback.msg", P_NOABORT);
		str = format(bbs.text(NewUserFeedbackHdr)
			,'',newuser.age,newuser.gender,newuser.birthdate
			,newuser.name,newuser.phone,newuser.computer,newuser.connection);
		bbs.email(bbs.node_val_user, WM_EMAIL|WM_SUBJ_RO|WM_FORCEFWD, str, "New User Validation");
		if(!newuser.stats.total_feedbacks && !newuser.stats.total_emails) {
			if(bbs.online)				/* didn't hang up */
				console.print(format(bbs.text(NoFeedbackWarning), system.username(bbs.node_val_user)));
		}
		bbs.email(cfg.node_valuser,str,"New User Validation",WM_EMAIL|WM_SUBJ_RO|WM_FORCEFWD);
		if(!useron.fbacks && !useron.emails) {
			if(online) {						/* didn't hang up */
				console.print(format(bbs.text(NoFeedbackWarning),system.username(bbs.node_val_user)));
				bbs.email(cfg.node_valuser,str,"New User Validation",WM_EMAIL|WM_SUBJ_RO|WM_FORCEFWD);
			} /* give 'em a 2nd try */
			if(!useron.fbacks && !useron.emails) {
        		console.print(format(bbs.text(NoFeedbackWarning),system.username(bbs.node_val_user)));
				logline(LOG_NOTICE,"N!","Aborted feedback");
				bbs.hangup();
				newuser.comment = "Didn't leave feedback";
				newuser.settings |= USER_DELETED;
				bbs.logout();
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

	return true;
}
