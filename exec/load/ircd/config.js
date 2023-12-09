/*

 ircd/config.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Anything that handles manipulating the IRCd configuration.
 
 BE AWARE that this file is loaded directly by programs other than the
 IRCd, for example the UIFC interface for making config changes.

 Copyright 2003-2023 Randy Sommerfeld <cyan@synchro.net>

*/

/* Various permissions that can be set on an O:Line */
const OLINE_CAN_REHASH		=(1<<0);	/* r */
const OLINE_CAN_RESTART		=(1<<1);	/* R */
const OLINE_CAN_DIE			=(1<<2);	/* D */
const OLINE_CAN_GLOBOPS		=(1<<3);	/* g */
const OLINE_CAN_WALLOPS		=(1<<4);	/* w */
const OLINE_CAN_LOCOPS		=(1<<5);	/* l */
const OLINE_CAN_LSQUITCON	=(1<<6);	/* c */
const OLINE_CAN_GSQUITCON	=(1<<7);	/* C */
const OLINE_CAN_LKILL		=(1<<8);	/* k */
const OLINE_CAN_GKILL		=(1<<9);	/* K */
const OLINE_CAN_KLINE		=(1<<10);	/* b */
const OLINE_CAN_UNKLINE		=(1<<11);	/* B */
const OLINE_CAN_LGNOTICE	=(1<<12);	/* n */
const OLINE_CAN_GGNOTICE	=(1<<13);	/* N */
const OLINE_IS_ADMIN		=(1<<14);	/* A */
/* Synchronet IRCd doesn't have umode +a	RESERVED */
const OLINE_CAN_UMODEC		=(1<<16);	/* c */
const OLINE_CAN_CHATOPS		=(1<<19);	/* s */
const OLINE_CHECK_SYSPASSWD	=(1<<20);	/* S */
const OLINE_CAN_EVAL		=(1<<21);	/* x */
const OLINE_IS_GOPER		=(1<<22);	/*  "big O" */

function Epoch() {
	return parseInt(new Date().getTime()/1000);
}

function true_array_len(my_array) {
	var i;
	var counter = 0;
	for (i in my_array) {
		counter++;
	}
	return counter;
}

function create_ban_mask(str,kline) {
	var tmp_banstr = new Array;
	tmp_banstr[0] = "";
	tmp_banstr[1] = "";
	tmp_banstr[2] = "";
	var i;
	var bchar_counter = 0;
	var part_counter = 0; // BAN: 0!1@2 KLINE: 0@1
	var regexp="[A-Za-z\{\}\`\^\_\|\\]\\[\\\\0-9\-.*?\~:]";
	var finalstr;
	for (i in str) {
		if (str[i].match(regexp)) {
			tmp_banstr[part_counter] += str[i];
			bchar_counter++;
		} else if ((str[i] == "!") && (part_counter == 0) &&
			   !kline) {
			part_counter = 1;
			bchar_counter = 0;
		} else if ((str[i] == "@") && (part_counter == 1) &&
			   !kline) {
			part_counter = 2;
			bchar_counter = 0;
		} else if ((str[i] == "@") && (part_counter == 0)) {
			if (kline) {
				part_counter = 1;
			} else {
				tmp_banstr[1] = tmp_banstr[0];
				tmp_banstr[0] = "*";
				part_counter = 2;
			}
			bchar_counter = 0;
		}
	}
	if (!tmp_banstr[0] && !tmp_banstr[1] && !tmp_banstr[2])
		return 0;
	if (tmp_banstr[0].match(/[.]/) && !tmp_banstr[1] && !tmp_banstr[2]) {
		if (kline)
			tmp_banstr[1] = tmp_banstr[0];
		else
			tmp_banstr[2] = tmp_banstr[0];
		tmp_banstr[0] = "";
	}
	if (!tmp_banstr[0])
		tmp_banstr[0] = "*";
	if (!tmp_banstr[1])
		tmp_banstr[1] = "*";
	if (!tmp_banstr[2] && !kline)
		tmp_banstr[2] = "*";
	if (kline)
		finalstr = tmp_banstr[0].slice(0,10) + "@" + tmp_banstr[1].slice(0,80);
	else
		finalstr = format("%s!%s@%s",
			tmp_banstr[0].slice(0,MAX_NICKLEN),
			tmp_banstr[1].slice(0,10),
			tmp_banstr[2].slice(0,80)
		);
		while (finalstr.match(/[*][*]/)) {
			finalstr=finalstr.replace(/[*][*]/g,"*");
		}
	return finalstr;
}

