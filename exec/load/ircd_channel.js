// $Id: ircd_channel.js,v 1.34 2019/08/06 20:44:39 deuce Exp $
//
// ircd_channel.js                
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details:
// http://www.gnu.org/licenses/gpl.txt
//
// Synchronet IRC Daemon as per RFC 1459, link compatible with Bahamut 1.4
//
// Copyright 2003-2009 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//
// ** Everything related to channels and their operation.
//

////////// Constants / Defines //////////
const CHANNEL_REVISION = "$Revision: 1.34 $".split(' ')[1];

const CHANMODE_NONE		=(1<<0); // NONE
const CHANMODE_BAN		=(1<<1); // b
const CHANMODE_INVITE	=(1<<2); // i
const CHANMODE_KEY		=(1<<3); // k
const CHANMODE_LIMIT	=(1<<4); // l
const CHANMODE_MODERATED=(1<<5); // m
const CHANMODE_NOOUTSIDE=(1<<6); // n
const CHANMODE_OP		=(1<<7); // o
const CHANMODE_PRIVATE	=(1<<8); // p
const CHANMODE_SECRET	=(1<<9); // s
const CHANMODE_TOPIC	=(1<<10); // t
const CHANMODE_VOICE	=(1<<11); // v

/* These are used in the mode crunching section to figure out what character
   to display in the crunched MODE line. */
function Mode(modechar,args,state,list,isnick) {
	this.modechar = modechar;	/* The mode's character */
	this.args = args;			/* Does this mode take only a single arg? */
	this.state = state;			/* Stateful? (changes channel behaviour) */
	this.list = list;			/* Does this mode accept a list? */
	this.isnick = isnick;		/* Is nick (true) or a n!u@h mask (false) */
}

MODE = new Object;
MODE[CHANMODE_BAN]		= new Mode("b",true,false,true,false);
MODE[CHANMODE_INVITE]	= new Mode("i",false,true,false,false);
MODE[CHANMODE_KEY]		= new Mode("k",true,true,false,false);
MODE[CHANMODE_LIMIT]	= new Mode("l",true,true,false,false);
MODE[CHANMODE_MODERATED]= new Mode("m",false,true,false,false);
MODE[CHANMODE_NOOUTSIDE]= new Mode("n",false,true,false,false);
MODE[CHANMODE_OP]		= new Mode("o",true,false,true,true);
MODE[CHANMODE_PRIVATE]	= new Mode("p",false,true,false,false);
MODE[CHANMODE_SECRET]	= new Mode("s",false,true,false,false);
MODE[CHANMODE_TOPIC]	= new Mode("t",false,true,false,false);
MODE[CHANMODE_VOICE]	= new Mode("v",true,false,true,true);

////////// Objects //////////
function Channel(nam) {
	this.nam=nam;
	this.mode=CHANMODE_NONE;
	this.topic="";
	this.topictime=0;
	this.topicchangedby="";
	this.arg = new Object;
	this.arg[CHANMODE_LIMIT] = 0;
	this.arg[CHANMODE_KEY] = "";
	this.users=new Object;
	this.modelist=new Object;
	this.modelist[CHANMODE_OP]=new Object;
	this.modelist[CHANMODE_VOICE]=new Object;
	this.modelist[CHANMODE_BAN]=new Array; /* True Array */
	this.bantime=new Object;
	this.bancreator=new Object;
	this.created=time();
	this.chanmode=Channel_chanmode;
	this.isbanned=Channel_isbanned;
	this.count_modelist=Channel_count_modelist;
	this.occupants=Channel_occupants;
	this.match_list_mask=Channel_match_list_mask;
}

////////// Functions //////////

function ChanMode_tweaktmpmode(tmp_bit,add) {
	if (!this.chan.modelist[CHANMODE_OP][this.user.id] && 
	    !this.user.server && !this.user.uline && !(this.mode&USERMODE_ADMIN)) {
		this.user.numeric482(this.chan.nam);
		return 0;
	}
	if (add) {
		this.addbits|=tmp_bit;
		this.delbits&=~tmp_bit;
	} else {
		this.addbits&=~tmp_bit;
		this.delbits|=tmp_bit;
	}
}

