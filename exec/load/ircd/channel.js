/*

 ircd/channel.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Everything related to channels in the IRCd and their operation.

 Copyright 2003-2021 Randy Sommerfeld <cyan@synchro.net>

*/

/* Channel Modes */
const CHANMODE_NONE     =(1<<0);
const CHANMODE_BAN      =(1<<1); /* +b */
const CHANMODE_INVITE   =(1<<2); /* +i */
const CHANMODE_KEY      =(1<<3); /* +k */
const CHANMODE_LIMIT    =(1<<4); /* +l */
const CHANMODE_MODERATED=(1<<5); /* +m */
const CHANMODE_NOOUTSIDE=(1<<6); /* +n */
const CHANMODE_OP       =(1<<7); /* +o */
const CHANMODE_PRIVATE  =(1<<8); /* +p */
const CHANMODE_SECRET   =(1<<9); /* +s */
const CHANMODE_TOPIC    =(1<<10); /* +t */
const CHANMODE_VOICE    =(1<<11); /* +v */

const MODE = {};
MODE[CHANMODE_BAN]      = new IRC_Channel_Mode("b",true,false,true,false);
MODE[CHANMODE_INVITE]   = new IRC_Channel_Mode("i",false,true,false,false);
MODE[CHANMODE_KEY]      = new IRC_Channel_Mode("k",true,true,false,false);
MODE[CHANMODE_LIMIT]    = new IRC_Channel_Mode("l",true,true,false,false);
MODE[CHANMODE_MODERATED]= new IRC_Channel_Mode("m",false,true,false,false);
MODE[CHANMODE_NOOUTSIDE]= new IRC_Channel_Mode("n",false,true,false,false);
MODE[CHANMODE_OP]       = new IRC_Channel_Mode("o",true,false,true,true);
MODE[CHANMODE_PRIVATE]  = new IRC_Channel_Mode("p",false,true,false,false);
MODE[CHANMODE_SECRET]   = new IRC_Channel_Mode("s",false,true,false,false);
MODE[CHANMODE_TOPIC]    = new IRC_Channel_Mode("t",false,true,false,false);
MODE[CHANMODE_VOICE]    = new IRC_Channel_Mode("v",true,false,true,true);

/* Object Prototypes */

function Channel(nam) {
	this.nam = nam;
	this.mode = CHANMODE_NONE;
	this.topic = "";
	this.topictime = 0;
	this.topicchangedby = "";
	this.arg = {};
	this.arg[CHANMODE_LIMIT] = 0;
	this.arg[CHANMODE_KEY] = "";
	this.users = {};
	this.modelist = {};
	this.modelist[CHANMODE_OP] = {};
	this.modelist[CHANMODE_VOICE] = {};
	this.modelist[CHANMODE_BAN] = [];
	this.bantime = {};
	this.bancreator = {};
	this.created = time();
	/* Functions */
	this.chanmode = Channel_chanmode;
	this.isbanned = Channel_isbanned;
	this.count_modelist = Channel_count_modelist;
	this.occupants = Channel_Occupants;
	this.match_list_mask = Channel_match_list_mask;
}