function parse_nline_flags(flags) {
	var i;
	var nline_flags = 0;

	for (i in flags) {
		switch(flags[i]) {
			case "q":
				nline_flags |= NLINE_CHECK_QWKPASSWD;
				break;
			case "w":
				nline_flags |= NLINE_IS_QWKMASTER;
				break;
			case "k":
				nline_flags |= NLINE_CHECK_WITH_QWKMASTER;
				break;
			default:
				log(LOG_WARNING,format(
					"!WARNING Unknown N:Line flag '%s' in config.",
					flags[i]
				));
				break;
		}
	}
	return nline_flags;
}

function parse_oline_flags(flags) {
	var i;
	var oline_flags = 0;

	for (i in flags) {
		switch(flags[i]) {
			case "r":
				oline_flags |= OLINE_CAN_REHASH;
				break;
			case "R":
				oline_flags |= OLINE_CAN_RESTART;
				break;
			case "D":
				oline_flags |= OLINE_CAN_DIE;
				break;
			case "g":
				oline_flags |= OLINE_CAN_GLOBOPS;
				break;
			case "w":
				oline_flags |= OLINE_CAN_WALLOPS;
				break;
			case "l":
				oline_flags |= OLINE_CAN_LOCOPS;
				break;
			case "c":
				oline_flags |= OLINE_CAN_LSQUITCON;
				break;
			case "C":
				oline_flags |= OLINE_CAN_GSQUITCON;
				break;
			case "k":
				oline_flags |= OLINE_CAN_LKILL;
				break;
			case "K":
				oline_flags |= OLINE_CAN_GKILL;
				break;
			case "b":
				oline_flags |= OLINE_CAN_KLINE;
				break;
			case "B":
				oline_flags |= OLINE_CAN_UNKLINE;
				break;
			case "n":
				oline_flags |= OLINE_CAN_LGNOTICE;
				break;
			case "N":
				oline_flags |= OLINE_CAN_GGNOTICE;
				break;
			case "u":
				oline_flags |= OLINE_CAN_UMODEC;
				break;
			case "A":
				oline_flags |= OLINE_IS_ADMIN;
				break;
			case "a":
			case "f":
			case "F":
				break; // All reserved for future use.
			case "s":
				oline_flags |= OLINE_CAN_CHATOPS;
				break;
			case "S":
				oline_flags |= OLINE_CHECK_SYSPASSWD;
				break;
			case "x":
			case "X":
				oline_flags |= OLINE_CAN_EVAL;
				break;
			case "O":
				oline_flags |= OLINE_IS_GOPER;
				oline_flags |= OLINE_CAN_GSQUITCON;
				oline_flags |= OLINE_CAN_GKILL;
				oline_flags |= OLINE_CAN_GGNOTICE;
				oline_flags |= OLINE_CAN_CHATOPS;
			case "o":
				oline_flags |= OLINE_CAN_REHASH;
				oline_flags |= OLINE_CAN_GLOBOPS;
				oline_flags |= OLINE_CAN_WALLOPS;
				oline_flags |= OLINE_CAN_LOCOPS;
				oline_flags |= OLINE_CAN_LSQUITCON;
				oline_flags |= OLINE_CAN_LKILL;
				oline_flags |= OLINE_CAN_KLINE;
				oline_flags |= OLINE_CAN_UNKLINE;
				oline_flags |= OLINE_CAN_LGNOTICE;
				oline_flags |= OLINE_CAN_UMODEC;
				break;
			default:
				log(LOG_WARNING,format(
					"!WARNING Unknown O:Line flag '%s' in config.",
					flags[i]
				));
				break;
		}
	}
	return oline_flags;
}

