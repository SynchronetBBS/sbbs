// $Id$
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
// Copyright 2003 Randolph Erwin Sommerfeld <sysop@rrx.ca>
//
// ** Everything related to channels and their operation.
//

////////// Constants / Defines //////////
const CHANNEL_REVISION = "$Revision$".split(' ')[1];

const CHANMODE_NONE		=(1<<0); // NONE
const CHANMODE_BAN		=(1<<1); // b
const CHANMODE_INVITE		=(1<<2); // i
const CHANMODE_KEY		=(1<<3); // k
const CHANMODE_LIMIT		=(1<<4); // l
const CHANMODE_MODERATED	=(1<<5); // m
const CHANMODE_NOOUTSIDE	=(1<<6); // n
const CHANMODE_OP		=(1<<7); // o
const CHANMODE_PRIVATE		=(1<<8); // p
const CHANMODE_SECRET		=(1<<9); // s
const CHANMODE_TOPIC		=(1<<10); // t
const CHANMODE_VOICE		=(1<<11); // v

// These are used in the mode crunching section to figure out what character
// to display in the crunched MODE line.
function Mode (modechar,args,state,list,isnick) {
	// The mode's character
	this.modechar = modechar;
	// Does this mode take a single argument only?
	this.args = args;
	// Is this mode a stateful mode? (i.e. changes channel behaviour)
	this.state = state;
	// Can this mode accept a list?
	this.list = list;
	// Is the list a nick (true), or a n!u@h IRC mask (false)?
	this.isnick = isnick;
}

MODE = new Array();
MODE[CHANMODE_BAN]		= new Mode("b",true,false,true,false);
MODE[CHANMODE_INVITE]		= new Mode("i",false,true,false,false);
MODE[CHANMODE_KEY]		= new Mode("k",true,true,false,false);
MODE[CHANMODE_LIMIT]		= new Mode("l",true,true,false,false);
MODE[CHANMODE_MODERATED]	= new Mode("m",false,true,false,false);
MODE[CHANMODE_NOOUTSIDE]	= new Mode("n",false,true,false,false);
MODE[CHANMODE_OP]		= new Mode("o",true,false,true,true);
MODE[CHANMODE_PRIVATE]		= new Mode("p",false,true,false,false);
MODE[CHANMODE_SECRET]		= new Mode("s",false,true,false,false);
MODE[CHANMODE_TOPIC]		= new Mode("t",false,true,false,false);
MODE[CHANMODE_VOICE]		= new Mode("v",true,false,true,true);

////////// Objects //////////
function Channel(nam) {
	this.nam=nam;
	this.mode=CHANMODE_NONE;
	this.topic="";
	this.topictime=0;
	this.topicchangedby="";
	this.arg = new Array;
	this.arg[CHANMODE_LIMIT] = 0;
	this.arg[CHANMODE_KEY] = "";
	this.users=new Array;
	this.modelist=new Array;
	this.modelist[CHANMODE_OP]=new Array;
	this.modelist[CHANMODE_VOICE]=new Array;
	this.modelist[CHANMODE_BAN]=new Array;
	this.bantime=new Array;
	this.bancreator=new Array;
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
	    !this.user.server && !this.user.uline) {
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
	    !this.user.server && !this.user.uline) {
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
				this.user.numeric401(this.tmplist[list_bit]
					[add][z]);
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
		if(IRC_match(banned_nuh,this.modelist[CHANMODE_BAN][this_ban]))
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

