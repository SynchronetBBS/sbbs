if(js.global.HIGH==undefined)
	load("sbbsdefs.js");

function attr_to_ansi(atr, curatr)
{
	var str='';

	if(curatr==atr) /* text hasn't changed. don't send codes */
		return('');

	str="\033[";
	if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		str += "0;";
		curatr=LIGHTGRAY;
	}
	if(atr&BLINK) {                     /* special attributes */
		if(!(curatr&BLINK))
			str+="5;";
	}
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			str+="1;";
	}
	if((atr&0x07) != (curatr&0x07)) {
		switch(atr&0x07) {
			case BLACK:
				str+="30;";
				break;
			case RED:
				str+="31;";
				break;
			case GREEN:
				str+="32;";
				break;
			case BROWN:
				str+="33;";
				break;
			case BLUE:
				str+="34;";
				break;
			case MAGENTA:
				str+="35;";
				break;
			case CYAN:
				str+="36;";
				break;
			case LIGHTGRAY:
				str+="37;";
				break;
		}
	}
	if((atr&0x70) != (curatr&0x70)) {
		switch(atr&0x70) {
			/* The BG_BLACK macro is 0x200, so isn't in the mask */
			case 0 /* BG_BLACK */:	
				str+="40;";
				break;
			case BG_RED:
				str+="41;";
				break;
			case BG_GREEN:
				str+="42;";
				break;
			case BG_BROWN:
				str+="43;";
				break;
			case BG_BLUE:
				str+="44;";
				break;
			case BG_MAGENTA:
				str+="45;";
				break;
			case BG_CYAN:
				str+="46;";
				break;
			case BG_LIGHTGRAY:
				str+="47;";
				break;
		}
	}
	if(str.length==2)
		return('');
	str=str.substr(-1)+'m';
	return(str);
}