function Clear_Config_Globals() {
	Admin1 = "";
	Admin2 = "";
	Admin3 = "";
	if (typeof CLines !== 'undefined') {
		for (i in CLines) {
			if (CLines[i].next_connect) {
				js.clearTimeout(CLines[i].next_connect);
				delete CLines[i].next_connect;
			}
		}
	}
	CLines = [];
	HLines = [];
	ILines = [];
	KLines = [];
	NLines = [];
	OLines = [];
	PLines = [];
	QLines = [];
	ULines = [];
	YLines = [];
	ZLines = [];
	RBL = [];
	Die_Password = "";
	Restart_Password = "";
}

function Read_Config_File() {
	var i, c;

	Clear_Config_Globals();

	var fname="";
	if (Config_Filename && Config_Filename.length) {
		if(Config_Filename.indexOf('/')>=0 || Config_Filename.indexOf('\\')>=0)
			fname=Config_Filename;
		else
			fname=system.ctrl_dir + Config_Filename;
	} else {
		fname=system.ctrl_dir + "ircd." + system.local_host_name + ".ini";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.local_host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.host_name + ".ini";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd.ini";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd.conf";
	}
	if (!IRCDCFG_Editor)
		log(LOG_INFO,"Trying to read configuration from: " + fname);
	var file_handle = new File(fname);
	if (file_handle.open("r")) {
		if (fname.substr(fname.length-3,3) == "ini")
			read_ini_config(file_handle);
		else
			read_conf_config(file_handle);
		file_handle.close();
	} else if (!IRCDCFG_Editor) {
		log(LOG_NOTICE, "Couldn't open configuration file! Proceeding with defaults only.");
	}

	if ((true_array_len(ILines) < 1) && !IRCDCFG_Editor)
		log(LOG_WARNING, "!WARNING Nobody appears to be allowed to connect - configure in [Allow]");

	Time_Config_Read = Epoch();

	YLines[0] = new YLine(120,600,100,1000000); /* Hardcoded class for fallback */

	if (IRCDCFG_Editor)
		return;

	Scan_For_Banned_Clients();
	for (i in CLines) {
		c = CLines[i];
		if (!YLines[c.ircclass]) {
			log(LOG_WARNING, format(
				"!WARNING IRC Class %u defined for server %s doesn't exist. Using Class 0.",
				c.ircclass,
				c.servername
			));
			c.ircclass = 0;
		}
		if ((YLines[c.ircclass].connfreq > 0) && c.port && !Servers[c.servername.toLowerCase()])
			Reset_Autoconnect(c, 1 /* connect immediately */);
	}
}