function ChanMode_tweaktmpmodelist(tmp_bit,add,arg) {
	if (!this.chan.modelist[CHANMODE_OP][this.user.id] &&
	    !this.user.server && !this.user.uline && !(this.mode&USERMODE_ADMIN)) {
		this.user.numeric482(this.chan.nam);
		return 0;
	}
	for (lstitem in this.tmplist[tmp_bit][add]) {
		// Is this argument in our list for this mode already?
		if (this.tmplist[tmp_bit][add][lstitem].toUpperCase() ==
		    arg.toUpperCase())
			return 0;
	}
	// It doesn't exist on our mode, push it in.
	this.tmplist[tmp_bit][add].push(arg);
	// Check for it against the other mode, and maybe nuke it.
	var oadd;
	if (add)
		oadd = false;
	else
		oadd = true;
	for (x in this.tmplist[tmp_bit][oadd]) {
		if (this.tmplist[tmp_bit][oadd][x].toUpperCase() ==
		    arg.toUpperCase()) {
			delete this.tmplist[tmp_bit][oadd][x];
			return 0;
		}
	}
}

function ChanMode_affect_mode_list(list_bit) {
	var tmp_nick;
	for (add in this.tmplist[list_bit]) {
		for (z in this.tmplist[list_bit][add]) {
			tmp_nick = Users[this.tmplist[list_bit][add][z].toUpperCase()];
			if (!tmp_nick)
				tmp_nick = search_nickbuf(this.tmplist[list_bit][add][z]);
			if (tmp_nick && (add=="true") &&
			    !this.chan.modelist[list_bit][tmp_nick.id]) {
				this.addmodes += MODE[list_bit].modechar;
				this.addmodeargs += " " + tmp_nick.nick;
				this.chan.modelist[list_bit][tmp_nick.id] = tmp_nick;
			} else if (tmp_nick && (add=="false") &&
			    this.chan.modelist[list_bit][tmp_nick.id]) {
				this.delmodes += MODE[list_bit].modechar;
				this.delmodeargs += " " + tmp_nick.nick;
				delete this.chan.modelist[list_bit][tmp_nick.id];
			} else if (!tmp_nick && this.local) {
				this.user.numeric401(this.tmplist[list_bit][add][z]);
			}
		}
	}
}

function Channel_count_modelist(list_bit) {
	var tmp_counter=0;
	for (tmp_count in this.modelist[list_bit]) {
		if (this.modelist[list_bit][tmp_count])
			tmp_counter++;
	}
	return tmp_counter;
}

function Channel_chanmode(show_args) {
	var tmp_mode = "+";
	var tmp_extras = "";
	if (this.mode&CHANMODE_INVITE)
		tmp_mode += "i";
	if (this.mode&CHANMODE_KEY) {
		tmp_mode += "k";
		if(show_args)
			tmp_extras += " " + this.arg[CHANMODE_KEY];
	}
	if (this.mode&CHANMODE_LIMIT) {
		tmp_mode += "l";
		if(show_args)
			tmp_extras += " " + this.arg[CHANMODE_LIMIT];
	}
	if (this.mode&CHANMODE_MODERATED)
		tmp_mode += "m";
	if (this.mode&CHANMODE_NOOUTSIDE)
		tmp_mode += "n";
	if (this.mode&CHANMODE_PRIVATE)
		tmp_mode += "p";
	if (this.mode&CHANMODE_SECRET)
		tmp_mode += "s";
	if (this.mode&CHANMODE_TOPIC)
		tmp_mode += "t";
	if (!tmp_extras)
		tmp_extras = " ";
	return tmp_mode + tmp_extras;
}

function Channel_isbanned(banned_nuh) {
	for (this_ban in this.modelist[CHANMODE_BAN]) {
		if(wildmatch(banned_nuh,this.modelist[CHANMODE_BAN][this_ban]))
			return 1;
	}
	return 0;
}