function expand_body(body, sys_misc, mode)
{
	if(body==undefined || sys_misc==undefined)
		return(body);
	if(mode==undefined)
		mode=P_NOATCODES;
	var cur_attr=7;

	// CTRL-A (not yet)
//		if(str[l]==CTRL_A && str[l+1]!=0) {
//			if(str[l+1]=='"' && !(sys_status&SS_NEST_PF)) {  /* Quote a file */
//				l+=2;
//				i=0;
//				while(i<12 && isprint(str[l]) && str[l]!='\\' && str[l]!='/')
//					tmp2[i++]=str[l++];
//				tmp2[i]=0;
//				sys_status|=SS_NEST_PF; 	/* keep it only one message deep! */
//				sprintf(tmp3,"%s%s",cfg.text_dir,tmp2);
//				printfile(tmp3,0);
//				sys_status&=~SS_NEST_PF; 
//			}
//			else if(toupper(str[l+1])=='Z')             /* Ctrl-AZ==EOF */
//				break;
//			else {
//				ctrl_a(str[l+1]);
//				l+=2; 
//			} 
//		}
	body=body.replace(/[`ú]\[/g, '\x1b['); /* Convert `[ and ú[ to ESC[ */
	if(sys_misc & SYS_PCBOARD) {
		body=body.replace(/@X([0-9a-zA-Z]{2})/g, function (str, code) {
			var new_attr=parseInt(code, 16);
			var ret=attr_to_ansi(new_attr,cur_attr);
			cur_attr=new_attr;
			return(ret);
		});
	}
	if(sys_misc & SYS_WILDCAT) {
		body=body.replace(/@([0-9a-zA-Z]{2})@/g, function (str, code) {
			var new_attr=parseInt(code, 16);
			var ret=attr_to_ansi(new_attr,cur_attr);
			cur_attr=new_attr;
			return(ret);
		});
	}
	if(sys_misc & SYS_RENEGADE) {
		body=body.replace(/|([0-9]{2})/g, function (str, code) {
			var new_attr=parseInt(code);

			if(new_attr >= 16) {
				new_attr -= 16;
				new_attr <<= 4;
				new_attr |= cur_attr & 0x0f;
			}
			else {
				new_attr |= (cur_attr & 0xf0);
			}

			var ret=attr_to_ansi(new_attr,cur_attr);
			cur_attr=new_attr;
			return(ret);
		});
	}
	if(sys_misc & SYS_CELERITY) {
		body=body.replace(/|(a-zA-Z])/g, function (str, code) {
			var new_attr;
			switch(code) {
				case 'k':
					new_attr = (cur_attr&0xf0)|BLACK;
					break;
				case 'b':
					new_attr = (cur_attr&0xf0)|BLUE;
					break;
				case 'g':
					new_attr = (cur_attr&0xf0)|GREEN;
					break;
				case 'c':
					new_attr = (cur_attr&0xf0)|CYAN;
					break;
				case 'r':
					new_attr = (cur_attr&0xf0)|RED;
					break;
				case 'm':
					new_attr = (cur_attr&0xf0)|MAGENTA;
					break;
				case 'y':
					new_attr = (cur_attr&0xf0)|YELLOW;
					break;
				case 'w':
					new_attr = (cur_attr&0xf0)|LIGHTGRAY;
					break;
				case 'd':
					new_attr = (cur_attr&0xf0)|BLACK|HIGH;
					break;
				case 'B':
					new_attr = (cur_attr&0xf0)|BLUE|HIGH;
					break;
				case 'G':
					new_attr = (cur_attr&0xf0)|GREEN|HIGH;
					break;
				case 'C':
					new_attr = (cur_attr&0xf0)|CYAN|HIGH;
					break;
				case 'R':
					new_attr = (cur_attr&0xf0)|RED|HIGH;
					break;
				case 'M':
					new_attr = (cur_attr&0xf0)|MAGENTA|HIGH;
					break;
				case 'Y':   /* Yellow */
					new_attr = (cur_attr&0xf0)|YELLOW|HIGH;
					break;
				case 'W':
					new_attr = (cur_attr&0xf0)|LIGHTGRAY|HIGH;
					break;
				case 'S':   /* swap foreground and background */
					new_attr = (cur_attr&0x07)<<4;
					break; 
			}
			var ret=attr_to_ansi(new_attr,cur_attr);
			cur_attr=new_attr;
			return(ret);
		});
	}
	if(sys_misc & SYS_WWIV) {
		body=body.replace(/\3([0-9])/g, function (str, code) {
			var new_attr;
			switch(str[l+1]) {
				default:
					new_attr=LIGHTGRAY;
					break;
				case '1':
					new_attr=CYAN|HIGH;
					break;
				case '2':
					new_attr=BROWN|HIGH;
					break;
				case '3':
					new_attr=MAGENTA;
					break;
				case '4':
					new_attr=LIGHTGRAY|HIGH|BG_BLUE;
					break;
				case '5':
					new_attr=GREEN;
					break;
				case '6':
					new_attr=RED|HIGH|BLINK;
					break;
				case '7':
					new_attr=BLUE|HIGH;
					break;
				case '8':
					new_attr=BLUE;
					break;
				case '9':
					new_attr=CYAN;
					break; 
			}

			var ret=attr_to_ansi(new_attr,cur_attr);
			cur_attr=new_attr;
			return(ret);
		});
	}

	// TODO: /* clear at newline for extra attr codes */
	/* expand sole LF to CR/LF */
	body=body.replace(/^\n/, '\r\n');
	body=body.replace(/([^\r])\n/g, '$1\r\n');

	body=body.replace(/@([^@]*)@/g, function(matched, code) {
		var fmt="%s";
		ma=new Array();
		if((ma=code.match(/^(.*)-L.*$/))!=undefined) {
			fmt="%-"+(code.length)+"s";
			code=ma[1];
		}
		if((ma=code.match(/^(.*)-R.*$/))!=undefined) {
			fmt="%"+(code.length)+"s";
			code=ma[1];
		}
		switch(code.toUpperCase()) {
			case 'BBS':
				return(format(fmt,system.name.toString()));
			case 'LOCATION':
				return(format(fmt,system.location.toString()));
			case 'SYSOP':
				return(format(fmt,system.operator.toString()));
			case 'HOSTNAME':
				return(format(fmt,system.host_name.toString()));
			case 'OS_VER':
				return(format(fmt,system.os_version.toString()));
			case 'UPTIME':
				var days=0;
				var hours=0;
				var min=0;
				var seconds=0;
				var ut=time()-system.uptime;
				days=(ut/86400);
				ut%=86400;
				hours=(ut/3600);
				ut%=3600;
				mins=(ut/60);
				secsonds=parseInt(ut%60);
				if(parseInt(days)!=0)
					ut=format("%d days %d:%02d",days,hours,mins);
				else
					ut=format("%d:%02d",hours,mins);
				return(format(fmt,ut.toString()));
			case 'TUSER':
				return(format(fmt,system.stats.total_users.toString()));
			case 'STATS.NUSERS':
				return(format(fmt,system.stats.new_users_today.toString()));
			case 'STATS.LOGONS':
				return(format(fmt,system.stats.total_logons.toString()));
			case 'STATS.LTODAY':
				return(format(fmt,system.stats.logons_today.toString()));
			case 'STATS.TIMEON':
				return(format(fmt,system.stats.total_timeon.toString()));
			case 'STATS.TTODAY':
				return(format(fmt,system.stats.timeon_today.toString()));
			case 'TMSG':
				return(format(fmt,system.stats.total_messages.toString()));
			case 'STATS.PTODAY':
				return(format(fmt,system.stats.messages_posted_today.toString()));
			case 'MAILW:0':
				return(format(fmt,system.stats.total_email.toString()));
			case 'STATS.ETODAY':
				return(format(fmt,system.stats.email_sent_today.toString()));
			case 'MAILW:1':
				return(format(fmt,system.stats.total_feedback.toString()));
			case 'STATS.FTODAY':
				return(format(fmt,system.stats.feedback_sent_today.toString()));
			case 'TFILE':
				return(format(fmt,system.stats.total_files.toString()));
			case 'STATS.ULS':
				return(format(fmt,system.stats.files_uploaded_today.toString()));
			case 'STATS.DLS':
				return(format(fmt,system.stats.files_downloaded_today.toString()));
			case 'STATS.DLB':
				return(format(fmt,system.stats.bytes_downloaded_today.toString()));
			default:
				return('@'+code+'@');
		}
	});

	return(body);
}