function Write_Config_File(fn) {
	var f = new File(fn);
	var c, i;

	if (!f.open('w'))
		return false;

	/* [Info] */
	f.iniSetValue("Info", "Servername", ServerName);
	f.iniSetValue("Info", "Description", ServerDesc);
	f.iniSetValue("Info", "Admin1", Admin1);
	f.iniSetValue("Info", "Admin2", Admin2);
	f.iniSetValue("Info", "Admin3", Admin3);

	/* [Port] */
	f.iniSetValue("Port:" + Default_Port, "Default", "true");
	for (i in PLines) {
		f.iniSetValue("Port:" + PLines[i], "Default", "false");
	}

	/* [Class] */
	for (i in YLines) {
		if (i == 0)
			continue;
		if (YLines[i].comment)
			f.iniSetValue("Class:" + i, "Comment", YLines[i].comment);
		f.iniSetValue("Class:" + i, "PingFrequency", YLines[i].pingfreq);
		f.iniSetValue("Class:" + i, "ConnectFrequency", YLines[i].connfreq);
		f.iniSetValue("Class:" + i, "Maximum", YLines[i].maxlinks);
		f.iniSetValue("Class:" + i, "SendQ", YLines[i].sendq);
	}

	/* [Allow] */
	c = 0;
	for (i in ILines) {
		c++;
		f.iniSetValue("Allow:" + c, "Mask", ILines[i].hostmask);
		f.iniSetValue("Allow:" + c, "Class", ILines[i].ircclass);
	}

	/* [Operator] */
	c = 0;
	for (i in OLines) {
		c++;
		f.iniSetValue("Operator:" + c, "Nick", OLines[i].nick);
		f.iniSetValue("Operator:" + c, "Mask", OLines[i].hostmask);
		f.iniSetValue("Operator:" + c, "Password", OLines[i].password);
		f.iniSetValue("Operator:" + c, "Flags", OLines[i].flags);
		f.iniSetValue("Operator:" + c, "Class", OLines[i].ircclass);
	}

	/* [Services] */
	c = 0;
	for (i in ULines) {
		c++;
		f.iniSetValue("Services:" + c, "Servername", ULines[i]);
	}

	/* [Ban] */
	c = 0;
	for (i in KLines) {
		if (KLines[i].type != "K")
			continue;
		c++;
		f.iniSetValue("Ban:" + c, "Hostmask", KLines[i].hostmask);
		f.iniSetValue("Ban:" + c, "Reason", KLines[i].reason);
	}

	/* [Server] */
	c = 0;
	for (i in CLines) {
		c++;
		f.iniSetValue("Server:" + c, "Servername", CLines[i].servername);
		f.iniSetValue("Server:" + c, "Hostname", CLines[i].host);
		f.iniSetValue("Server:" + c, "Port", CLines[i].port);
		f.iniSetValue("Server:" + c, "InboundPassword", NLines[i].password);
		f.iniSetValue("Server:" + c, "OutboundPassword", CLines[i].password);
		f.iniSetValue("Server:" + c, "Class", CLines[i].ircclass);
		if (NLines[i].flags)
			f.iniSetValue("Server:" + c, "Flags", NLines[i].flags);
		f.iniSetValue(
			"Server:" + c,
			"Hub",
			HLine_Exists(CLines[i].servername) ? true : false
		);
	}

	/* [Restrict] */
	c = 0;
	for (i in QLines) {
		c++;
		f.iniSetValue("Restrict:" + c, "Mask", QLines[i].nick);
		f.iniSetValue("Restrict:" + c, "Reason", QLines[i].reason);
	}

	/* [RBL] */
	c = 0;
	for (i in RBL) {
		c++;
		f.iniSetValue("RBL:" + c, "Hostname", RBL[i].hostname);
		if (RBL[i].good)
			f.iniSetValue("RBL:" + c, "GoodResponses", RBL[i].good.join(","));
		if (RBL[i].bad)
			f.iniSetValue("RBL:" + c, "BadResponses", RBL[i].bad.join(","));
	}

	f.close();

	return true;
}

function HLine_Exists(str) {
	var i;

	for (i in HLines) {
		if (HLines[i].servername.toLowerCase() == str.toLowerCase())
			return true;
	}
	return false;
}

function ini_sections() {
	this.Info = ini_Info;
	this.Port = ini_Port;
	this.Class = ini_Class;
	this.Allow = ini_Allow;
	this.Operator = ini_Operator;
	this.Services = ini_Services;
	this.Ban = ini_Ban;
	this.Restrict = ini_Restrict;
	this.RBL = ini_RBL;
	this.Server = ini_Server;
	this.Hub = ini_Hub;
}

function ini_Info(arg, ini) {
	if (ini.Servername)
		ServerName = ini.Servername;
	if (ini.Description)
		ServerDesc = ini.Description;
	if (ini.Admin1)
		Admin1 = ini.Admin1;
	if (ini.Admin2)
		Admin2 = ini.Admin2;
	if (ini.Admin3)
		Admin3 = ini.Admin3;
}