function Channel_occupants() {
	var chan_occupants="";
	for(thisChannel_user in this.users) {
		var Channel_user=this.users[thisChannel_user];
		if (Channel_user.nick) {
			if (chan_occupants)
				chan_occupants += " ";
			if (this.modelist[CHANMODE_OP][Channel_user.id])
				chan_occupants += "@";
			if (this.modelist[CHANMODE_VOICE][Channel_user.id])
				chan_occupants += "+";
			chan_occupants += Channel_user.nick;
		}
	}
	return chan_occupants;
}

// Yay, version 3.0 of this.set_chanmode(), eradicates any global variables.
function ChanMode(chan,user) {
	this.tmplist = new Object;
	this.tmplist[CHANMODE_OP] = new Object;
	this.tmplist[CHANMODE_OP][false] = new Array; //deop
	this.tmplist[CHANMODE_OP][true] = new Array; //op
	this.tmplist[CHANMODE_VOICE] = new Object;
	this.tmplist[CHANMODE_VOICE][false] = new Array; //devoice
	this.tmplist[CHANMODE_VOICE][true] = new Array; //voice
	this.tmplist[CHANMODE_BAN] = new Object;
	this.tmplist[CHANMODE_BAN][false] = new Array; //unban
	this.tmplist[CHANMODE_BAN][true] = new Array; //ban
	this.state_arg = new Object;
	this.state_arg[CHANMODE_KEY] = "";
	this.state_arg[CHANMODE_LIMIT] = "";
	this.addbits = 0;
	this.delbits = 0;
	this.addmodes = "";
	this.addmodeargs = "";
	this.delmodes = "";
	this.delmodeargs = "";
	this.chan = chan;
	this.user = user;
	// Functions.
	this.tweaktmpmodelist = ChanMode_tweaktmpmodelist;
	this.tweaktmpmode = ChanMode_tweaktmpmode;
	this.affect_mode_list = ChanMode_affect_mode_list;
}

