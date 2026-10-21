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

 Copyright 2003-2023 Randy Sommerfeld <cyan@synchro.net>

*/

/* Channel Modes */
var CHANMODE_NONE     =(1<<0);
var CHANMODE_BAN      =(1<<1); /* +b */
var CHANMODE_INVITE   =(1<<2); /* +i */
var CHANMODE_KEY      =(1<<3); /* +k */
var CHANMODE_LIMIT    =(1<<4); /* +l */
var CHANMODE_MODERATED=(1<<5); /* +m */
var CHANMODE_NOOUTSIDE=(1<<6); /* +n */
var CHANMODE_OP       =(1<<7); /* +o */
var CHANMODE_PRIVATE  =(1<<8); /* +p */
var CHANMODE_SECRET   =(1<<9); /* +s */
var CHANMODE_TOPIC    =(1<<10); /* +t */
var CHANMODE_VOICE    =(1<<11); /* +v */

var MODE = {};
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

function Channel(name) {
	this.nam = name;
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
	this.created = Epoch();
	/* The raw identity value stays wire-valid even with a clock outside the
	   protocol epoch range. Object identity distinguishes repeated values. */
	this.leaf_timestamp = Math.max(1, Math.min(4294967295, this.created));
	this.leaf_timestamp_id = String(++Leaf_Channel_Sequence);
	/* Functions */
	this.chanmode = Channel_chanmode;
	this.isbanned = Channel_isbanned;
	this.count_modelist = Channel_count_modelist;
	this.occupants = Channel_Occupants;
	this.match_list_mask = Channel_match_list_mask;
	this.is_str_member_of_chanmode_list = Channel_is_str_member_of_chanmode_list;
}

function ChanMode_tweaktmpmode(tmp_bit,add) {
	if (   !this.chan.modelist[CHANMODE_OP][this.user.id]
		&& !this.user.server
		&& !this.user.uline
		&& !(this.user.mode&USERMODE_ADMIN)
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
		&& !(this.user.mode&USERMODE_ADMIN)
	) {
		this.user.numeric482(this.chan.nam);
		return 0;
	}
	for (i in this.list[bit][add]) {
		/* Is this argument in our list for this mode already? */
		if (this.list[bit][add][i].toUpperCase() == arg.toUpperCase())
			return 0;
	}
	/* It doesn't exist on our mode, push it in. */
	this.list[bit][add].push(arg);
	/* Check for it against the other mode, and maybe nuke it. */
	for (i in this.list[bit][add ? false : true]) {
		if (this.list[bit][add ? false : true][i].toUpperCase() == arg.toUpperCase()) {
			delete this.list[bit][add ? false : true][i];
			return 0;
		}
	}
}