/* Former M:Line and P:Line combined into one */
function ini_Port(arg, ini) {
	var port = parseInt(arg);
	if (port != arg) {
		log(LOG_WARNING,format(
			"!WARNING Possible malformed port number in .ini: %s vs %u - section ignored.",
			arg,
			port
		));
		return;
	}
	if (ini_false_true(ini.Default)) {
		Default_Port = arg;
		return;
	}
	PLines.push(arg);
	return;
}

/* Former Y:Line */
/* Eventually it'd be nice to move to named classes instead of numbers. */
function ini_Class(arg, ini) {
	var ircclass = parseInt(arg);

	if (ircclass != arg) {
		log(LOG_WARNING,format(
			"!WARNING Possible malformed IRC class in .ini: %s vs %u - section ignored.",
			arg,
			ircclass
		));
		return;
	}

	YLines[ircclass] = new YLine(
		ini_int_min_max(ini.PingFrequency, 1, 999, 60, format("PingFrequency IRC class %u",ircclass)),
		ini_int_min_max(ini.ConnectFrequency, 0, 9999, 60, format("ConnectFrequency IRC class %u",ircclass)),
		ini_int_min_max(ini.Maximum, 1, 99999, 100, format("Maximum IRC class %u",ircclass)),
		ini_int_min_max(ini.SendQ, 2048, 999999999, 1000000, format("SendQ IRC class %u",ircclass))
	);
	if (ini.Comment)
		YLines[ircclass].comment = ini.Comment;
}

/* Former I:Line */
function ini_Allow(arg, ini) {
	var ircclass, masks, i;

	if (!ini.Mask) {
		log(LOG_WARNING,format(
			"!WARNING No mask found in [Allow:%s], ignoring this section.",
			arg
		))
		return;
	}

	ircclass = parseInt(ini.Class);
	if (ircclass != ini.Class) {
		log(LOG_WARNING,format(
			"!WARNING No IRC class or malformed IRC class in Allow:%s. Using class of 0.",
			arg
		));
		ircclass = 0;
	}

	masks = ini.Mask.split(",");

	for (i in masks) {
		if (!masks[i].match("[@]")) {
			log(LOG_WARNING,format(
				"!WARNING Malformed mask in [Allow:%s]: %s. Ignoring.",
				arg,
				masks[i]
			));
			continue;
		}
		ILines.push(new ILine(
			masks[i],
			null, /* password */
			masks[i], /* hostmask */
			null, /* port */
			ircclass
		));
	}
}

/* Former O:Line */
function ini_Operator(arg, ini) {
	var ircclass, masks, i;

	if (!ini.Nick || !ini.Mask || !ini.Password || !ini.Flags || !ini.Class) {
		log(LOG_WARNING,format(
			"!WARNING Missing information from Operator:%s. Section ignored.",
			arg
		));
		return;
	}

	if (!ini.Mask.match("[@]")) {
		log(LOG_WARNING,format(
			"!WARNING Malformed mask in Operator:%s. Proper format is user@hostname with wildcards.",
			arg
		));
		return;
	}

	ircclass = parseInt(ini.Class);
	if (ircclass != ini.Class) {
		log(LOG_WARNING,format(
			"!WARNING Malformed IRC Class in Operator:%s. Proceeding with Class 0.",
			arg
		));
		ircclass = 0;
	}

	masks = ini.Mask.split(",");

	for (i in masks) {
		if (!masks[i].match("[@]")) {
			log(LOG_WARNING,format(
				"!WARNING Malformed mask in [Operator:%s] %s. Ignored.",
				arg,
				masks[i]
			));
			continue;
		}
		OLines.push(new OLine(
			masks[i],
			ini.Password,
			ini.Nick,
			parse_oline_flags(ini.Flags),
			ircclass
		));
	}

	return;
}

/* Former U:Line */
function ini_Services(arg, ini) {
	if (ini.Servername)
		ULines.push(ini.Servername);
	return;
}