function IRCClient_set_chanmode(chan,modeline,bounce_modes) {
	if (!chan || !modeline)
		return;

	var cmode = new ChanMode(chan,this);

	var cm_args = modeline.split(' ');

	var add=true;

	var mode_counter=0;
	var mode_args_counter=1; // start counting at args, not the modestring

	for (modechar in cm_args[0]) {
		mode_counter++;
		switch (cm_args[0][modechar]) {
			case "+":
				if (!add)
					add=true;
				mode_counter--;
				break;
			case "-":
				if (add)
					add=false;
				mode_counter--;
				break;
			case "b":
				if(add && (cm_args.length<=mode_args_counter)) {
					cmode.addbits|=CHANMODE_BAN;//list bans
					break;
				}
				cmode.tweaktmpmodelist(CHANMODE_BAN,add,
					cm_args[mode_args_counter]);
				mode_args_counter++;
				break;
			case "i":
				cmode.tweaktmpmode(CHANMODE_INVITE,add);
				break;
			case "k":
				if(cm_args.length > mode_args_counter) {
					cmode.tweaktmpmode(CHANMODE_KEY,add);
					cmode.state_arg[CHANMODE_KEY]=cm_args[mode_args_counter];
					mode_args_counter++;
				}
				break;
			case "l":
				if (add && (cm_args.length > mode_args_counter)) {
					var regexp = "^[0-9]{1,5}$";
					mode_args_counter++;
					if(cm_args[mode_args_counter-1].match(regexp))
						cmode.state_arg[CHANMODE_LIMIT]=cm_args[mode_args_counter-1];
					else
						break;
					cmode.tweaktmpmode(CHANMODE_LIMIT,true);
				} else if (!add) {
					cmode.tweaktmpmode(CHANMODE_LIMIT,false);
					if (cm_args.length > mode_args_counter)
						mode_args_counter++;
				}
				break;
			case "m":
				cmode.tweaktmpmode(CHANMODE_MODERATED,add);
				break;
			case "n":
				cmode.tweaktmpmode(CHANMODE_NOOUTSIDE,add);
				break;
			case "o":
				if (cm_args.length <= mode_args_counter)
					break;
				cmode.tweaktmpmodelist(CHANMODE_OP,add,
					cm_args[mode_args_counter]);
				mode_args_counter++;
				break;
			case "p":
				if( (add && !(chan.mode&CHANMODE_SECRET) ||
				     (cmode.delbits&CHANMODE_SECRET) ) ||
				    (!add) )
					cmode.tweaktmpmode(CHANMODE_PRIVATE,add);
				break;
			case "s":
				if( (add && !(chan.mode&CHANMODE_PRIVATE) ||
				     (cmode.delbits&CHANMODE_PRIVATE) ) ||
				    (!add) )
					cmode.tweaktmpmode(CHANMODE_SECRET,add);
				break;
			case "t":
				cmode.tweaktmpmode(CHANMODE_TOPIC,add);
				break;
			case "v":
				if (cm_args.length <= mode_args_counter)
					break;
				cmode.tweaktmpmodelist(CHANMODE_VOICE,add,
					cm_args[mode_args_counter]);
				mode_args_counter++;
				break;
			default:
				if ((!this.parent) && (!this.server))
					this.numeric(472, cm_args[0][modechar] + " :is unknown mode char to me.");
				mode_counter--;
				break;
		}
		if (mode_counter == max_modes)
			break;
	}

	// If we're bouncing modes, traverse our side of what the modes look
	// like and remove any modes not mentioned by what was passed to the
	// function.  Or, clear any ops, voiced members, or bans on the 'bad'
	// side of the network sync.
	if (bounce_modes) {
		for (cm in MODE) {
			if (MODE[cm].state && (chan.mode&cm) && !(cmode.addbits&cm)) {
				cmode.delbits |= cm;
			} else if (MODE[cm].list && MODE[cm].isnick) {
				for (member in chan.modelist[cm]) {
					cmode.delmodes += MODE[cm].modechar;
					cmode.delmodeargs += " " + chan.modelist[cm][member].nick;
					delete chan.modelist[cm][member];
				}
			} else if (MODE[cm].list && !MODE[cm].isnick) {
				for (ban in chan.modelist[cm]) {
					cmode.delmodes += MODE[cm].modechar;
					cmode.delmodeargs += " " + chan.modelist[cm][ban];
					delete chan.modelist[cm][ban];
					delete chan.bantime[ban];
					delete chan.bancreator[ban];
				}
			}
		}
	}

	// Now we run through all the mode toggles and construct our lists for
	// later display.  We also play with the channel bit switches here.
	for (cm in MODE) {
		if (MODE[cm].state) {
			if ((cm&CHANMODE_KEY) && (cmode.addbits&CHANMODE_KEY)&& 
			    cmode.state_arg[cm] && chan.arg[cm] &&
			    !this.server && !this.parent && !bounce_modes) {
				this.numeric(467, chan.nam +
					" :Channel key already set.");
			} else if ((cmode.addbits&cm) && (!(chan.mode&cm) ||
			    ((cm==CHANMODE_LIMIT)&&(chan.arg[CHANMODE_LIMIT]!=
			     cmode.state_arg[CHANMODE_LIMIT])) ) ) {
				cmode.addmodes += MODE[cm].modechar;
				chan.mode |= cm;
				if (MODE[cm].args && MODE[cm].state) {
					cmode.addmodeargs += " " +
						cmode.state_arg[cm];
					chan.arg[cm] = cmode.state_arg[cm];
				}
			} else if ((cmode.delbits&cm) && (chan.mode&cm)) {
				cmode.delmodes += MODE[cm].modechar;
				chan.mode &= ~cm;
				if (MODE[cm].args && MODE[cm].state) {
					cmode.delmodeargs += " " +
						cmode.state_arg[cm];
					chan.arg[cm] = "";
				}
			}
		}
	}

	// This is a special case, if +b was passed to us without arguments,
	// we simply display a list of bans on the channel.
	if (cmode.addbits&CHANMODE_BAN) {
		for (the_ban in chan.modelist[CHANMODE_BAN]) {
			this.numeric(367, chan.nam + " " + chan.modelist[CHANMODE_BAN][the_ban] + " " + chan.bancreator[the_ban] + " " + chan.bantime[the_ban]);
		}
		this.numeric(368, chan.nam + " :End of Channel Ban List.");
	}

	// Bans are a specialized case, sigh.
	for (z in cmode.tmplist[CHANMODE_BAN][true]) { // +b
		var set_ban = create_ban_mask(
			cmode.tmplist[CHANMODE_BAN][add][z]);
		if ((chan.count_modelist(CHANMODE_BAN) >= max_bans) &&
		     !this.server && !this.parent) {
			this.numeric(478, chan.nam + " " + set_ban + " :" +
				"Cannot add ban, channel's ban list is full.");
		} else if (set_ban && !chan.isbanned(set_ban)) {
			cmode.addmodes += "b";
			cmode.addmodeargs += " " + set_ban;
			var banid = chan.modelist[CHANMODE_BAN].push(set_ban) - 1;
			chan.bantime[banid] = time();
			chan.bancreator[banid] = this.nuh;
		}
	}

	for (z in cmode.tmplist[CHANMODE_BAN][false]) { // -b
		for (ban in chan.modelist[CHANMODE_BAN]) {
			if (cmode.tmplist[CHANMODE_BAN][false][z].toUpperCase()
			    == chan.modelist[CHANMODE_BAN][ban].toUpperCase()) {
				cmode.delmodes += "b";
				cmode.delmodeargs += " " +
					cmode.tmplist[CHANMODE_BAN][false][z];
				delete chan.modelist[CHANMODE_BAN][ban];
				delete chan.bantime[ban];
				delete chan.bancreator[ban];
			}
		}
	}

	// Modes where we just deal with lists of nicks.
	cmode.affect_mode_list(CHANMODE_OP);
	cmode.affect_mode_list(CHANMODE_VOICE);

	if (!cmode.addmodes && !cmode.delmodes)
		return 0;

	var final_modestr = "";

	if (cmode.addmodes)
		final_modestr += "+" + cmode.addmodes;
	if (cmode.delmodes)
		final_modestr += "-" + cmode.delmodes;
	if (cmode.addmodeargs)
		final_modestr += cmode.addmodeargs;
	if (cmode.delmodeargs)
		final_modestr += cmode.delmodeargs;

	var final_args = final_modestr.split(' ');
	var arg_counter = 0;
	var mode_counter = 0;
	var mode_output = "";
	var f_mode_args = "";
	for (marg in final_args[0]) {
		switch (final_args[0][marg]) {
			case "+":
				mode_output += "+";
				add = true;
				break;
			case "-":
				mode_output += "-";
				add = false;
				break;
			case "l":
				if (!add) {
					mode_counter++;
					mode_output += final_args[0][marg];
					break;
				}
			case "b": // only modes with arguments
			case "k":
			case "l":
			case "o":
			case "v":
				arg_counter++;
				f_mode_args += " " + final_args[arg_counter];
			default:
				mode_counter++;
				mode_output += final_args[0][marg];
				break;
		}
		if (mode_counter >= max_modes) {
			var str = "MODE " + chan.nam + " " + mode_output + f_mode_args;
			if (!this.server)
				this.bcast_to_channel(chan, str, true);
			else
				this.bcast_to_channel(chan, str, false);
			if (chan.nam[0] != "&")
				this.bcast_to_servers(str);

			if (add)
				mode_output = "+";
			else
				mode_output = "-";
			f_mode_args = "";
		}
	}

	if (mode_output.length > 1) {
		str = "MODE " + chan.nam + " " + mode_output + f_mode_args;
		if (!this.server)
			this.bcast_to_channel(chan, str, true);
		else
			this.bcast_to_channel(chan, str, false);
		if (chan.nam[0] != "&")
			this.bcast_to_servers(str);
	}

	return 1;
}