function ChanMode_affect_mode_list(list_bit) {
	var tmp_nick, add, z;
	for (add in this.list[list_bit]) {
		for (z in this.list[list_bit][add]) {
			tmp_nick = Users[this.list[list_bit][add][z].toUpperCase()];
			if (!tmp_nick)
				tmp_nick = search_nickbuf(this.list[list_bit][add][z]);
			if (tmp_nick && this.chan.users[tmp_nick.id]
				&& (add=="true") && !this.chan.modelist[list_bit][tmp_nick.id]) {
				this.addmodes += MODE[list_bit].modechar;
				this.addmodeargs += " " + tmp_nick.nick;
				this.chan.modelist[list_bit][tmp_nick.id] = tmp_nick;
				if (list_bit == CHANMODE_OP)
					delete this.chan.pending_founder;
			} else if (tmp_nick
						&& (add=="false")
						&& this.chan.modelist[list_bit][tmp_nick.id]
			) {
				this.delmodes += MODE[list_bit].modechar;
				this.delmodeargs += " " + tmp_nick.nick;
				delete this.chan.modelist[list_bit][tmp_nick.id];
			} else if (!tmp_nick && this.local) {
				this.user.numeric401(this.list[list_bit][add][z]);
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
	this.list = {};
	this.list[CHANMODE_OP] = {};
	this.list[CHANMODE_OP][false] = [];
	this.list[CHANMODE_OP][true] = [];
	this.list[CHANMODE_VOICE] = {};
	this.list[CHANMODE_VOICE][false] = [];
	this.list[CHANMODE_VOICE][true] = [];
	this.list[CHANMODE_BAN] = {};
	this.list[CHANMODE_BAN][false] = [];
	this.list[CHANMODE_BAN][true] = [];
	this.state_arg = {};
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

/* Channel timestamps are protocol values, not samples of the peer's current
   clock. Preserve them through relays; clock health is checked separately. */
function IRC_parse_channel_timestamp(str) {
	if (!/^[0-9]+$/.test(str))
		return null;
	var ts = Number(str);
	return ts <= 4294967295 ? ts : null;
}

function IRC_bcast_join(user, chan, ts, founder) {
	var i, peer;
	for (i in Local_Servers) {
		peer = Local_Servers[i];
		if (!peer.admitted || peer.nick == user.parent)
			continue;
		if (user.local) IRC_tsmap_claim(peer, chan);
		var wire_ts = IRC_is_leaf_uplink(peer) ? chan.leaf_timestamp : ts;
		/* Older hubs reject a leaf's server-prefixed founder MODE. Their
		   existing SJOIN dialect carries membership and founder status together. */
		if (founder && peer.hub && !peer.uline && peer.their_capab
			&& peer.their_capab.indexOf("TSJOIN") >= 0
			&& peer.their_capab.indexOf("CSYNC") < 0) {
			peer.rawout(format(":%s SJOIN %lu %s + :@%s",
				ServerName, wire_ts, chan.nam, user.nick));
			continue;
		}
		peer.rawout(format(":%s JOIN %s%s", user.nick,
			!IRC_legacy_leaf(peer) && peer.their_capab && peer.their_capab.indexOf("TSJOIN") >= 0
				? wire_ts + " " : "", chan.nam));
		if (founder)
			peer.ircout("MODE " + chan.nam + " +o " + user.nick);
	}
}

function IRC_bcast_chanmode(origin, chan, modes, ts) {
	var i, peer;
	for (i in Local_Servers) {
		peer = Local_Servers[i];
		if (!peer.admitted || peer.nick == origin.parent)
			continue;
		IRC_send_chanmode(peer, origin, chan, modes, ts);
	}
	/* A hub acknowledges accepted leaf changes. This orders them after any
	   corrective snapshot already queued to that leaf. */
	peer = origin.parent && Servers[String(origin.parent).toLowerCase()];
	if (peer && peer.local && !peer.hub && !peer.uline)
		peer.ircout("MODE " + chan.nam + IRC_mode_timestamp(peer,
			IRC_legacy_leaf(peer) ? 0 : chan.created) + modes);
}

function IRC_send_chanmode(peer, origin, chan, modes, ts) {
	var stamp = ts !== undefined ? IRC_mode_timestamp(peer, IRC_legacy_leaf(peer) ? 0 : ts) : " ";
	var prefix = "MODE " + chan.nam + stamp;
	/* Old parsers incorrectly consume an argument for -l. Send its removal
	   separately, before the remaining changes (which may remove our ops). */
	if (!peer.uline && (!peer.their_capab || peer.their_capab.indexOf("CSYNC") < 0)) {
		var fields = modes.split(" "), add = true, filtered = "", remove_limit = false;
		for (var i = 0; i < fields[0].length; i++) {
			var m = fields[0][i];
			if (m == "+" || m == "-") add = m == "+";
			if (m == "l" && !add) remove_limit = true;
			else filtered += m;
		}
		if (remove_limit) {
			peer.originatorout(prefix + "-l", origin);
			fields[0] = filtered;
			if (!/[a-z]/.test(filtered)) return;
			modes = fields.join(" ");
		}
	}
	peer.originatorout(prefix + modes, origin);
}

function IRC_bcast_part(user, chan) {
	for (var i in Local_Servers) {
		var peer = Local_Servers[i];
		if (!peer.admitted || peer.quitting || peer.nick == user.parent) continue;
		var line = "PART " + chan.nam;
		if (user.local && IRC_is_leaf_uplink(peer)) {
			var token = user.id + "." + (++Leaf_Part_Sequence);
			user.pending_channel_parts[chan.nam.toUpperCase()] = {peer: peer, token: token};
			line += " :" + token;
		}
		peer.originatorout(line, user);
	}
}

/* The original timestamp-bearing event propagates the reset to servers.
   Local clients need explicit removals before the winning state is applied. */
function Channel_reset_timestamp(chan, ts) {
	var changes = [], i, bit, u, line, modes = "", args = "", count = 0;
	for (bit in MODE) {
		if (MODE[bit].state && (chan.mode & bit))
			changes.push({mode: MODE[bit].modechar,
				arg: bit == CHANMODE_KEY ? chan.arg[bit] : ""});
	}
	for (i in chan.modelist[CHANMODE_OP])
		changes.push({mode: "o", arg: chan.modelist[CHANMODE_OP][i].nick});
	for (i in chan.modelist[CHANMODE_VOICE])
		changes.push({mode: "v", arg: chan.modelist[CHANMODE_VOICE][i].nick});
	chan.created = ts;
	chan.mode = CHANMODE_NONE;
	chan.arg[CHANMODE_KEY] = "";
	chan.arg[CHANMODE_LIMIT] = 0;
	chan.modelist[CHANMODE_OP] = {};
	chan.modelist[CHANMODE_VOICE] = {};
	delete chan.pending_founder;
	delete chan.pending_burst;
	for (i = 0; i < changes.length; i++) {
		var nextarg = changes[i].arg ? " " + changes[i].arg : "";
		if (count && (count == MAX_MODES
			|| (":" + ServerName + " MODE " + chan.nam + " -"
				+ modes + changes[i].mode + args + nextarg).length > 510)) {
			line = "MODE " + chan.nam + " -" + modes + args;
			for (u in chan.users)
				if (chan.users[u].local) chan.users[u].ircout(line);
			modes = args = "";
			count = 0;
		}
		modes += changes[i].mode;
		args += nextarg;
		count++;
	}
	if (count) {
		line = "MODE " + chan.nam + " -" + modes + args;
		for (u in chan.users)
			if (chan.users[u].local) chan.users[u].ircout(line);
	}
}

function IRCClient_set_chanmode(chan,cm_args,ts,options) {
	var c, i, j;
	var add;
	var final_modestr = "";
	options = options || {};

	if (!chan || !cm_args) {
		throw "set_chanmode() called without chan or cm_args";
		return;
	}

	/* TS-based conflict resolution for server bursts.
	   ts == 0 means "no timestamp provided" (not a real epoch 0).
	   If the incoming side is newer (higher ts), reject its ops/modes.
	   If the incoming side is older (lower ts), wipe local ops/modes first.
	   U:lined services (ChanServ/NickServ, etc.) are authoritative for the
	   modes they set and must never be TS-filtered: otherwise their auto-ops
	   are silently rejected (ts > created) or collaterally wipe other
	   members' ops (ts < created), which desyncs channel op state. */
	if (ts && ts > 0 && !this.uline) {
		if (ts > chan.created) {
			/* Incoming side is newer - our side wins, reject their modes */
			log(LOG_DEBUG, format(
				"[TS] set_chanmode: rejecting modes for %s: remote ts %d > local ts %d",
				chan.nam, ts, chan.created
			));
			return 2; /* distinct sentinel: TS rejected (not "no net change") */
		} else if (ts < chan.created) {
			/* Incoming side is older - their side wins, wipe our ops/modes */
			log(LOG_DEBUG, format(
				"[TS] set_chanmode: wiping local ops/modes for %s: remote ts %d < local ts %d",
				chan.nam, ts, chan.created
			));
			Channel_reset_timestamp(chan, ts);
			/* Bans are kept - standard TS protocol keeps ban lists from both sides */
		}
		/* ts == chan.created: no conflict, fall through to apply normally */
	}

	c = new ChanMode(chan,this);

	var mode_counter = 0;
	var mode_args_counter = 1; // start counting at args, not the modestring

	add = true; /* if a mode is passed without + or -, assume + */
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
					c.addbits|=CHANMODE_BAN; /* list bans */
					break;
				}
				if (cm_args.length <= mode_args_counter)
					break;
				c.tweaktmpmodelist(CHANMODE_BAN,add,cm_args[mode_args_counter]);
				mode_args_counter++;
				break;
			case "i":
				c.tweaktmpmode(CHANMODE_INVITE,add);
				break;
			case "k":
				if(cm_args.length > mode_args_counter) {
					c.tweaktmpmode(CHANMODE_KEY,add);
					c.state_arg[CHANMODE_KEY]=cm_args[mode_args_counter];
					mode_args_counter++;
				}
				break;
			case "l":
				if (add && (cm_args.length > mode_args_counter)) {
					var regexp = "^[0-9]{1,5}$";
					mode_args_counter++;
					if(!cm_args[mode_args_counter-1].match(regexp))
						break;
					c.state_arg[CHANMODE_LIMIT] = cm_args[mode_args_counter-1];
					c.tweaktmpmode(CHANMODE_LIMIT,true);
				} else if (!add) {
					c.tweaktmpmode(CHANMODE_LIMIT,false);
				}
				break;
			case "m":
				c.tweaktmpmode(CHANMODE_MODERATED,add);
				break;
			case "n":
				c.tweaktmpmode(CHANMODE_NOOUTSIDE,add);
				break;
			case "o":
				if (cm_args.length <= mode_args_counter)
					break;
				c.tweaktmpmodelist(CHANMODE_OP,add,cm_args[mode_args_counter]);
				mode_args_counter++;
				break;
			case "p":
				if( (add && !(chan.mode&CHANMODE_SECRET) ||
				     (c.delbits&CHANMODE_SECRET) ) ||
				    (!add) )
					c.tweaktmpmode(CHANMODE_PRIVATE,add);
				break;
			case "s":
				if( (add && (!(chan.mode&CHANMODE_PRIVATE) || options.merge) ||
				     (c.delbits&CHANMODE_PRIVATE) ) ||
				    (!add) )
					c.tweaktmpmode(CHANMODE_SECRET,add);
				break;
			case "t":
				c.tweaktmpmode(CHANMODE_TOPIC,add);
				break;
			case "v":
				if (cm_args.length <= mode_args_counter)
					break;
				c.tweaktmpmodelist(CHANMODE_VOICE,add,cm_args[mode_args_counter]);
				mode_args_counter++;
				break;
			default:
				if ((!this.parent) && (!this.server))
					this.numeric(472, cm_args[0][i] + " :is unknown mode char to me.");
				mode_counter--;
				break;
		}
		if (this.local && !this.server && mode_counter == MAX_MODES)
			break;
	}
	if (options.merge) {
		/* Equal-TS snapshots merge by a commutative rule. Ordinary live
		   MODE commands continue to set the requested value. */
		if ((c.addbits & CHANMODE_KEY) && (chan.mode & CHANMODE_KEY)
			&& chan.arg[CHANMODE_KEY] > c.state_arg[CHANMODE_KEY])
			c.state_arg[CHANMODE_KEY] = chan.arg[CHANMODE_KEY];
		if ((c.addbits & CHANMODE_LIMIT) && (chan.mode & CHANMODE_LIMIT))
			c.state_arg[CHANMODE_LIMIT] = String(Math.max(
				Number(chan.arg[CHANMODE_LIMIT]), Number(c.state_arg[CHANMODE_LIMIT])));
		if ((chan.mode | c.addbits) & CHANMODE_SECRET) {
			c.addbits &= ~CHANMODE_PRIVATE;
			if (c.addbits & CHANMODE_SECRET)
				c.delbits |= CHANMODE_PRIVATE;
		}
	}

	/* Now we run through all the mode toggles and construct our lists for
	   later display.  We also play with the channel bit switches here. */
	for (i in MODE) {
		if (MODE[i].state) {
			if (   (i&CHANMODE_KEY)
				&& (c.addbits&CHANMODE_KEY)
				&& c.state_arg[i]
				&& chan.arg[i]
				&& !this.server
				&& !this.parent
			) {
				this.numeric(467, format("%s :Channel key already set.", chan.nam));
			} else if (   (c.addbits&i)
						&& (
							   !(chan.mode&i)
							|| (
								  (i==CHANMODE_LIMIT || i==CHANMODE_KEY)
								&&(chan.arg[i]!=c.state_arg[i])
							   )
						   )
			) {
				c.addmodes += MODE[i].modechar;
				chan.mode |= i;
				if (MODE[i].args && MODE[i].state) {
					c.addmodeargs += " " +
						c.state_arg[i];
					chan.arg[i] = c.state_arg[i];
				}
			} else if ((c.delbits&i) && (chan.mode&i)) {
				c.delmodes += MODE[i].modechar;
				chan.mode &= ~i;
				if (MODE[i].args && MODE[i].state) {
					if (i == CHANMODE_KEY)
						c.delmodeargs += " " + chan.arg[i];
					chan.arg[i] = i == CHANMODE_LIMIT ? 0 : "";
				}
			}
		}
	}

	/* This is a special case, if +b was passed to us without arguments,
	   we simply display a list of bans on the channel. */
	if (c.addbits&CHANMODE_BAN) {
		for (i in chan.modelist[CHANMODE_BAN]) {
			this.numeric(367, format(
				"%s %s %s %s",
				chan.nam,
				chan.modelist[CHANMODE_BAN][i],
				chan.bancreator[i],
				chan.bantime[i]
			));
		}
		this.numeric(368, chan.nam + " :End of Channel Ban List.");
	}

	/* Bans are a specialized case */
	for (i in c.list[CHANMODE_BAN][true]) { // +b
		var set_ban = create_ban_mask(
			c.list[CHANMODE_BAN][true][i]);
		if (   (chan.count_modelist(CHANMODE_BAN) >= MAX_BANS)
			&& !this.server
			&& !this.parent
		) {
			this.numeric(478, format("%s %s :Can't add ban, channel ban list is full.",
				chan.nam,
				set_ban
			));
		} else if (set_ban && !chan.is_str_member_of_chanmode_list(CHANMODE_BAN, set_ban)) {
			c.addmodes += "b";
			c.addmodeargs += " " + set_ban;
			var banid = chan.modelist[CHANMODE_BAN].push(set_ban) - 1;
			chan.bantime[banid] = Epoch();
			chan.bancreator[banid] = this.nuh;
		}
	}

	for (i in c.list[CHANMODE_BAN][false]) { // -b
		for (j in chan.modelist[CHANMODE_BAN]) {
			if (   c.list[CHANMODE_BAN][false][i].toUpperCase()
			    == chan.modelist[CHANMODE_BAN][j].toUpperCase()
			) {
				c.delmodes += "b";
				c.delmodeargs += " " +
					c.list[CHANMODE_BAN][false][i];
				delete chan.modelist[CHANMODE_BAN][j];
				delete chan.bantime[j];
				delete chan.bancreator[j];
			}
		}
	}

	/* Modes where we just deal with lists of nicks. */
	c.affect_mode_list(CHANMODE_OP);
	c.affect_mode_list(CHANMODE_VOICE);

	if (!c.addmodes && !c.delmodes)
		return 0;

	if (c.addmodes)
		final_modestr += "+" + c.addmodes;
	if (c.delmodes)
		final_modestr += "-" + c.delmodes;
	if (c.addmodeargs)
		final_modestr += c.addmodeargs;
	if (c.delmodeargs)
		final_modestr += c.delmodeargs;

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
			case "b": /* only modes with arguments */
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
			var str = format("MODE %s %s%s", chan.nam, mode_output, f_mode_args);
			if (!this.server)
				this.bcast_to_channel(chan, str, true);
			else
				this.bcast_to_channel(chan, str, false);
			if (chan.nam[0] != "&" && !options.local_only)
				IRC_bcast_chanmode(this, chan, mode_output + f_mode_args, ts);

			if (add)
				mode_output = "+";
			else
				mode_output = "-";
			f_mode_args = "";
			mode_counter = 0;
		}
	}

	if (mode_output.length > 1) {
		var str = format("MODE %s %s%s", chan.nam, mode_output, f_mode_args);
		if (!this.server)
			this.bcast_to_channel(chan, str, true);
		else
			this.bcast_to_channel(chan, str, false);
		if (chan.nam[0] != "&" && !options.local_only)
			IRC_bcast_chanmode(this, chan, mode_output + f_mode_args, ts);
	}

	return 1;
}