/* Former K:Line & Z:Line */
function ini_Ban(arg, ini) {
	var kline_mask, masks, i;

	if (!ini.Mask) {
		log(LOG_WARNING,format(
			"!WARNING No mask provided in Ban:%s. Ignoring.",
			arg
		));
		return;
	}

	masks = ini.Mask.split(",");

	for (i in masks) {
		kline_mask = create_ban_mask(masks[i], true /* kline */);

		if (!kline_mask) {
			log(LOG_WARNING,format(
					"!WARNING Invalid ban mask in [Ban:%s] %s Ignoring.",
					arg,
					masks[i]
			));
			continue;
		}

		KLines.push(new KLine(
			kline_mask,
			ini.Reason ? ini.Reason : "No reason provided.",
			"K" /* ban type: K, A, or Z. */
		));	
	}
}

/* Former H:Line */
/* Servermasks are deprecated */
function ini_Hub(arg, ini) {
	if (HLine_Exists(ini.Servername))
		return;
	HLines.push(new HLine(
		"*", /* servermask permitted */
		ini.Servername /* servername */
	));
	return;
}

/* Former C:Line and N:Line */
function ini_Server(arg, ini) {
	var ircclass, port;

	if (!ini.Servername || !ini.Hostname || !ini.Class) {
		log(LOG_WARNING,format(
			"!WARNING Missing information from [Server:%s], Section ignored.",
			arg
		));
		return;
	}

	if ((!ini.InboundPassword || !ini.OutboundPassword) && !ini.Password) {
		log(LOG_WARNING,format(
			"!WARNING No password provided for [Server:%s], Section ignored.",
			arg
		));
		return;
	}

	port = parseInt(ini.Port);
	if (port != ini.Port)
		port = 0;

	ircclass = parseInt(ini.Class);
	if (ircclass != ini.Class) {
		log(LOG_WARNING,format(
			"!WARNING Malformed IRC Class in [Server:%s], Using default class of 0.",
			arg
		));
		ircclass = 0;
	}

	if (ini_false_true(ini.Hub) && !HLine_Exists(ini.Servername))
		HLines.push(new HLine("*", ini.Servername));

	CLines.push(new CLine(
		ini.Hostname,
		ini.OutboundPassword ? ini.OutboundPassword : ini.Password,
		ini.Servername,
		port,
		ircclass
	));
	NLines.push(new NLine(
		ini.Hostname,
		ini.InboundPassword ? ini.InboundPassword : ini.Password,
		ini.Servername,
		parse_nline_flags(ini.Flags),
		ircclass
	));
}

/* Former Q:Lines */
function ini_Restrict(arg, ini) {
	var masks, i;

	if (!ini.Mask) {
		log(LOG_WARNING,format(
			"!WARNING Missing Mask from Restrict:%s. Section ignored.",
			arg
		));
		return;
	}

	masks = ini.Mask.split(",");

	for (i in masks) {
		QLines.push(new QLine(
			masks[i],
			ini.Reason ? ini.Reason : "No reason provided."
		));
	}
}

function ini_RBL(arg, ini) {
	if (!ini.Hostname) {
		log(LOG_WARNING,format(
			"!WARNING No Hostname in RBL:%s. Ignoring.",
			arg
		));
		return;
	}
	RBL.push(new RBL_Config_Object(ini.Hostname, ini.GoodResponses, ini.BadResponses));
}

function load_config_defaults() {
	/*** M:Line ***/
	ServerName = format("%s.synchro.net", system.qwk_id.toLowerCase());
	ServerDesc = system.name;
	Default_Port = 6667;
	/*** A:Line ***/
	Admin1 = format("%s (%s)", system.name, system.qwk_id);
	Admin2 = system.version_notice;
	Admin3 = format("Sysop- <sysop@%s>", system.host_name);
	/*** Y:Line *** ping freq, connect freq, max clients, max sendq bytes */
	YLines[0] = new YLine(120,600,100,1000000);
	/*** I:Line *** mask, password, hostmask, port, class */
	/*** U:Line ***/
	ULines.push("services.synchro.net");
	ULines.push("stats.synchro.net");
	/*** K:Line *** deliberately empty by default */
	/*** Z:Line *** deprecated and combined with above */
	/*** Q:Line ***/
	QLines.push(new QLine("*Serv", "Reserved for Services"));
	QLines.push(new QLine("Global", "Reserved for Services"));
	QLines.push(new QLine("IRCOp*", "Reserved for IRC Operators"));
	QLines.push(new QLine("Sysop", "Reserved for Sysop"));
	/*** H:Line ***/
	HLines.push(new HLine("*", "vert.synchro.net"));
	HLines.push(new HLine("*", "cvs.synchro.net"));
	HLines.push(new HLine("*", "hub.synchro.net"));
	/*** P:Line *** deliberately empty by default */
}