function ChanMode_tweaktmpmode(tmp_bit,add) {
	if (   !this.chan.modelist[CHANMODE_OP][this.user.id]
		&& !this.user.server
		&& !this.user.uline
		&& !(this.mode&USERMODE_ADMIN)
	) {
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

function ChanMode_tweaktmpmodelist(bit,add,arg) {
	var i;

	if (   !this.chan.modelist[CHANMODE_OP][this.user.id]
		&& !this.user.server
		&& !this.user.uline
		&& !(this.mode&USERMODE_ADMIN)
	) {
		this.user.numeric482(this.chan.nam);
		return 0;
	}
	for (i in this.tmplist[bit][add]) {
		/* Is this argument in our list for this mode already? */
		if (this.tmplist[bit][add][i].toUpperCase() == arg.toUpperCase())
			return 0;
	}
	/* It doesn't exist on our mode, push it in. */
	this.tmplist[bit][add].push(arg);
	/* Check for it against the other mode, and maybe nuke it. */
	for (i in this.tmplist[bit][add ? false : true]) {
		if (this.tmplist[bit][add ? false : true][i].toUpperCase() == arg.toUpperCase()) {
			delete this.tmplist[bit][add ? false : true][i];
			return 0;
		}
	}
}

function ChanMode_affect_mode_list(list_bit) {
	var tmp_nick, add, z;
	for (add in this.tmplist[list_bit]) {
		for (z in this.tmplist[list_bit][add]) {
			tmp_nick = Users[this.tmplist[list_bit][add][z].toUpperCase()];
			if (!tmp_nick)
				tmp_nick = search_nickbuf(this.tmplist[list_bit][add][z]);
			if (tmp_nick && (add=="true") && !this.chan.modelist[list_bit][tmp_nick.id]) {
				this.addmodes += MODE[list_bit].modechar;
				this.addmodeargs += " " + tmp_nick.nick;
				this.chan.modelist[list_bit][tmp_nick.id] = tmp_nick;
			} else if (tmp_nick
						&& (add=="false")
						&& this.chan.modelist[list_bit][tmp_nick.id]
			) {
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
	var i;
	var ret = 0;

	for (i in this.modelist[list_bit]) {
		if (this.modelist[list_bit][i])
			ret++;
	}

	return ret;
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

function Channel_isbanned(nuh) {
	var i;

	for (i in this.modelist[CHANMODE_BAN]) {
		if(wildmatch(nuh,this.modelist[CHANMODE_BAN][i]))
			return 1;
	}
	return 0;
}

function Channel_Occupants() {
	var i;
	var user;
	var ret = "";

	for (i in this.users) {
		user = this.users[i];
		if (user.nick) {
			if (ret)
				ret += " ";
			if (this.modelist[CHANMODE_OP][user.id])
				ret += "@";
			if (this.modelist[CHANMODE_VOICE][user.id])
				ret += "+";
			ret += user.nick;
		}
	}

	return ret;
}

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
	var i, j;

	if (!chan || !modeline)
		return;

	var cmode = new ChanMode(chan,this);

	var cm_args = modeline.split(' ');

	var add=true;

	var mode_counter=0;
	var mode_args_counter=1; // start counting at args, not the modestring

	for (i in cm_args[0]) {
		mode_counter++;
		switch (cm_args[0][i]) {
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
					this.numeric(472, cm_args[0][i] + " :is unknown mode char to me.");
				mode_counter--;
				break;
		}
		if (mode_counter == MAX_MODES)
			break;
	}

	// If we're bouncing modes, traverse our side of what the modes look
	// like and remove any modes not mentioned by what was passed to the
	// function.  Or, clear any ops, voiced members, or bans on the 'bad'
	// side of the network sync.
	if (bounce_modes) {
		for (i in MODE) {
			if (MODE[i].state && (chan.mode&i) && !(cmode.addbits&i)) {
				cmode.delbits |= i;
			} else if (MODE[i].list && MODE[i].isnick) {
				for (j in chan.modelist[i]) {
					cmode.delmodes += MODE[i].modechar;
					cmode.delmodeargs += " " + chan.modelist[i][j].nick;
					delete chan.modelist[i][j];
				}
			} else if (MODE[i].list && !MODE[i].isnick) {
				for (j in chan.modelist[i]) {
					cmode.delmodes += MODE[i].modechar;
					cmode.delmodeargs += " " + chan.modelist[i][j];
					delete chan.modelist[i][j];
					delete chan.bantime[j];
					delete chan.bancreator[j];
				}
			}
		}
	}

	// Now we run through all the mode toggles and construct our lists for
	// later display.  We also play with the channel bit switches here.
	for (i in MODE) {
		if (MODE[i].state) {
			if (   (i&CHANMODE_KEY)
				&& (cmode.addbits&CHANMODE_KEY)
				&& cmode.state_arg[i]
				&& chan.arg[i]
				&& !this.server
				&& !this.parent
				&& !bounce_modes
			) {
				this.numeric(467, format("%s :Channel key already set.", chan.nam));
			} else if (   (cmode.addbits&i)
						&& (
							   !(chan.mode&i)
							|| (
								  (i==CHANMODE_LIMIT)
								&&(chan.arg[CHANMODE_LIMIT]!=cmode.state_arg[CHANMODE_LIMIT])
							   )
						   )
			) {
				cmode.addmodes += MODE[i].modechar;
				chan.mode |= i;
				if (MODE[i].args && MODE[i].state) {
					cmode.addmodeargs += " " +
						cmode.state_arg[i];
					chan.arg[i] = cmode.state_arg[i];
				}
			} else if ((cmode.delbits&i) && (chan.mode&i)) {
				cmode.delmodes += MODE[i].modechar;
				chan.mode &= ~i;
				if (MODE[i].args && MODE[i].state) {
					cmode.delmodeargs += " " +
						cmode.state_arg[i];
					chan.arg[i] = "";
				}
			}
		}
	}

	// This is a special case, if +b was passed to us without arguments,
	// we simply display a list of bans on the channel.
	if (cmode.addbits&CHANMODE_BAN) {
		for (the_ban in chan.modelist[CHANMODE_BAN]) {
			this.numeric(367, format(
				"%s %s %s %s",
				chan.nam,
				chan.modelist[CHANMODE_BAN][the_ban],
				chan.bancreator[the_ban],
				chan.bantime[the_ban]
			));
		}
		this.numeric(368, chan.nam + " :End of Channel Ban List.");
	}

	// Bans are a specialized case, sigh.
	for (i in cmode.tmplist[CHANMODE_BAN][true]) { // +b
		var set_ban = create_ban_mask(
			cmode.tmplist[CHANMODE_BAN][add][i]);
		if (   (chan.count_modelist(CHANMODE_BAN) >= MAX_BANS)
			&& !this.server
			&& !this.parent
		) {
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

	for (i in cmode.tmplist[CHANMODE_BAN][false]) { // -b
		for (j in chan.modelist[CHANMODE_BAN]) {
			if (   cmode.tmplist[CHANMODE_BAN][false][i].toUpperCase()
			    == chan.modelist[CHANMODE_BAN][j].toUpperCase()
			) {
				cmode.delmodes += "b";
				cmode.delmodeargs += " " +
					cmode.tmplist[CHANMODE_BAN][false][i];
				delete chan.modelist[CHANMODE_BAN][j];
				delete chan.bantime[j];
				delete chan.bancreator[j];
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
	for (i in final_args[0]) {
		switch (final_args[0][i]) {
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
					mode_output += final_args[0][i];
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
				mode_output += final_args[0][i];
				break;
		}
		if (mode_counter >= MAX_MODES) {
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
	chan_name = chan_name.slice(0,MAX_CHANLEN)

	if (!join_key)
		join_key = "";

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
	if ((true_array_len(this.channels) >= MAX_USER_CHANS) && this.local) {
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
			if (   (chan.mode&CHANMODE_LIMIT)
				&& (true_array_len(chan.users) >= chan.arg[CHANMODE_LIMIT])
			) {
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
			this.bcast_to_servers_raw(
				format(":%s SJOIN %s %s",
					this.nick,
					chan.created,
					chan.nam
				)
			);
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
			this.bcast_to_servers_raw(
				format(":%s SJOIN %s %s %s :%s%s",
					ServerName,
					chan.created,
					chan.nam,
					chan.chanmode(),
					create_op,
					this.nick
				)
			);
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
	var i;

	for (i in this.channels) {
		this.do_part(this.channels[i].nam);
	}
}