function Channel_is_str_member_of_chanmode_list(modebit,str) {
	var i;

	if (!modebit || !MODE[modebit] || !MODE[modebit].list)
		throw "Channel_is_str_member_of_chanmode_list() called with invalid channel modebit";

	str = str.toUpperCase();

	if (MODE[modebit].isnick) {
		for (i in this.modelist[modebit]) {
			if (this.modelist[modebit][i].nick.toUpperCase() == str)
				return true;
		}
	} else {
		for (i in this.modelist[modebit]) {
			if (this.modelist[modebit][i].toUpperCase() == str)
				return true;
		}
	}

	return false;
}

function IRCClient_do_join(chan_name,join_key,join_ts) {
	chan_name = chan_name.slice(0,MAX_CHANLEN)

	if (!join_key)
		join_key = "";
	if (!join_ts)
		join_ts = 0;

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
		/* TS-based conflict resolution: if the joining user's side has a
		   different channel creation time, the older side wins.
		   Older incoming ts -> wipe our local ops/modes (they lose).
		   Newer incoming ts -> nothing to do, our state stands.
		   U:lined services are authoritative and never trigger a wipe. */
		if (join_ts > 0 && join_ts < chan.created && !this.uline) {
			log(LOG_DEBUG, format(
				"[TS] do_join: remote ts %d older than local %d for %s - wiping local ops/modes",
				join_ts, chan.created, chan.nam
			));
			Channel_reset_timestamp(chan, join_ts);
		}
		if (chan.pending_burst && this.parent != chan.pending_burst.nick)
			delete chan.pending_burst;
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
			IRC_bcast_join(this, chan, join_ts || chan.created);
		}
	} else {
		// create a new channel
		Channels[uc_chan_name] = new Channel(chan_name);
		chan=Channels[uc_chan_name];
		/* Use the provided timestamp if given (from a server burst),
		   otherwise the Channel constructor already stamped Epoch(). */
		if (join_ts > 0)
			chan.created = join_ts;
		if (!this.local)
			chan.pending_founder = this.id;
		chan.users[this.id] = this;
		var str="JOIN :" + chan.nam;
		var create_op = "";
		if (chan_name[0] != "&") {
			IRC_bcast_join(this, chan, join_ts || chan.created, this.local);
		}
		if (this.local) {
			this.originatorout(str,this);
			create_op = "@";
			chan.modelist[CHANMODE_OP][this.id] = this;
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
			if (chan_name[0] != "&")
				IRC_bcast_part(this, chan);
			this.rmchan(chan);
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