function read_ini_config(conf) {
	var ini = conf.iniGetAllObjects();
	var Sections = new ini_sections();
	var i, s;

	load_config_defaults();

	for (i in ini) {
		if (ini_false_true(ini[i].disabled) || ini_false_true(ini[i].Disabled))
			continue;
		if (!ini_true_false(ini[i].enabled) || !ini_true_false(ini[i].Enabled))
			continue;
		s = ini[i].name.split(":");
		if (typeof Sections[s[0]] === 'function')
			Sections[s[0]](s[1], ini[i]);
	}
}

function ini_int_min_max(str, min, max, def, desc) {
	var input = parseInt(str);
	if (input != str) {
		log(LOG_WARNING,format(
			"!WARNING Malformed value (%s vs. %u) in %s. Using default of %u.",
			str,
			input,
			desc,
			def
		));
		return def;
	}
	if (input < min) {
		log(LOG_WARNING,format(
			"!WARNING Value %u too low in %s. Using default of %u.",
			input,
			desc,
			def
		));
		return def;
	}
	if (input > max) {
		log(LOG_WARNING,format(
			"!WARNING Value %u too high in %s. Consider lowering it.",
			input,
			desc,
			def
		));
	}
	return input;
}

function ini_false_true(str) {
	if (typeof str === "boolean")
		return str;
	if (typeof str !== "string")
		return false;
	str = str.toUpperCase();
	switch (str[0]) {
		case "1":
		case "Y":
		case "T":
			return true;
		default:
			return false;
	}
}

function ini_true_false(str) {
	if (typeof str === "boolean")
		return str;
	if (typeof str !== "string")
		return true;
	str = str.toUpperCase();
	switch (str[0]) {
		case "0":
		case "N":
		case "F":
			return false;
		default:
			return true;
	}
}