function IRCClient_do_join(chan_name,join_key) {
	chan_name = chan_name.slice(0,max_chanlen)

	var uc_chan_name = chan_name.toUpperCase();

	if((chan_name[0] != "#") && (chan_name[0] != "&") && !this.parent) {
		this.numeric403(chan_name);
		return 0;
	}
	if(chan_name.search(/[\x00-\x20\x2c\xa0]/)!=-1) {
		if (this.local)
			this.numeric(479, chan_name
				+ " :Channel name contains illegal characters.");
		return 0;
	}
	if (this.channels[uc_chan_name])
		return 0;
	if ((true_array_len(this.channels) >= max_user_chans) && this.local) {
		this.numeric("405", chan_name + " :You have joined too many channels.");
		return 0;
	}
	if (Channels[uc_chan_name]) {
		var chan = Channels[uc_chan_name];
		if (this.local) {
			if ((chan.mode&CHANMODE_INVITE) && (uc_chan_name != this.invited)) {
				this.numeric("473", chan.nam
					+ " :Cannot join channel (+i: invite only)");
				return 0;
			}
			if ((chan.mode&CHANMODE_LIMIT) && (true_array_len(chan.users)
			     	>= chan.arg[CHANMODE_LIMIT]) ) {
				this.numeric("471", chan.nam
					+ " :Cannot join channel (+l: channel is full)");
				return 0;
			}
			if ((chan.mode&CHANMODE_KEY)
					&& (chan.arg[CHANMODE_KEY] != join_key)) {
				this.numeric("475", chan.nam
					+ " :Cannot join channel (+k: key required)");
				return 0;
			}
			if (chan.isbanned(this.nuh) && (uc_chan_name != this.invited) ) {
				this.numeric("474", chan.nam
					+ " :Cannot join channel (+b: you're banned!)");
				return 0;
			}
		}
		// add to existing channel
		chan.users[this.id] = this;
		var str="JOIN :" + chan.nam;
		if (!this.local) {
			this.bcast_to_channel(chan, str, false);
		} else {
			this.bcast_to_channel(chan, str, true);
			if (chan.topic) {
				this.numeric332(chan);
				this.numeric333(chan);
			} else {
				this.numeric331(chan);
			}
		}
		if (chan_name[0] != "&") {
			this.bcast_to_servers_raw(":" + this.nick + " SJOIN "
				+ chan.created + " " + chan.nam,BAHAMUT);
			this.bcast_to_servers_raw(":" + this.nick + " JOIN "
				+ chan.nam + " " + chan.created,DREAMFORGE);
		}
	} else {
		// create a new channel
		Channels[uc_chan_name] = new Channel(chan_name);
		chan=Channels[uc_chan_name];
		chan.users[this.id] = this;
		var str="JOIN :" + chan.nam;
		var create_op = "";
		if (this.local) {
			this.originatorout(str,this);
			create_op = "@";
			chan.modelist[CHANMODE_OP][this.id] = this;
		}
		if (chan_name[0] != "&") {
			this.bcast_to_servers_raw(":" + servername + " SJOIN "
				+ chan.created + " " + chan.nam + " "
				+ chan.chanmode() + " :" + create_op + this.nick,BAHAMUT);
			this.bcast_to_servers_raw(":" + this.nick + " JOIN "
				+ chan.nam,DREAMFORGE);
			if (create_op) {
				server_bcast_to_servers(":" + servername + " MODE "
					+ chan.nam + " +o " + this.nick + " "
					+ chan.created,DREAMFORGE);
			}
		}
	}
	if (this.invited.toUpperCase() == chan.nam.toUpperCase())
		this.invited = "";
	this.channels[uc_chan_name] = chan;
	if (!this.parent) {
		this.names(chan);
		this.numeric(366, chan.nam + " :End of /NAMES list.");
	}
	return 1; // success
}

function IRCClient_do_part(chan_name) {
	var chan;
	var str;

	if((chan_name[0] != "#") && (chan_name[0] != "&") && !this.parent) {
		this.numeric403(chan_name);
		return 0;
	}
	chan = Channels[chan_name.toUpperCase()];
	if (chan) {
		if (this.channels[chan.nam.toUpperCase()]) {
			str = "PART " + chan.nam;
			if (this.parent)
				this.bcast_to_channel(chan, str, false);
			else
				this.bcast_to_channel(chan, str, true);
			this.rmchan(chan);
			if (chan_name[0] != "&")
				this.bcast_to_servers(str);
		} else if (this.local) {
			this.numeric442(chan_name);
		}
	} else if (this.local) {
		this.numeric403(chan_name);
	}
}

function IRCClient_part_all() {
	var partingChannel;

	for(thisChannel in this.channels) {
		partingChannel=this.channels[thisChannel];
		this.do_part(partingChannel.nam);
	}
}

