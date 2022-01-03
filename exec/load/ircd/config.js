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

 Copyright 2003-2022 Randy Sommerfeld <cyan@synchro.net>

*/

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

function Read_Config_File() {
	var i;

	/* All of these variables are global. */
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
	Die_Password = "";
	Restart_Password = "";
	/* End of global variables */

	var fname="";
	if (Config_Filename && Config_Filename.length) {
		if(Config_Filename.indexOf('/')>=0 || Config_Filename.indexOf('\\')>=0)
			fname=Config_Filename;
		else
			fname=system.ctrl_dir + Config_Filename;
	} else {
		fname=system.ctrl_dir + "ircd." + system.local_host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd." + system.host_name + ".conf";
		if(!file_exists(fname))
			fname=system.ctrl_dir + "ircd.conf";
	}
	log(LOG_INFO,"Trying to read configuration from: " + fname);
	var file_handle = new File(fname);
	if (file_handle.open("r")) {
		if (fname.substr(fname.length-3,3) == "ini")
			read_ini_config(file_handle);
		else
			read_conf_config(file_handle);
		file_handle.close();
	} else {
		log(LOG_NOTICE, "Couldn't open configuration file! Proceeding with defaults.");
	}

	Time_Config_Read = Epoch();
	Scan_For_Banned_Clients();

	YLines[0] = new YLine(120,600,1,5050000); /* Default IRC class */
}

function ini_sections() {
	this.IRCdInfo = ini_IRCdInfo;
	this.Port = ini_Port;
	this.ConnectClass = ini_ConnectClass;
	this.Allow = ini_Allow;
	this.Operator = ini_Operator;
	this.Services = ini_Services;
	this.Ban = ini_Ban;
	this.Restrict = ini_Restrict;
	this.Hub = ini_Hub;
}

function ini_IRCdInfo(arg, ini) {
	ServerName=ini.Hostname;
	ServerDesc=ini.Info;
	Admin1=ini.Admin1;
	Admin2=ini.Admin2;
	Admin3=ini.Admin3;
}

/* Former M:Line */
function ini_Port(arg, ini) {
	Default_Port = arg;
}

/* Former Y:Line */
function ini_ConnectClass(arg, ini) {
	YLines[arg] = new YLine(
		parseInt(ini.PingFrequency),
		parseInt(ini.ConnectFrequency),
		parseInt(ini.Maximum),
		parseInt(ini.SendQ)
	);
}

/* Former I:Line */
function ini_Allow(arg, ini) {
	ILines[arg] = new ILine(
		ini.Mask,
		null, /* password */
		ini.Mask, /* hostmask */
		null, /* port */
		0 /* irc class */
	);
}

/* Former O:Line */
function ini_Operator(arg, ini) {
}

/* Former U:Line */
function ini_Services(arg, ini) {
}

/* Former K:Line & Z:Line */
function ini_Ban(arg, ini) {
}

function ini_Restrict(arg, ini) {
}

/* Former H:Line */
function ini_Hub(arg, ini) {
	HLines.push(new HLine(
		"*", /* servermask permitted */
		arg /* servername */
	));
}

function read_ini_config(conf) {
	var ini = conf.iniGetAllObjects();
	var Sections = new ini_sections();
	var i, s;

	for (var i in ini) {
		s = i.name.split(":");
		if (typeof Sections[s[0]] === 'function')
			Sections[s[0]](s[1], i);
	}
}

function read_conf_config(conf) {
	var conf_line, arg, i;

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