function read_conf_config(conf) {
	var conf_line, arg, i;

	load_config_defaults();

	function fancy_split(line) {
		var ret = [];
		var i;
		var s = 0;
		var inb = false;
		var str;

		for (i = 0; i < line.length; i++) {
			if (line[i] == ':') {
				if (inb)
					continue;
				if (i > 0 && line[i-1] == ']')
					str = line.slice(s, i-1);
				else
					str = line.slice(s, i);
				ret.push(str);
				s = i + 1;
			}
			else if (!inb && line[i] == '[' && s == i) {
				inb = true;
				s = i + 1;
			}
			else if (line[i] == ']' && (i+1 == line.length || line[i+1] == ':')) {
				inb = false;
			}
		}
		if (s < line.length) {
			if (i > 0 && line[i-1] == ']')
				str = line.slice(s, i-1);
			else
				str = line.slice(s, i);
			ret.push(str);
		}

		return ret;
	}

	while (!conf.eof) {
		conf_line = conf.readln();
		if ((conf_line != null) && conf_line.match("[:]")) {
			arg = fancy_split(conf_line);
			for (i in arg) {
				arg[i]=arg[i].replace(/SYSTEM_HOST_NAME/g,system.host_name);
				arg[i]=arg[i].replace(/SYSTEM_NAME/g,system.name);
				arg[i]=arg[i].replace(/VERSION_NOTICE/g,system.version_notice);
				if (typeof system.qwk_id !== 'undefined')
					arg[i]=arg[i].replace(/SYSTEM_QWKID/g,system.qwk_id.toLowerCase());
			}
		switch (conf_line[0].toUpperCase()) {
				case "A":
					if (!arg[3])
						break;
					Admin1 = arg[1];
					Admin2 = arg[2];
					Admin3 = arg[3];
					break;
				case "C":
					if (!arg[5])
						break;
					CLines.push(new CLine(arg[1],arg[2],arg[3],arg[4],parseInt(arg[5])));
					break;
				case "H":
					if (!arg[3])
						break;
					HLines.push(new HLine(arg[1],arg[3]));
					break;
				case "I":
					if (!arg[5] || !parseInt(arg[5]))
						break;
					ILines.push(new ILine(arg[1],arg[2],arg[3],arg[4],parseInt(arg[5])));
					break;
				case "K":
					if (!arg[2])
						break;
					var kline_mask = create_ban_mask(arg[1],true);
					if (!kline_mask) {
						log(LOG_WARNING,"!WARNING Invalid K:Line (" + arg[1] + ")");
						break;
					}
					KLines.push(new KLine(kline_mask,arg[2],"K"));
					break;
				case "M":
					if (!arg[3])
						break;
					ServerName = arg[1];
					ServerDesc = arg[3];
					Default_Port = parseInt(arg[4]);
					break;
				case "N":
					if (!arg[5])
						break;
					NLines.push(new NLine(arg[1],arg[2],arg[3],
						parse_nline_flags(arg[4]),arg[5]) );
					break;
				case "O":
					if (!arg[5])
						break;
					OLines.push(new OLine(arg[1],arg[2],arg[3],
						parse_oline_flags(arg[4]),parseInt(arg[5]) ));
					break;
				case "P":
					PLines.push(parseInt(arg[4]));
					break;
				case "Q":
					if (!arg[3])
						break;
					QLines.push(new QLine(arg[3],arg[2]));
					break;
				case "U":
					if (!arg[1])
						break;
					ULines.push(arg[1]);
					break;
				case "X":
					Die_Password = arg[1];
					Restart_Password = arg[2];
					break;
				case "Y":
					if (!arg[5])
						break;
					YLines[parseInt(arg[1])] = new YLine(parseInt(arg[2]),
						parseInt(arg[3]),parseInt(arg[4]),parseInt(arg[5]));
					break;
				case "Z":
					if (!arg[2])
						break;
					ZLines.push(new ZLine(arg[1],arg[2]));
					break;
				case "#":
				case ";":
				default:
					break;
			}
		}
	}
}

function CLine(host,password,servername,port,ircclass) {
	this.host = host;
	this.password = password;
	this.servername = servername;
	this.port = port;
	this.ircclass = ircclass;
}

function HLine(allowedmask,servername) {
	this.allowedmask = allowedmask;
	this.servername = servername;
}

function ILine(ipmask,password,hostmask,port,ircclass) {
	this.ipmask = ipmask;
	this.password = password;
	this.hostmask = hostmask;
	this.port = port;
	this.ircclass = ircclass;
}

function KLine(hostmask,reason,type) {
	this.hostmask = hostmask;
	this.reason = reason;
	this.type = type;
}

function NLine(host,password,servername,flags,ircclass) {
	this.host = host;
	this.password = password;
	this.servername = servername;
	this.flags = flags;
	this.ircclass = ircclass;
}

function OLine(hostmask,password,nick,flags,ircclass) {
	this.hostmask = hostmask;
	this.password = password;
	this.nick = nick;
	this.flags = flags;
	this.ircclass = ircclass;
}

function QLine(nick,reason) {
	this.nick = nick;
	this.reason = reason;
}

function YLine(pingfreq,connfreq,maxlinks,sendq) {
	this.pingfreq = pingfreq;
	this.connfreq = connfreq;
	this.maxlinks = maxlinks;
	this.sendq = sendq;
	this.active = 0;
}

function ZLine(ipmask,reason) {
	this.ipmask = ipmask;
	this.reason = reason;
}

function RBL_Config_Object(hostname, good, bad) {
	this.hostname = hostname;
	this.good = good ? good.split(",") : "";
	this.bad = bad ? bad.split(",") : "";
}
