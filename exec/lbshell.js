// lbshell.js

// Lightbar Command Shell for Synchronet Version 4.00a+

// $Id: lbshell.js,v 1.133 2020/05/04 06:12:43 rswindell Exp $

// @format.tab-size 4, @format.use-tabs true

//##############################################################################
//#
//# Tips:
//#
//#	Tabstops should be set to 4 to view/edit this file
//#	If your editor does not support control characters,
//#		use \1 for Ctrl-A codes
//#
//################################# Begins Here #################################

/* Adjustable settings */
const LBShell_Attr=0x07;		/* light-grey on black */
const MessageWindow_Attr=7;
const MessageTimeout=50;		/* 100ths of a second */


load("sbbsdefs.js");
load("nodedefs.js");
load("lightbar.js");
load("graphic.js");
bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
load("text.js");
load("str_cmds.js");
var ansiterm = bbs.mods.ansiterm_lib;
if(!ansiterm)
	ansiterm = bbs.mods.ansiterm_lib = load({}, "ansiterm_lib.js");

var str;
var size=file_size(system.text_dir+"lbshell_bg.bin");
size/=2;	// Divide by two for attr/char pairs
size/=80;	// Divide by 80 cols.  Size should now be height (assuming int)
var BackGround=new Graphic(80,size,LBShell_Attr,' ');
var bg_filename=system.text_dir+"lbshell_bg.bin";
var bg_names;
bg_names=directory(system.text_dir+"/backgrounds/*.bin");
if(bg_names.length>0) {
	bg_filename=bg_names[random(bg_names.length)];
}
var use_bg=BackGround.load(bg_filename);
var MessageWindow=new Graphic(80,console.screen_rows,MessageWindow_Attr,' ');
var bars80="\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4";
var spaces80="                                                                               ";
var msg_rows=0;
var msg_timeouts=new Array();
var menus_displayed=new Array();
var lastmessage_time=0;
var lastmessage_type=0;
var orig_passthru=console.ctrlkey_passthru;
var hangup_now=false;

function handle_a_ctrlkey(key)
{
	var i;
	var pause=false;
	switch(key) {
		case ctrl('O'):	/* CTRL-O - Pause */
			break;
		case ctrl('U'):	/* CTRL-U User List */
		case ctrl('T'):	/* CTRL-T Time Info */
		case ctrl('K'):	/* CTRL-K Control Key Menu */
			pause=true;
		case ctrl('P'):	/* Ctrl-P Messages */
			stop_mouse();
			clear_screen();
			console.handle_ctrlkey(key);
			if(pause)
				console.pause();
			draw_main(true);
			for(i=0; i<menus_displayed.length; i++)
				menus_displayed[i].draw();
			start_mouse();
			break;
	}
}

function get_message()
{
	var rows=0;

	/* Update node action */
	if(bbs.node_action != system.node_list[bbs.node_num-1].action)
		system.node_list[bbs.node_num-1].action = bbs.node_action;

	/* Check for messages */
	if(system.node_list[bbs.node_num-1].misc & NODE_MSGW)
		rows+=MessageWindow.putmsg(1,MessageWindow.height,system.get_telegram(user.number),MessageWindow_Attr,true);
	if(system.node_list[bbs.node_num-1].misc & NODE_NMSG)
		rows+=MessageWindow.putmsg(1,MessageWindow.height,system.get_node_message(bbs.node_num),MessageWindow_Attr,true);

	/* Fix up node status */
	if(system.node_list[bbs.node_num-1].status==NODE_WFC) {
		log("NODE STATUS FIXUP");
		system.node_list[bbs.node_num-1].status=NODE_INUSE;
	}

	/* Check if user data has changed */
	if((system.node_list[bbs.node_num-1].misc & NODE_UDAT) && bbs.compare_ars("REST NOT G")) {
		user.cached=false;
		system.node_list[bbs.node_num-1].misc &= ~NODE_UDAT;
	}

	/* Interrupted? */
	if(system.node_list[bbs.node_num-1].misc & NODE_INTR) {
		rows+=MessageWindow.putmsg(1,MessageWindow.height,bbs.text(NodeLocked),MessageWindow_Attr,true);
		log("Interrupted");
		hangup_now=true;
	}

	/* Sysop Chat? */
	if(system.node_list[bbs.node_num-1].misc & NODE_LCHAT) {
		menu_opt(function() {
			bbs.private_chat(true);
			bbs.nodesync();
		});
	}

	/* Time left warning? */
	if((bbs.time_left/60)<(5-console.timeleft_warning) && !bbs.compare_ars("SYSOP")) {
		console.timeleft_warning=5-(bbs.time_left/60);
		rows+=MessageWindow.putmsg(1,MessageWindow.height,format(bbs.text(OnlyXminutesLeft),bbs.time_left/60+1,(bbs.time_left/60)?"s":""),MessageWindow_Attr,true);
	}

	if(bbs.time_left==0) {
		/* Call get_time_left() to handle the hangup and such */
		stop_mouse();
		clear_screen();
		bbs.get_time_left();
		bbs.hangup();
	}

	/* New day? */
//	if(!(system.status & SS_NEWDAY))
//		bbs.nodesync();

	return(rows);
}

function message_callback()
{
	var rows;
	var display=false;
	var i;
	var old_rows=msg_rows;

	rows=get_message();
	if(rows>0) {
		display=true;
		/*
		 * ToDo: This currently assumes that the
		 * current menu is the only one protruding into the message window
		 * This would not be correct with (for example) 20 external areas
		 * and 20 externals in one area
		 */

		/* Create new timeout object. */
		if(rows > console.screen_rows-1)
			rows=console.screen_rows-1;
		var timeout=new Object;
		timeout.ticks=0;
		timeout.rows=rows;
		msg_timeouts.push(timeout);
		msg_rows+=rows;
		if(msg_rows > console.screen_rows-1) {
			/* Find and remove older messages that have scrolled off the top (or at least partially). */
			for(i=0; i<msg_timeouts.length; i++) {
				timeout=msg_timeouts.shift();
				msg_rows -= timeout.rows;
				if(msg_rows < console.screen_rows-1)
					break;
			}
		}
		console.beep();
	}
	/* Increment the ticks and expire as necessary */
	for(i=0; i<msg_timeouts.length; i++) {
		msg_timeouts[i].ticks++;
		if(msg_timeouts[i].ticks>=MessageTimeout) {
			msg_rows -= msg_timeouts[i].rows;
			i--;
			msg_timeouts.shift();
			display=true;
		}
	}
	if(display) {
		var display_rows=old_rows;
		if(msg_rows>old_rows)
			display_rows=msg_rows;
		// Is this lightbar inside of the message window?
		if(this.ypos+this.items.length-1 /* Last Used Row */ > console.screen_rows-display_rows /* Row above top row of message window */) {
			/* Draw to left of lightbar */
			if(this.xpos>1)
				cleararea(1,console.screen_rows-display_rows+1,this.xpos-1,display_rows,false);
			/* Draw below lightbar */
			i=this.ypos+this.items[0].length;
			if(i<=console.screen_rows)
				cleararea(this.xpos,i,this.items[0].length,console.screen_rows-i+1,true);
			/* Draw to right of lightbar */
			i=this.xpos+this.items[0].length;
			if(i<=console.screen_columns)
				cleararea(i,console.screen_rows-display_rows+1,console.screen_columns-i+1,display_rows,true);
		}
		else
			/* We can draw the whole thing. */
			cleararea(1,console.screen_rows-display_rows+1,console.screen_columns,display_rows);
	}
	if(hangup_now)
		bbs.hangup();
}

function format_opt(str, width, expand)
{
	var opt=str;
	if(expand) {
		var cleaned=opt;
		cleaned=cleaned.replace(/\|/g,'');
		opt+=spaces80.substr(0,width-cleaned.length-2);
		opt+=' >';
	}
	return(opt);
}

function ShellLB() {
	Lightbar.call(this);
}
ShellLB.prototype = new Lightbar;
// Extend the Lightbar prototype...
ShellLB.prototype.erase=function() {
	cleararea(this.xpos,this.ypos,this.items[0].text.length,this.items.length,true);
};
ShellLB.prototype.lpadding="\xb3";
ShellLB.prototype.rpadding="\xb3";
ShellLB.prototype.timeout=100;
ShellLB.prototype.callback = message_callback;
ShellLB.prototype.hotkeys = KEY_LEFT+KEY_RIGHT+"\b\x7f\x1b"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
ShellLB.prototype.mouse_miss_key = '\b';
ShellLB.prototype.mouse_enabled = true;

function Mainbar()
{
	this.items=new Array();
	this.direction=1;
	this.xpos=2;
	this.ypos=1;
	this.hotkeys=KEY_DOWN+";"+ctrl('O')+ctrl('U')+ctrl('T')+ctrl('K')+ctrl('P');
	this.mouse_miss_key = undefined;
	this.add("|File","F",undefined,undefined,undefined,bbs.compare_ars("REST T"));
	this.add("|Messages","M");
	this.add("|Email","E",undefined,undefined,undefined,bbs.compare_ars("REST SE"));
	this.add("|Chat","C",undefined,undefined,undefined,bbs.compare_ars("REST C"));
	this.add("|Settings","S");
	this.add("E|xternals","x",undefined,undefined,undefined,bbs.compare_ars("REST X"));
	this.add("|View","V");
	this.add("|Goodbye","G");
	this.add("Commands",";");
	this.lpadding="";
	this.rpadding="";
}
Mainbar.prototype=ShellLB.prototype;

function top_bar(width)
{
	return("\xda"+bars80.substr(0,width)+"\xbf");
}

function bottom_bar(width)
{
	return("\xc0"+bars80.substr(0,width)+"\xd9");
}

function format_opt(str, width, expand)
{
	var opt=str;
	if(expand) {
		var cleaned=opt;
		cleaned=cleaned.replace(/\|/g,'');
		opt+=spaces80.substr(0,width-cleaned.length-2);
		opt+=' >';
	}
	return(opt);
}

function Filemenu()
{
	this.items=new Array();
	// Width of longest line with no dynamic variables
	var width=0;
	var scantime=system.datestr(bbs.new_file_time);
	// Expand for scan time line.
	if(width < 27+scantime.length)
		width=27+scantime.length;
	if(width < 11+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
		width=11+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
	this.xpos=1;
	this.ypos=2;
	this.full_width = width+2;
	this.add(top_bar(width),undefined,undefined,"","");
	this.add(
		 format_opt("|Change Directory",width,false)
		,"C",width
	);
	this.add(
		 format_opt("|List Dir ("+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+")",width,false)
		,"L",width
	);
	this.add(
		 format_opt("Scan for |New Files since "+scantime,width,true)
		,"N",width
	);
	this.add(
		 format_opt("Search for |Filenames",width,true)
		,"F",width
	);
	this.add(
		 format_opt("Search for |Text in Descriptions",width,true)
		,"T",width
	);
	this.add(
		 format_opt("|Download file(s)",width,true)
		,"D",width,undefined,undefined
		,bbs.compare_ars("REST D")
			|| (!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_download)
	);
	this.add(
		 format_opt("|Upload file(s)",width,true)
		,"U",width,undefined,undefined
		,bbs.compare_ars("REST U")
			|| ((!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload)
			&& file_area.upload_dir==undefined)
	);
	this.add(
		 format_opt("|Remove/Edit Files",width,false)
		,"R",width
	);
	this.add(
		 format_opt("View/Edit |Batch Queue",width,false)
		,"B",width,undefined,undefined
		// Disabled if you can't upload or download.
		// Disabled if no upload dir and no batch queue
		,(bbs.compare_ars("REST U AND REST D"))
			|| (bbs.batch_upload_total <= 0
				&& bbs.batch_dnload_total <= 0
				&& file_area.upload_dir==undefined
			)
	);
	this.add(
		 format_opt("|View",width,true)
		,"V",width
	);
	this.add(
		 format_opt("|Settings",width,true)
		,"S",width
	);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Filemenu.prototype=ShellLB.prototype;

function Filedirmenu(x, y, changenewscan)
{
	this.items=new Array();
	var width=changenewscan?20:0;

	if(width<10+file_area.lib_list[bbs.curlib].name.length)
		width=10+file_area.lib_list[bbs.curlib].name.length;
	if(width<12+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
		width=12+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
	this.xpos=x;
	this.ypos=y;
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|All File Areas","A",width);
	this.add("|Library ("+file_area.lib_list[bbs.curlib].name+")","L",width);
	this.add("|Directory ("+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+")","D",width);
	if(changenewscan)
		this.add("Change |New Scan Date","N",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Filedirmenu.prototype=ShellLB.prototype;

function Fileinfo()
{
	this.items=new Array();
	this.xpos=22;
	this.ypos=4;
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("File |Transfer Policy","T",32);
	this.add("Information on |Directory","D",32);
	this.add("|Users With Access to Dir","U",32);
	this.add("|Your File Transfer Statistics","Y",32);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Fileinfo.prototype=ShellLB.prototype;

function Settingsmenu()
{
	var width=18;

	this.items=new Array();
	this.xpos=30;
	this.ypos=2;
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|User Configuration","U",width);
	this.add("Minute |Bank","B",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Settingsmenu.prototype=ShellLB.prototype;

function Emailmenu()
{
	var width=24;

	this.items=new Array();
	this.xpos=17;
	this.ypos=2;
	this.add(top_bar(width),undefined,undefined,"","");
	this.add(format_opt("|Send Mail",width,true),"S",width);
	this.add("|Read Inbox","R",width);
	this.add("Read Sent |Messages","M",width,undefined,undefined,bbs.compare_ars("REST K"));
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Emailmenu.prototype=ShellLB.prototype;

function Messagemenu()
{
	var width=31;

	if(width<8+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
		width=8+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length
	this.items=new Array();
	this.xpos=7;
	this.ypos=2;
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|Change Sub","C",width);
	this.add("|Read "+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name,"R",width);
	this.add(
		 format_opt("Scan For |New Messages",width,true)
		,"N",width
	);
	this.add(
		 format_opt("Scan For Messages To |You",width,true)
		,"Y",width
	);
	this.add(
		 format_opt("Search For |Text in Messages",width,true)
		,"T",width
	);
	this.add("|Post In "+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name,"P",width,undefined,undefined,bbs.compare_ars("REST P"));
	if(bbs.compare_ars("REST N") && (msg_area.grp_list[bbs.curgrp].sub_list[bbs.crusub] & (SUB_QNET|SUB_PNET|SUB_FIDO)))
		this.items[6].disabed=true;
	this.add("Read/Post |Auto-Message","A",width);
	this.add("|QWK Packet Transfer Menu","Q",width);
	this.add("|View Information on Sub","V",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Messagemenu.prototype=ShellLB.prototype;

function Chatmenu()
{
	var width=27;

	this.items=new Array();
	this.xpos=24;
	this.ypos=2;
	this.add(top_bar(width),undefined,undefined,"","");
	this.add("|Multinode Chat","M",width);
	this.add("|Private Node to Node Chat","P",width);
	this.add("|Chat With The SysOp","C",width);
	this.add("|Talk With The System Guru","T",width);
	this.add("|Finger A Remote User/System","F",width);
	this.add("I|RC Chat","R",width);
	this.add("InterBBS |Instant Messages","I",width);
	this.add(format_opt("|Settings",width,true),"S",width);
	this.add(bottom_bar(width),undefined,undefined,"","");
}
Chatmenu.prototype=ShellLB.prototype;

// Generate menus of available xtrn sections.
function Xtrnsecs()
{
	var hotkeys="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*():;<>";

	this.items=new Array();
	this.xpos=40;
	this.ypos=2;
	var xtrnsecwidth=0;
	var j;
	for(j=0; j<xtrn_area.sec_list.length && j<console.screen_rows-2; j++) {
		if(xtrn_area.sec_list[j].name.length > xtrnsecwidth)
			xtrnsecwidth=xtrn_area.sec_list[j].name.length;
	}
	xtrnsecwidth += 4;
	if(xtrnsecwidth>37)
		xtrnsecwidth=37;
	this.add("\xda"+bars80.substr(0,xtrnsecwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list.length; j++) {
		// see if at least one program is available
		var canaccess = false;
		for (var k in xtrn_area.sec_list[j].prog_list) {
			if (xtrn_area.sec_list[j].prog_list[k].can_access) {
				canaccess = true;
			}
		}
		if (canaccess) {
			this.add("< |" + hotkeys.substr(j, 1) + " " + xtrn_area.sec_list[j].name, j.toString(), xtrnsecwidth);
		}
	}
	this.add("\xc0"+bars80.substr(0,xtrnsecwidth)+"\xd9",undefined,undefined,"","");
}
Xtrnsecs.prototype=ShellLB.prototype;

function Xtrnsec(sec)
{
	var hotkeys="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*():;<>";
	this.items=new Array();
	var j=0;

	xtrnsecprogwidth=0;
	// Figure out the correct width
	for(j=0; j<xtrn_area.sec_list[sec].prog_list.length && j < (console.screen_rows-2); j++) {
		if(xtrn_area.sec_list[sec].prog_list[j].name.length > xtrnsecprogwidth)
			xtrnsecprogwidth=xtrn_area.sec_list[sec].prog_list[j].name.length;
	}
	xtrnsecprogwidth+=2;
	if(xtrnsecprogwidth>37)
		xtrnsecprogwidth=37;
	if(xtrn_area.sec_list[sec].prog_list.length+3+sec <= console.screen_rows)
		this.ypos=sec+2;
	else
		this.ypos=console.screen_rows-j-1;
	this.xpos=40-xtrnsecprogwidth-2;
	this.add("\xda"+bars80.substr(0,xtrnsecprogwidth)+"\xbf",undefined,undefined,"","");
	for(j=0; j<xtrn_area.sec_list[sec].prog_list.length && j<console.screen_rows-3; j++) {
		if (xtrn_area.sec_list[sec].prog_list[j].can_access)
			this.add("|"+hotkeys.substr(j,1)+" "+xtrn_area.sec_list[sec].prog_list[j].name,j.toString(),xtrnsecprogwidth);
	}
	this.add("\xc0"+bars80.substr(0,xtrnsecprogwidth)+"\xd9",undefined,undefined,"","");
}
Xtrnsec.prototype=ShellLB.prototype;

function Infomenu()
{
	this.items=new Array();
	this.xpos=51;
	this.ypos=2;
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("System |Information","I",25);
	this.add("Synchronet |Version Info","V",25);
	this.add("Info on |Sub-Board","S",25);
	this.add("|Your Statistics","Y",25);
	this.add("< |User Lists","U",25);
	this.add("|Text Files","T",25);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Infomenu.prototype=ShellLB.prototype;

function Userlists()
{
	this.items=new Array();
	this.xpos=37;
	this.ypos=6;
	this.add("\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xbf",undefined,undefined,"","");
	this.add("|Logons Today","L",12);
	this.add("|Sub-Board","S",12);
	this.add("|All","A",12);
	this.add("\xc0\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xd9",undefined,undefined,"","");
}
Userlists.prototype=ShellLB.prototype;

var mainbar=new Mainbar;

draw_main(true);
var next_key='';
start_mouse();
while(bbs.online) {
	var done=0;
	var key=next_key;
	var extra_select=false;
	next_key='';
	bbs.node_action=NODE_MAIN;
	if(key=='')
		key=mainbar.getval()
	else
		mainbar.draw();
	extra_select=false;
	if(key==KEY_DOWN) {
		key=mainbar.items[mainbar.current].retval;
		extra_select=true;
	}
	switch(key) {
		case ctrl('O'): /* CTRL-O - Pause */
		case ctrl('U'): /* CTRL-U User List */
		case ctrl('T'): /* CTRL-T Time Info */
		case ctrl('K'): /* CTRL-K Control Key Menu */
		case ctrl('P'): /* Ctrl-P Messages */
			handle_a_ctrlkey(key);
			break;
		case ';':
			stop_mouse();
			mainbar.current=8;
			mainbar.draw();
			console.gotoxy(1,2);
			console.attributes=9;
			console.write("Command (? For Help): ");
			console.attributes=7;
			if(!console.aborted) {
				var str=console.getstr("",40,K_EDIT);
				clear_screen();
				if(str=='?') {
					if(!bbs.compare_ars("SYSOP"))
						str='HELP';
				}
				if(str=='?') {
					bbs.menu("sysmain");
					console.pause();
					bbs.menu("sysxfer");
				}
				else {
					var oldshell=user.command_shell;
					str_cmds(str);
					/* Still using this shell? */
					if(user.command_shell != oldshell)
						exit(0);
				}
				console.pause();
				draw_main(true);
			}
			else
				cleararea(1,2,console.screen_columns,1,true);
			start_mouse();
			break;
		case 'F':
			show_filemenu();
			break;
		case 'S':
			show_settingsmenu();
			break;
		case 'E':
			show_emailmenu();
			break;
		case 'M':
			show_messagemenu();
			break;
		case 'C':
			show_chatmenu();
			break;
		case 'x':
			var curr_xtrnsec=0;
			var x_sec;
			var x_prog;
			done=false;
			var xtrnsec=new Xtrnsecs;
			menus_displayed.push(xtrnsec);
			while(!done && bbs.online) {
				x_sec=xtrnsec.getval();
				if(x_sec==KEY_LEFT)
					x_sec=(xtrnsec.current-1).toString();
				if(x_sec==KEY_RIGHT) {
					main_right();
					break;
				}
				if(x_sec=='\b' || x_sec=='\x7f' || x_sec=='\x1b') {
					if (xtrnsec.mouse_miss_str !== undefined) {
						console.ungetstr(xtrnsec.mouse_miss_str);
						delete xtrnsec.mouse_miss_str;
					}
					break;
				}
				if(x_sec==ctrl('O')
						|| x_sec==ctrl('U')
						|| x_sec==ctrl('T')
						|| x_sec==ctrl('K')
						|| x_sec==ctrl('P')) {
					handle_a_ctrlkey(x_sec);
					continue;
				}
				curr_xtrnsec=parseInt(x_sec);
				var this_xtrnsec=new Xtrnsec(curr_xtrnsec);
				menus_displayed.push(this_xtrnsec);
				while(bbs.online) {
					x_prog=this_xtrnsec.getval();
					if(x_prog==KEY_LEFT) {
						this_xtrnsec.erase();
						xtrnsec.erase();
						main_left();
						done=1;
						break;
					}
					if(x_prog==KEY_RIGHT || x_prog=='\b' || x_prog=='\x7f' || x_prog=='\x1b') {
						this_xtrnsec.erase();
						if (this_xtrnsec.mouse_miss_str !== undefined) {
							console.ungetstr(this_xtrnsec.mouse_miss_str);
							delete this_xtrnsec.mouse_miss_str;
						}
						break;
					}
					if(x_prog==ctrl('O')
							|| x_prog==ctrl('U')
							|| x_prog==ctrl('T')
							|| x_prog==ctrl('K')
							|| x_prog==ctrl('P')) {
						handle_a_ctrlkey(x_prog);
						continue;
					}
					stop_mouse();
					clear_screen();

					bbs.exec_xtrn(xtrn_area.sec_list[curr_xtrnsec].prog_list[parseInt(x_prog)].number);
					start_mouse();
					draw_main(true);
					xtrnsec.draw();
				}
				menus_displayed.pop();
			}
			menus_displayed.pop();
			xtrnsec.erase();
			break;
		case 'V':
			var infomenu=new Infomenu;
			menus_displayed.push(infomenu);
			infoloop: while(bbs.online) {
				key=infomenu.getval();
				switch(key) {
					case ctrl('O'): /* CTRL-O - Pause */
					case ctrl('U'): /* CTRL-U User List */
					case ctrl('T'): /* CTRL-T Time Info */
					case ctrl('K'): /* CTRL-K Control Key Menu */
					case ctrl('P'): /* Ctrl-P Messages */
						handle_a_ctrlkey(key);
						break;
					case 'I':
						menu_opt(function() {
							bbs.sys_info();
							console.pause();
						});
						break;
					case 'V':
						menu_opt(function() {
							bbs.ver();
							console.pause();
						});
						break;
					case 'S':
						menu_opt(function() {
							bbs.sub_info();
							console.pause();
						});
						break;
					case 'Y':
						menu_opt(function() {
							bbs.user_info();
							console.pause();
						});
						break;
					case KEY_LEFT:
						if(infomenu.items[infomenu.current].retval!='U') {
							main_left();
							done=1;
							break infoloop;
						}
						// Fall-through
					case 'U':
						var userlists=new Userlists;
						menus_displayed.push(userlists);
						userlistloop: while(bbs.online) {
							key=userlists.getval();
							switch(key) {
								case ctrl('O'): /* CTRL-O - Pause */
								case ctrl('U'): /* CTRL-U User List */
								case ctrl('T'): /* CTRL-T Time Info */
								case ctrl('K'): /* CTRL-K Control Key Menu */
								case ctrl('P'): /* Ctrl-P Messages */
									handle_a_ctrlkey(key);
									break;
								case KEY_LEFT:
									userlists.erase();
									main_left();
									break infoloop;
								case KEY_RIGHT:
								case '\b':
									if (userlists.mouse_miss_str !== undefined) {
										console.ungetstr(userlists.mouse_miss_str);
										delete userlists.mouse_miss_str;
									}
								case '\x7f':
								case '\x1b':
									userlists.erase();
									break userlistloop;
								case 'L':
									menu_opt(function() {
										bbs.list_logons();
										console.pause();
									});
									break;
								case 'S':
									menu_opt(function() {
										bbs.list_users(UL_SUB);
										console.pause();
									});
									break;
								case 'A':
									menu_opt(function() {
										bbs.list_users(UL_ALL);
										console.pause();
									});
									break;
							}
						}
						menus_displayed.pop();
						break;
					case 'T':
						menu_opt(function() {
							bbs.text_sec();
						});
						break infoloop;
					case KEY_RIGHT:
						main_right();
						done=1;
						break infoloop;
					case '\b':
						if (infomenu.mouse_miss_str !== undefined) {
							console.ungetstr(infomenu.mouse_miss_str);
							delete infomenu.mouse_miss_str;
						}
					case '\x7f':
					case '\x1b':
						break infoloop;
				}
			}
			menus_displayed.pop();
			infomenu.erase();
			break;
		case 'G':       // Goodbye
			if(!extra_select) {
				stop_mouse();
				console.clear(LIGHTGRAY);
				bbs.logoff(/* prompt? */false);
				draw_main(true);
			}
	}
}

function todo_getfiles(lib, dir)
{
	var path=format("%s%s.ixb", file_area.lib_list[lib].dir_list[dir].data_dir, file_area.lib_list[lib].dir_list[dir].code);
	return(file_size(path)/22);	/* F_IXBSIZE */
}

function mouse_enable(enable)
{
	if(console.term_supports(USER_ANSI)) {
		ansiterm.send('mouse', enable ? 'set' : 'clear', 'x10_compatible');
		ansiterm.send('mouse', enable ? 'set' : 'clear', 'extended_coord');
	}
}

function start_mouse()
{
	mouse_enable(true);
}

function stop_mouse()
{
	mouse_enable(false);
}

function clear_screen()
{
	/*
	 * Called whenever a command needs to exit the menu for user interaction.
	 *
	 * If you'd like a header before non-menu stuff, this is the place to put
	 * it.
	 */

	console.attributes=7;
	console.line_counter=0;
	console.clear();
	/* We are going to a line-mode thing... re-enable CTRL keys. */
	console.ctrlkey_passthru=orig_passthru;
}

function draw_main(topline)
{
	/*
	 * Called to re-display the main menu.
	 * topline is false when the top line doesn't need redrawing.
	 */
	/* Disable CTRL keys that we "know" how to handle. */
	console.ctrlkey_passthru="+KOPTU";
	console.aborted=false;
	if(topline) {
		console.line_counter=0;
		console.clear();
		cleararea(1,1,console.screen_columns,console.screen_rows,true);
	}
	else
		cleararea(1,2,console.screen_columns,console.screen_rows,true);
}

function menu_opt(func)
{
	stop_mouse();
	clear_screen();
	func();
	draw_main(true);
	for(i=0; i<menus_displayed.length; i++)
		menus_displayed[i].draw();
	start_mouse();
}

function main_right()
{
	do {
		mainbar.current++;
		if(mainbar.current==mainbar.items.length)
			mainbar.current=0;
	} while(mainbar.items[mainbar.current].disabled || mainbar.items[mainbar.current].retval==undefined)
	next_key=mainbar.items[mainbar.current].retval;
	if(next_key=='G' || next_key==';')
		next_key='';
}

function main_left()
{
	do {
		if(mainbar.current==0)
			mainbar.current=mainbar.items.length;
		mainbar.current--;
	} while(mainbar.items[mainbar.current].disabled || mainbar.items[mainbar.current].retval==undefined)
	next_key=mainbar.items[mainbar.current].retval;
	if(next_key=='G' || next_key==';')
		next_key='';
}

/*
 * Displays and handles the file menu
 */
function show_filemenu()
{
	var cur=1;
	var nd=false;
	bbs.node_action=NODE_XFER;
	while(bbs.online) {
		var filemenu=new Filemenu();
		var ret;
		var i;
		var j;
		filemenu.nodraw=nd;
		filemenu.current=cur;

		menus_displayed.push(filemenu);
		ret=filemenu.getval();
		if(ret==KEY_RIGHT) {
			if(filemenu.items[filemenu.current].text.substr(-2,2)==' >')
				ret=filemenu.items[filemenu.current].retval;
		}
		file: switch(ret) {
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				handle_a_ctrlkey(ret);
				break;
			case KEY_LEFT:
				filemenu.erase();
				main_left();
				menus_displayed.pop();
				return;
			case '\b':
				if (filemenu.mouse_miss_str !== undefined) {
					console.ungetstr(filemenu.mouse_miss_str);
					delete filemenu.mouse_miss_str;
				}
			case '\x7f':
			case '\x1b':
				filemenu.erase();
				menus_displayed.pop();
				return;
			case KEY_RIGHT:
				filemenu.erase();
				main_right();
				menus_displayed.pop();
				return;
			case 'C':
				menu_opt(function() {
					changedir: do {
						if(!file_area.lib_list.length)
							break changedir;
						while(bbs.online) {
							var orig_lib=bbs.curlib;
							i=0;
							j=0;
							if(file_area.lib_list.length>1) {
								if(file_exists(system.text_dir+"menu/libs.*"))
									bbs.menu("libs");
								else {
									console.putmsg(bbs.text(CfgLibLstHdr),P_SAVEATR);
									for(i=0; i<file_area.lib_list.length; i++) {
										if(i==bbs.curlib)
											console.putmsg('*',P_SAVEATR);
										else
											console.putmsg(' ',P_SAVEATR);
										if(i<9)
											console.putmsg(' ',P_SAVEATR);
										if(i<99)
											console.putmsg(' ',P_SAVEATR);
										// We use console.putmsg to expand ^A, @, etc
										console.putmsg(format(bbs.text(CfgLibLstFmt),i+1,file_area.lib_list[i].description),P_SAVEATR);
									}
								}
								console.mnemonics(format(bbs.text(JoinWhichLib),bbs.curlib+1));
								j=console.getnum(file_area.lib_list.length,false);
								if(j<0)
									break changedir;
								if(!j)
									j=bbs.curlib;
								else
									j--;
							}
							bbs.curlib=j;
							if(file_exists(system.text_dir+"menu/dirs"+(bbs.curlib+1)))
								bbs.menu("dirs"+(bbs.curlib+1));
							else {
								console.line_counter=0;
								 console.clear();
								 console.putmsg(format(bbs.text(DirLstHdr), file_area.lib_list[j].description),P_SAVEATR);
								 for(i=0; i<file_area.lib_list[j].dir_list.length; i++) {
									if(i==bbs.curdir)
										console.putmsg('*',P_SAVEATR);
									else
										console.putmsg(' ',P_SAVEATR);
									if(i<9)
										console.putmsg(' ',P_SAVEATR);
									if(i<99)
										console.putmsg(' ',P_SAVEATR);
									console.putmsg(format(bbs.text(DirLstFmt),i+1, file_area.lib_list[j].dir_list[i].description,"",todo_getfiles(j,i)),P_SAVEATR);
								}
							}
							console.mnemonics(format(bbs.text(JoinWhichDir),bbs.curdir+1));
							i=console.getnum(file_area.lib_list[j].dir_list.length);
							if(i==-1) {
								if(file_area.lib_list.length==1) {
									bbs.curlib=orig_lib;
									break changedir;
								}
								continue;
							}
							if(!i)
								i=bbs.curdir;
							else
								i--;
							bbs.curdir=i;
							break changedir;
						}
					} while(0);
				});
				break;
			case 'L':
				menu_opt(function() {
					bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
					console.pause();
				});
				break;
			case 'N':
				var typemenu=new Filedirmenu(filemenu.xpos+filemenu.full_width, filemenu.current+1, true);
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\r\nchNew File Scan (All)\r\n");
								bbs.scan_dirs(FL_ULTIME,true);
								console.pause();
							});
							break;
						case 'L':
							/* Scan this lib only */
							menu_opt(function() {
								console.putmsg("\r\nchNew File Scan (Lib)\r\n");
								for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++)
									if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number,FL_ULTIME)<0)
										break;
								console.pause();
							});
							break;
						case 'D':
							/* Scan this dir only */
							menu_opt(function() {
								console.putmsg("\r\nchNew File Scan (Dir)\r\n");
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,FL_ULTIME);
								console.pause();
							});
							break;
						case 'N':
							// ToDo: Don't clear screen here, just do one line
							menu_opt(function() {
								bbs.new_file_time=bbs.get_newscantime(bbs.new_file_time);
							});
							break;
						case KEY_RIGHT:
							typemenu.erase();
							filemenu.erase();
							main_right();
							menus_displayed.pop();
							menus_displayed.pop();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:	// Anything else will escape.
							typemenu.erase();
							filemenu.nodraw=true;
							menus_displayed.pop();
							break file;
					}
				}
				break;
			case 'F':
				var typemenu=new Filedirmenu(filemenu.xpos+filemenu.full_width, filemenu.current+1, false);
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\r\nchSearch for Filename(s) (All)\r\n");
								var spec=bbs.get_filespec();
								for(i=0; i<file_area.lib_list.length; i++) {
									for(j=0;j<file_area.lib_list[i].dir_list.length;j++)
										if(bbs.list_files(file_area.lib_list[i].dir_list[j].number,spec,0)<0)
											break;
								}
								console.pause();
							});
							break;
						case 'L':
							/* Scan this lib only */
							menu_opt(function() {
								console.putmsg("\r\nchSearch for Filename(s) (Lib)\r\n");
								var spec=bbs.get_filespec();
								for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++)
									if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[j].number,spec,0)<0)
										break;
								console.pause();
							});
							break;
						case 'D':
							/* Scan this dir only */
							menu_opt(function() {
								console.putmsg("\r\nchSearch for Filename(s) (Dir)\r\n");
								var spec=bbs.get_filespec();
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,0);
								console.pause();
							});
							break;
						case KEY_RIGHT:
							filemenu.erase();
							typemenu.erase();
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:	// Anything else will escape.
							typemenu.erase();
							filemenu.nodraw=true;
							menus_displayed.pop();
							break file;
					}
				}
				break;
			case 'T':
				var typemenu=new Filedirmenu(filemenu.xpos+filemenu.full_width, filemenu.current+1, false);
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\r\nchSearch for Text in Description(s) (All)\r\n");
								console.putmsg(bbs.text(SearchStringPrompt));
								var spec=console.getstr(40,K_LINE|K_UPPER);
								for(i=0; i<file_area.lib_list.length; i++) {
									for(j=0;j<file_area.lib_list[i].dir_list.length;j++)
										if(bbs.list_files(file_area.lib_list[i].dir_list[j].number,spec,FL_FINDDESC)<0)
											break;
								}
								console.pause();
							});
							break;
						case 'L':
							/* Scan this lib only */
							menu_opt(function() {
								console.putmsg("\r\nchSearch for Text in Description(s) (Lib)\r\n");
								console.putmsg(bbs.text(SearchStringPrompt));
								var spec=console.getstr(40,K_LINE|K_UPPER);
								for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++)
									if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[j].number,spec,FL_FINDDESC)<0)
										break;
								console.pause();
							});
							break;
						case 'D':
							/* Scan this dir only */
							menu_opt(function() {
								console.putmsg("\r\nchSearch for Text in Description(s) (Dir)\r\n");
								console.putmsg(bbs.text(SearchStringPrompt));
								var spec=console.getstr(40,K_LINE|K_UPPER);
								bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number,spec,FL_FINDDESC);
								console.pause();
							});
							break;
						case KEY_RIGHT:
							filemenu.erase();
							typemenu.erase();
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:	// Anything else will escape.
							typemenu.erase();
							filemenu.nodraw=true;
							menus_displayed.pop();
							break file;
					}
				}
				break;
			case 'D':
				var typemenu=new ShellLB;
				typemenu.xpos=filemenu.xpos+filemenu.full_width;
				typemenu.ypos=filemenu.current+1;
				typemenu.add(top_bar(17),undefined,undefined,"","");
				typemenu.add('|Batch','B',17,undefined,undefined,bbs.batch_dnload_total<=0);
				typemenu.add('By |Name/File spec','N',17);
				typemenu.add('From |User','U',17);
				typemenu.add(bottom_bar(17),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'B':
							menu_opt(function() {
								bbs.batch_download();
							});
							break;
						case 'N':
							menu_opt(function() {
								var spec=bbs.get_filespec();
								bbs.list_file_info(bbs.curdir,spec,FI_DOWNLOAD);
							});
							break;
						case 'U':
							menu_opt(function() {
								bbs.list_file_info(bbs.curdir,spec,FI_USERXFER);
							});
							break;
						case KEY_RIGHT:
							filemenu.erase();
							typemenu.erase();
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							typemenu.erase();
							filemenu.nodraw=true;
							menus_displayed.pop();
							break file;
					}
				}
				break;
			case 'U':
				var typemenu=new ShellLB;
				var width=19;
				typemenu.xpos=filemenu.xpos+filemenu.full_width;
				typemenu.ypos=filemenu.current+1;
				if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
					if(width<9+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length)
						width=9+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name.length;
				}
				typemenu.add(top_bar(width),undefined,undefined,"","");
				if(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload || file_area.upload_dir==undefined) {
					typemenu.add('To |Dir ('+file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].name+')','C',width,undefined,undefined,!file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].can_upload);
				}
				else {
					typemenu.add('To Upload |Dir','P',width);
				}
				typemenu.add('To |Sysop Only','S',width,undefined,undefined,file_area.sysop_dir==undefined);
				typemenu.add('To Specific |User(s)','U',width,undefined,undefined,file_area.user_dir==undefined);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'C':	// Current dir
							menu_opt(function() {
								bbs.upload_file(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number);
							});
							break;
						case 'P':	// Upload dir
							menu_opt(function() {
								bbs.upload_file(file_area.upload_dir);
							});
							break;
						case 'S':	// Sysop dir
							menu_opt(function() {
								bbs.upload_file(file_area.sysop_dir);
							});
							break;
						case 'U':	// To user
							menu_opt(function() {
								bbs.upload_file(file_area.user_dir);
							});
							break;
						case KEY_RIGHT:
							filemenu.erase();
							typemenu.erase();
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							typemenu.erase();
							filemenu.nodraw=true;
							menus_displayed.pop();
							break file;
					}
				}
				break;
			case 'R':
				menu_opt(function() {
					fileremove: do {
						console.putmsg("\r\nchRemove/Edit File(s)\r\n");
						str=bbs.get_filespec();
						if(str==null)
							break fileremove;
						if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_REMOVE)) {
							var s=0;
							console.putmsg(bbs.text(SearchingAllDirs));
							for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
								if(i!=bbs.curdir &&
										(s=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_REMOVE))!=0) {
									if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
										break fileremove;
									}
								}
							}
							console.putmsg(bbs.text(SearchingAllLibs));
							for(i=0; i<file_area.lib_list.length; i++) {
								if(i==bbs.curlib)
									continue;
								for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
									if((s=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_REMOVE))!=0) {
										if(s==-1 || str.indexOf('?')!=-1 || str.indexOf('*')!=-1) {
											break fileremove;
										}
									}
								}
							}
						}
					} while(0);
				});
				break;
			case 'B':
				console.attributes=LBShell_Attr;
				menu_opt(function() {
					bbs.batch_menu();
				});
				break;
			case 'V':
				var typemenu=new ShellLB;
				var width=32;
				typemenu.xpos=filemenu.xpos+filemenu.full_width;
				typemenu.ypos=filemenu.current+1;
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('File |Contents','C',width);
				typemenu.add('File |Information','I',width);
				typemenu.add('File Transfer |Policy','P',width);
				typemenu.add('|Directory Info','D',width);
				typemenu.add('|Users with Access to Dir','U',width);
				typemenu.add('Your File Transfer |Statistics','S',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'C':
							stop_mouse();
							clear_screen();
							console.putmsg("\r\nchView File(s)\r\n");
							str=bbs.get_filespec();
							if(str!=null) {
								if(!bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FL_VIEW)) {
									console.putmsg(bbs.text(SearchingAllDirs));
									for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
										if(i==bbs.curdir)
											continue;
										if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FL_VIEW))
											break;
									}
									if(i<file_area.lib_list[bbs.curlib].dir_list.length)
										break file;
									console.putmsg(bbs.text(SearchingAllLibs));
									libloop: for(i=0; i<file_area.lib_list.length; i++) {
										if(i==bbs.curlib)
											continue;
										for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
											if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FL_VIEW))
											break libloop;
										}
									}
								}
							}
							console.pause();
							draw_main(true);
							filemenu.draw();
							start_mouse();
							break;
						case 'I':
							stop_mouse();
							clear_screen();
							console.putmsg("\r\nchView File Information\r\n");
							str=bbs.get_filespec();
							if(str!=null) {
								if(!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_INFO)) {
									console.putmsg(bbs.text(SearchingAllDirs));
									for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
										if(i==bbs.curdir)
											continue;
										if(bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_INFO))
											break;
									}
									if(i<file_area.lib_list[bbs.curlib].dir_list.length)
										break file;
									console.putmsg(bbs.text(SearchingAllLibs));
									libloop: for(i=0; i<file_area.lib_list.length; i++) {
										if(i==bbs.curlib)
											continue;
										for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
											if(bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FI_INFO))
											break libloop;
										}
									}
								}
							}
							console.pause();
							draw_main(true);
							filemenu.draw();
							start_mouse();
							break;
						case 'P':
							menu_opt(function() {
								bbs.xfer_policy();
								console.pause();
							});
							break;
						case 'D':
							menu_opt(function() {
								bbs.dir_info();
								console.pause();
							});
							break;
						case 'U':
							menu_opt(function() {
								bbs.list_users(UL_DIR);
								console.pause();
							});
							break;
						case 'S':
							break;
						case KEY_RIGHT:
							filemenu.erase();
							typemenu.erase();
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							typemenu.erase();
							menus_displayed.pop();
							filemenu.nodraw=true;
							break file;
					}
				}
				break;
			case 'S':
				var cur=1;
				while(bbs.online) {
					var typemenu=new ShellLB;
					var width=28;
					if(user.settings&USER_EXTDESC)
						width++;
					typemenu.xpos=filemenu.xpos+filemenu.full_width;
					typemenu.ypos=filemenu.current+1;
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add('Set |Batch Flagging '+(user.settings&USER_BATCHFLAG?'Off':'On'),'B',width);
					typemenu.add('|Set Extended Descriptions '+(user.settings&USER_EXTDESC?'Off':'On'),'S',width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					typemenu.current=cur;
					menus_displayed.push(typemenu);
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'B':
							user.settings ^= USER_BATCHFLAG;
							break;
						case 'S':
							/* Need to clear for shorter menu */
							if(user.settings & USER_EXTDESC)
								cleararea(typemenu.xpos+typemenu.items[0].text.length-1,typemenu.ypos,1,typemenu.items.length,true);
							user.settings ^= USER_EXTDESC;
							break;
						case KEY_RIGHT:
							filemenu.erase();
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							filemenu.nodraw=true;
							menus_displayed.pop();
							break file;
					}
					cur=typemenu.current;
					menus_displayed.pop();
				}
				break;
		}
		cur=filemenu.current;
		nd=filemenu.nodraw;
		menus_displayed.pop();
	}
}

function show_messagemenu()
{
	var cur=1;
	var nd=false;

	while(!done) {
		var i;
		var j;
		var ret;
		var messagemenu=new Messagemenu();
		messagemenu.current=cur;
		messagemenu.nodraw=nd;

		menus_displayed.push(messagemenu);
		ret=messagemenu.getval();
		if(ret==KEY_RIGHT) {
			if(messagemenu.items[messagemenu.current].text.substr(-2,2)==' >')
				ret=messagemenu.items[messagemenu.current].retval;
		}
		message: switch(ret) {
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				handle_a_ctrlkey(ret);
				break;
			case KEY_LEFT:
				cleararea(messagemenu.xpos,messagemenu.ypos,messagemenu.items[0].text.length,messagemenu.items.length,true);
				menus_displayed.pop();
				main_left();
				return;
			case '\b':
				if (messagemenu.mouse_miss_str !== undefined) {
					console.ungetstr(messagemenu.mouse_miss_str);
					delete messagemenu.mouse_miss_str;
				}
			case '\x7f':
			case '\x1b':
				cleararea(messagemenu.xpos,messagemenu.ypos,messagemenu.items[0].text.length,messagemenu.items.length,true);
				menus_displayed.pop();
				return;
			case KEY_RIGHT:
				cleararea(messagemenu.xpos,messagemenu.ypos,messagemenu.items[0].text.length,messagemenu.items.length,true);
				main_right();
				menus_displayed.pop();
				return;
			case 'C':
				stop_mouse();
				clear_screen();
				if(!msg_area.grp_list.length)
					break;
				msgjump: while(bbs.online) {
					var orig_grp=bbs.curgrp;
					var i=0;
					var j=0;
					if(msg_area.grp_list.length>1) {
						if(file_exists(system.text_dir+"menu/grps.*"))
							bbs.menu("grps");
						else {
							console.putmsg(bbs.text(CfgGrpLstHdr),P_SAVEATR);
							for(i=0; i<msg_area.grp_list.length; i++) {
								if(i==bbs.curgrp)
									console.putmsg('*',P_SAVEATR);
								else
									console.putmsg(' ',P_SAVEATR);
								if(i<9)
									console.putmsg(' ',P_SAVEATR);
								if(i<99)
									console.putmsg(' ',P_SAVEATR);
								// We use console.putmsg to expand ^A, @, etc
								console.putmsg(format(bbs.text(CfgGrpLstFmt),i+1,msg_area.grp_list[i].description),P_SAVEATR);
							}
						}
						console.mnemonics(format(bbs.text(JoinWhichGrp),bbs.curgrp+1));
						j=console.getnum(msg_area.grp_list.length);
						if(j<0)
							break msgjump;
						if(!j)
							j=bbs.curgrp;
						else
							j--;
					}
					bbs.curgrp=j;
					if(file_exists(system.text_dir+"menu/subs"+(bbs.curgrp+1)))
						bbs.menu("subs"+(bbs.curgrp+1));
					else {
						console.line_counter=0;
						console.clear();
						console.putmsg(format(bbs.text(SubLstHdr), msg_area.grp_list[j].description),P_SAVEATR);
						for(i=0; i<msg_area.grp_list[j].sub_list.length; i++) {
							var msgbase=new MsgBase(msg_area.grp_list[j].sub_list[i].code);
							if(msgbase==undefined)
								continue;
							if(!msgbase.open())
								continue;
							if(i==bbs.cursub)
								console.putmsg('*',P_SAVEATR);
							else
								console.putmsg(' ',P_SAVEATR);
							if(i<9)
								console.putmsg(' ',P_SAVEATR);
							if(i<99)
								console.putmsg(' ',P_SAVEATR);
							console.putmsg(format(bbs.text(SubLstFmt),i+1, msg_area.grp_list[j].sub_list[i].description,"",msgbase.total_msgs),P_SAVEATR);
							msgbase.close();
						}
					}
					console.mnemonics(format(bbs.text(JoinWhichSub),bbs.cursub+1));
					i=console.getnum(msg_area.grp_list[j].sub_list.length);
					if(i==-1) {
						if(msg_area.grp_list.length==1) {
							bbs.curgrp=orig_grp;
							break msgjump;
						}
						continue;
					}
					if(!i)
						i=bbs.cursub;
					else
						i--;
					bbs.cursub=i;
					break;
				}
				draw_main(true);
				start_mouse();
				break;
			case 'R':
				menu_opt(function() {
					bbs.scan_posts();
				});
				break;
			case 'N':
				var typemenu=new ShellLB;
				var width=29;
				if(width<8+msg_area.grp_list[bbs.curgrp].name.length)
					width=8+msg_area.grp_list[bbs.curgrp].name.length;
				if(width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
					width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
				typemenu.xpos=messagemenu.xpos+messagemenu.items[0].text.length;
				typemenu.ypos=messagemenu.current+1;
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('|All Message Areas','A',width);
				typemenu.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
				typemenu.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
				typemenu.add('Change New Scan |Configuration','C',width);
				typemenu.add('Change New Scan |Pointers','P',width);
				typemenu.add('|Reset New Scan Pointers','R',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
								bbs.scan_subs(SCAN_NEW|SCAN_MSGSONLY,/* All? */ true);
							});
							break;
						case 'G':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
								for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
									if(msg_area.grp_list[bbs.curgrp].sub_list[i].scan_cfg&SCAN_CFG_NEW
									    && !bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_NEW))
										break;
							});
							break;
						case 'S':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hNew Message Scan\r\n");
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_NEW);
							});
							break;
						case 'C':
							menu_opt(function() {
								bbs.cfg_msg_scan(SCAN_CFG_NEW);
							});
							break;
						case 'P':
							menu_opt(function() {
								bbs.cfg_msg_ptrs(SCAN_CFG_NEW);
							});
							break;
						case 'R':
							menu_opt(function() {
								bbs.reinit_msg_ptrs()
							});
							break;
						case KEY_RIGHT:
							cleararea(messagemenu.xpos,messagemenu.ypos,messagemenu.items[0].text.length,messagemenu.items.length,true);
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							messagemenu.nodraw=true;
							menus_displayed.pop();
							break message;
					}
				}
				break;
			case 'Y':
				var typemenu=new ShellLB;
				var width=30;
				if(width<8+msg_area.grp_list[bbs.curgrp].name.length)
					width=8+msg_area.grp_list[bbs.curgrp].name.length;
				if(width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
					width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
				typemenu.xpos=messagemenu.xpos+messagemenu.items[0].text.length;
				typemenu.ypos=messagemenu.current+1;
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('|All Message Areas','A',width);
				typemenu.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
				typemenu.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
				typemenu.add('Change Your Scan |Configuration','C',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
								bbs.scan_subs(SCAN_TOYOU, /* All? */ true);
							});
							break;
						case 'G':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
								for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++)
									if(msg_area.grp_list[bbs.curgrp].sub_list.scan_cfg&SCAN_CFG_TOYOU
									    && !bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_TOYOU))
										break;
							});
							break;
						case 'S':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hYour Message Scan\r\n");
								bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_TOYOU);
							});
							break;
						case 'C':
							menu_opt(function() {
								bbs.cfg_msg_scan(SCAN_CFG_TOYOU);
							});
							break;
						case KEY_RIGHT:
							cleararea(messagemenu.xpos,messagemenu.ypos,messagemenu.items[0].text.length,messagemenu.items.length,true);
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							messagemenu.nodraw=true;
							menus_displayed.pop();
							break message;
					}
				}
				break;
			case 'T':
				var typemenu=new ShellLB;
				var width=17;
				if(width<8+msg_area.grp_list[bbs.curgrp].name.length)
					width=8+msg_area.grp_list[bbs.curgrp].name.length;
				if(width<6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length)
					width=6+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name.length;
				typemenu.xpos=messagemenu.xpos+messagemenu.items[0].text.length;
				typemenu.ypos=messagemenu.current+1;
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('|All Message Areas','A',width);
				typemenu.add("|Group ("+msg_area.grp_list[bbs.curgrp].name+")",'G',width);
				typemenu.add('|Sub ('+msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].name+')','S',width);
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					var subonly;

					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
								bbs.scan_subs(SCAN_FIND, /* All? */true);
								console.crlf();
								if(console.line_counter)
									console.pause();
							});
							break;
						case 'G':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
								subonly=console.yesno(bbs.text(DisplaySubjectsOnlyQ));
								console.putmsg(bbs.text(SearchStringPrompt));
								str=console.getstr("",40,K_LINE|K_UPPER);
								if(str.length) {
									for(i=0; i<msg_area.grp_list[bbs.curgrp].sub_list.length; i++) {
										if(subonly)
											bbs.list_msgs(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_FIND, str);
										else {
											if(!bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[i].number, SCAN_FIND, str))
												break;
										}
									}
								}
								console.crlf();
								if(console.line_counter)
									console.pause();
							});
							break;
						case 'S':
							menu_opt(function() {
								console.putmsg("\r\n\x01c\x01hMessage Search\r\n");
								subonly=console.yesno(bbs.text(DisplaySubjectsOnlyQ));
								console.putmsg(bbs.text(SearchStringPrompt));
								str=console.getstr("",40,K_LINE|K_UPPER);
								if(str.length) {
									if(subonly)
										bbs.list_msgs(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_FIND, str);
									else
										bbs.scan_posts(msg_area.grp_list[bbs.curgrp].sub_list[bbs.cursub].number, SCAN_FIND, str);
								}
								console.crlf();
								if(console.line_counter)
									console.pause();
							});
							break;
						case KEY_RIGHT:
							cleararea(messagemenu.xpos,messagemenu.ypos,messagemenu.items[0].text.length,messagemenu.items.length,true);
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							messagemenu.nodraw=true;
							menus_displayed.pop();
							break message;
					}
				}
				break;
			case 'P':
				menu_opt(function() {
					bbs.post_msg();
				});
				break;
			case 'A':
				menu_opt(function() {
					bbs.auto_msg();
				});
				break;
			case 'Q':
				menu_opt(function() {
					bbs.qwk_sec();
				});
				break;
			case 'V':
				menu_opt(function() {
					bbs.sub_info();
					console.pause();
				});
				break;
		}
		cur=messagemenu.current;
		nd=messagemenu.nodraw;
		menus_displayed.pop();
	}
}

function show_emailmenu()
{
	var cur=1;
	/* There's nothing dynamic, so we can fiddle this here */
	var emailmenu=new Emailmenu();
	/* For consistency */
	emailmenu.current=cur;
	menus_displayed.push(emailmenu);

	while(bbs.online) {
		var i;
		var j;
		var ret;

		/* Nothing dynamic, so we don't need to save/restore nodraw */

		ret=emailmenu.getval();
		if(ret==KEY_RIGHT) {
			if(emailmenu.items[emailmenu.current].text.substr(-2,2)==' >')
				ret=emailmenu.items[emailmenu.current].retval;
		}
		email: switch(ret) {
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				handle_a_ctrlkey(ret);
				break;
			case KEY_LEFT:
				cleararea(emailmenu.xpos,emailmenu.ypos,emailmenu.items[0].text.length,emailmenu.items.length,true);
				main_left();
				menus_displayed.pop();
				return;
			case '\b':
				if (emailmenu.mouse_miss_str !== undefined) {
					console.ungetstr(emailmenu.mouse_miss_str);
					delete emailmenu.mouse_miss_str;
				}
			case '\x7f':
			case '\x1b':
				cleararea(emailmenu.xpos,emailmenu.ypos,emailmenu.items[0].text.length,emailmenu.items.length,true);
				menus_displayed.pop();
				return;
			case KEY_RIGHT:
				cleararea(emailmenu.xpos,emailmenu.ypos,emailmenu.items[0].text.length,emailmenu.items.length,true);
				menus_displayed.pop();
				main_right();
				return;
			case 'S':
				var typemenu=new ShellLB;
				var width=30;
				typemenu.xpos=emailmenu.xpos+emailmenu.items[0].text.length;
				typemenu.ypos=emailmenu.current+1;
				typemenu.add(top_bar(width),undefined,undefined,"","");
				typemenu.add('To |Sysop','S',width,undefined,undefined,bbs.compare_ars("REST S"));
				typemenu.add('To |Local User','L',width,undefined,undefined,bbs.compare_ars("REST E"));
				typemenu.add('To Local User with |Attachment','A',width,undefined,undefined,bbs.compare_ars("REST E"));
				typemenu.add('To |Remote User','R',width,undefined,undefined,bbs.compare_ars("REST E OR REST M"));
				typemenu.add('To Remote User with A|ttachment','T',width,undefined,undefined,bbs.compare_ars("REST E OR REST M"));
				typemenu.add(bottom_bar(width),undefined,undefined,"","");
				menus_displayed.push(typemenu);
				while(bbs.online) {
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'S':
							menu_opt(function() {
								bbs.email(1,WM_EMAIL,bbs.text(ReFeedback));
							});
							break;
						case 'L':
							menu_opt(function() {
								console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
								str=console.getstr("",40,K_UPRLWR);
								if(str!=null && str!="") {
									if(str=="Sysop")
										str="1";
									if(str.search(/\@/)!=-1)
										bbs.netmail(str);
									else {
										i=bbs.finduser(str);
										if(i>0)
											bbs.email(i,WM_EMAIL);
									}
								}
							});
							break;
						case 'A':
							menu_opt(function() {
								console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
								str=console.getstr("",40,K_UPRLWR);
								if(str!=null && str!="") {
									i=bbs.finduser(str);
									if(i>0)
										bbs.email(i,WM_EMAIL|WM_FILE);
								}
							});
							break;
						case 'R':
							menu_opt(function() {
								if(console.noyes("\r\nAttach a file"))
									i=0;
								else
									i=WM_FILE;
								console.putmsg(bbs.text(EnterNetMailAddress),P_SAVEATR);
								str=console.getstr("",60,K_LINE);
								if(str!=null && str !="")
									bbs.netmail(str,i);
							});
							break;
						case 'T':
							menu_opt(function() {
								console.putmsg("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
								str=console.getstr("",40,K_UPRLWR);
								if(str!=null && str!="")
									bbs.netmail(str,WM_FILE);
							});
							break;
						case KEY_RIGHT:
							cleararea(emailmenu.xpos,emailmenu.ypos,emailmenu.items[0].text.length,emailmenu.items.length,true);
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough XXXX
						default:
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							emailmenu.nodraw=true;
							menus_displayed.pop();
							break email;
					}
				}
				break;
			case 'R':
				menu_opt(function() {
					bbs.read_mail(MAIL_YOUR);
					console.pause();
				});
				break;
			case 'M':
				menu_opt(function() {
					bbs.read_mail(MAIL_SENT);
					console.pause();
				});
				break;
		}
		cur=emailmenu.current;
	}
}

function show_chatmenu()
{
	var cur=1;
	/* There's nothing dynamic, so we can fiddle this here */
	var chatmenu=new Chatmenu();
	/* For consistency */
	chatmenu.current=cur;
	menus_displayed.push(chatmenu);

	while(bbs.online) {
		var i;
		var j;
		var ret;

		/* Nothing dynamic, so we don't need to save/restore nodraw */

		ret=chatmenu.getval();
		if(ret==KEY_RIGHT) {
			if(chatmenu.items[chatmenu.current].text.substr(-2,2)==' >')
				ret=chatmenu.items[chatmenu.current].retval;
		}
		chat: switch(ret) {
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				handle_a_ctrlkey(ret);
				break;
			case KEY_LEFT:
				cleararea(chatmenu.xpos,chatmenu.ypos,chatmenu.items[0].text.length,chatmenu.items.length,true);
				main_left();
				menus_displayed.pop();
				return;
			case '\b':
				if (chatmenu.mouse_miss_str !== undefined) {
					console.ungetstr(chatmenu.mouse_miss_str);
					delete chatmenu.mouse_miss_str;
				}
			case '\x7f':
			case '\x1b':
				cleararea(chatmenu.xpos,chatmenu.ypos,chatmenu.items[0].text.length,chatmenu.items.length,true);
				menus_displayed.pop();
				return;
			case KEY_RIGHT:
				cleararea(chatmenu.xpos,chatmenu.ypos,chatmenu.items[0].text.length,chatmenu.items.length,true);
				main_right();
				menus_displayed.pop();
				return;
			case 'M':
				menu_opt(function() {
					bbs.multinode_chat();
				});
				break;
			case 'P':
				menu_opt(function() {
					bbs.private_chat();
				});
				break;
			case 'C':
				menu_opt(function() {
					if(!bbs.page_sysop())
						bbs.page_guru();
				});
				break;
			case 'T':
				menu_opt(function() {
					bbs.page_guru();
				});
				break;
			case 'F':
				menu_opt(function() {
					bbs.exec("?finger");
					console.pause();
				});
				break;
			case 'R':
				menu_opt(function() {
					var server = "irc.synchro.net 6667";
					if(user.security.level >= 90 || user.security.exemptions&UFLAG_C) {
						write("\001n\001y\001hIRC Server: ");
						server=console.getstr(server, 40, K_EDIT|K_LINE|K_AUTODEL);
						if(console.aborted)
							return;
					}
					write("\001n\001y\001hIRC Channel: ");
					var channel=console.getstr("#Synchronet", 40, K_EDIT|K_LINE|K_AUTODEL);
					if(!console.aborted)
						bbs.exec("?irc -a " + server + " " + channel);
				});
				break;
			case 'I':
				menu_opt(function() {
					bbs.exec("?sbbsimsg");
				});
				break;
			case 'S':
				while(bbs.online) {
					var typemenu=new ShellLB;
					var width=24;
					if(user.chat_settings&CHAT_SPLITP)
						width++;
					typemenu.xpos=chatmenu.xpos+chatmenu.items[0].text.length;
					typemenu.ypos=chatmenu.current+1;
					typemenu.add(top_bar(width),undefined,undefined,"","");
					typemenu.add("Set |Split Screen Chat "+(user.chat_settings&CHAT_SPLITP?"Off":"On"),'S',width);
					typemenu.add("Set A|vailability "+(user.chat_settings&CHAT_NOPAGE?"On":"Off"),'V',width);
					typemenu.add("Set Activity |Alerts "+(user.chat_settings&CHAT_NOACT?"On":"Off"),'A',width);
					typemenu.add(bottom_bar(width),undefined,undefined,"","");
					menus_displayed.push(typemenu);
					ret=typemenu.getval();
					switch(ret) {
						case ctrl('O'): /* CTRL-O - Pause */
						case ctrl('U'): /* CTRL-U User List */
						case ctrl('T'): /* CTRL-T Time Info */
						case ctrl('K'): /* CTRL-K Control Key Menu */
						case ctrl('P'): /* Ctrl-P Messages */
							handle_a_ctrlkey(ret);
							break;
						case 'S':
							if(user.chat_settings&CHAT_SPLITP)
								cleararea(typemenu.xpos+typemenu.items[0].text.length-1,typemenu.ypos,1,typemenu.items.length,true);
							user.chat_settings ^= CHAT_SPLITP;
							break;
						case 'V':
							user.chat_settings ^= CHAT_NOPAGE;
							break;
						case 'A':
							user.chat_settings ^= CHAT_NOACT;
							break;
						case KEY_RIGHT:
							cleararea(chatmenu.xpos,chatmenu.ypos,chatmenu.items[0].text.length,chatmenu.items.length,true);
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							menus_displayed.pop();
							menus_displayed.pop();
							main_right();
							return;
						case '\b':
							if (typemenu.mouse_miss_str !== undefined) {
								console.ungetstr(typemenu.mouse_miss_str);
								delete typemenu.mouse_miss_str;
							}
							// Fallthrough
						default:
							cleararea(typemenu.xpos,typemenu.ypos,typemenu.items[0].text.length,typemenu.items.length,true);
							chatmenu.nodraw=true;
							menus_displayed.pop();
							break chat;
					}
					menus_displayed.pop();
				}
				break;
		}
		cur=chatmenu.current;
	}
}

function show_settingsmenu()
{
	var settingsmenu=new Settingsmenu();
	var ret;

	menus_displayed.push(settingsmenu);
	while(bbs.online) {
		ret=settingsmenu.getval();
		switch(ret) {
			case ctrl('O'): /* CTRL-O - Pause */
			case ctrl('U'): /* CTRL-U User List */
			case ctrl('T'): /* CTRL-T Time Info */
			case ctrl('K'): /* CTRL-K Control Key Menu */
			case ctrl('P'): /* Ctrl-P Messages */
				handle_a_ctrlkey(ret);
				break;
			case 'U':
				menu_opt(function() {
					user.cached=false;
					var oldshell=user.command_shell;
					bbs.user_config();
					user.cached=false;
					/* Still using this shell? */
					if(user.command_shell != oldshell)
						exit(0);
				});
				break;
			case 'B':
				menu_opt(function() {
					bbs.time_bank();
				});
				break;
			case KEY_RIGHT:
				settingsmenu.erase();
				main_right();
				menus_displayed.pop();
				return;
			case KEY_LEFT:
				settingsmenu.erase();
				main_left();
				menus_displayed.pop();
				return;
			case '\b':
				if (settingsmenu.mouse_miss_str !== undefined) {
					console.ungetstr(settingsmenu.mouse_miss_str);
					delete settingsmenu.mouse_miss_str;
				}
			case '\x7f':
			case '\x1b':
				settingsmenu.erase();
				menus_displayed.pop();
				return;
		}
	}
}

function cleararea(xpos,ypos,width,height,eol_allowed)
{
	/*
	 * This function "clears" an area of the screen to the
	 * background.
	 * eol_allowed indicates that you can clear to eol without
	 * breaking anything.
	 */
	var x;
	var y;

	if(use_bg) {
		var bgx;
		var bgy;
		var bgw;
		var bgh;
		var bgxo;
		var bgyo;
		var between;
		var mtop;

		/* Redraw main menu line if asked */
		if(ypos==1) {
			console.gotoxy(1,1);
			console.attributes=0x17;
			console.cleartoeol();
			mainbar.draw();
			ypos++;
			height--;
		}

		/* Calculate the position of the top line of the message window */
		mtop=console.screen_rows-msg_rows+1;

		/* Calculate the correct values for the Bagkround draw */
		bgx=xpos;
		bgy=ypos;
		bgxo=xpos-1;	/* zero-based */
		bgyo=ypos-2;	/* zero-based and make allowance for top line (The top of BackGround is at line 2) */
		bgw=width;
		bgh=height;
		if(bgyo+bgh>BackGround.height)	/* Too high? */
			bgh=BackGround.height-bgyo;
		if(ypos+bgh > mtop)
			bgh=mtop-ypos;
		if(bgxo+bgw>BackGround.width)	/* Too wide? */
			bgw=BackGround.width-bgxo;
		if(bgw>0 && bgh>0) {	/* Anything to draw? */
			BackGround.draw(bgx,bgy,bgw,bgh,bgxo,bgyo);
			/* Decrement height, increment ypos to account for drawn area */
			ypos+=bgh;
			height-=bgh;
		}
		if(height<=0)	/* All done? */
			return;

		while(height>0 && ypos < mtop) {
			console.gotoxy(1,ypos);
			console.attributes=LBShell_Attr;
			console.cleartoeol();
			ypos++;
			height--;
		}
		if(height<=0)	/* All done? */
			return;

		/* Calculate the correct values for the MessageWindow draw */
		bgx=xpos;
		bgy=ypos;
		bgxo=xpos-1;	/* zero-based */
		bgyo=ypos-2;
		bgw=width;
		bgh=height;		/* The rest is all message baby */
		MessageWindow.draw(bgx,bgy,bgw,bgh,bgxo,bgyo);
		return;
	}
	for(y=ypos; y<ypos+height; y++) {
		if(eol_allowed) {
			if(y==1) {
				console.gotoxy(1,1);
				console.attributes=0x17;
				console.cleartoeol();
				mainbar.draw();
			}
			else if(y<console.screen_rows-8) {
				console.gotoxy(xpos,y);
				console.attributes=LBShell_Attr;
				console.cleartoeol();
			}
			else {
				console.gotoxy(1,y);
				switch(y) {
					case console.screen_rows-8:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n \x01n\x01h\xdc\xdc\xdc\xdc \xdb \xdc\xdc  \xdc\xdc\xde\xdb \xdc \xdc\xdc\xdc\xdc \xdc\xdc \xdc\xdc  \xdc\xdc\xdc\xdc\xdc\xdc\xdc \x01n\x01b\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc ");
						break;
					case console.screen_rows-7:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01h\x01c\x016\xdf\x01n\x01c\xdc\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n \x01n\x01c\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n \x01c\xdc\x01h\x016\xdf\x01n\x01c\xdc\x01h\x016\xdf\x01n\x01c\xdc\xdc\xdc\x01h\xdf \x016\xdf\x01n \x01h\x016\xdf\x01n \x01c\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n\x01c\xdc\xdc \xdc\x01bgj \xdb\x01w\x014@TIME-L@ @DATE@  \x01y\x01h@BOARDNAME-L19@ \x01n ");
						break;
					case console.screen_rows-6:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n  \x01b\x01h\x014\xdf\x01n\x01b\xdd\x01h\xdf\x014\xdf\x010\xdf \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010  \x014\xdf\x010 \x014\xdf\x01n\x01b\xdd\x01h\x014\xdf\x010 \x014\xdf\x01n\x01b\xdd\x01h\x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010   \x014\xdf\x010   \x014\x01n\x01b\xdb\x01h\x01w\x014Last On\x01k: \x01n\x014@LASTDATEON@  \x01h\x01cNode \x01k\x01n\x01c\x014@NODE-L3@ \x01wUp \x01c@UPTIME-L8@\x01n ");
						break;
					case console.screen_rows-5:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n\x01b \xdf\xdf  \xdf  \xdf \xdb\xdd\xdf\xdf\xdf\xdf \xdb\xdd  \xdb\xdb\xdf\xdf  \xdf \xdb\xdd\xdf\xdf\xdf \xdf   \xdb\x014\x01h\x01wFirstOn\x01k:\x01n\x014 @SINCE@  \x01h\x01cCalls\x01n\x01c\x014@SERVED-R4@ \x01wof\x01c @TCALLS-L7@\x01n ");
						break;
					case console.screen_rows-4:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n                                       \x01b\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf ");
						break;
					case console.screen_rows-3:
						console.attributes=7;
						console.cleartoeol();
						console.putmsg(" \x01n\x01c[\x01h@GN@\x01n\x01c] @GRP@ [\x01h@SN@\x01n\x01c] @SUB@\x01n\r\n");
						break;
					case console.screen_rows-2:
						console.attributes=7;
						console.cleartoeol();
						console.putmsg(" \x01n\x01c(\x01h@LN@\x01n\x01c) @LIB@ (\x01h@DN@\x01n\x01c) @DIR@\x01n\r\n");
						break;
					case console.screen_rows-1:
						console.attributes=LBShell_Attr;
						console.cleartoeol();
						break;
					case console.screen_rows:
						console.attributes=LBShell_Attr;
						console.cleartoeol();
						break;
				}
			}
		}
		else {
			/* Not allowed to clear to eol... */
			/*
			 * Right now, we're ***assuming*** that second-level menus are
			 * never touching the bottom stuff.
			 */
			if(y==1) {
				console.gotoxy(1,1);
				console.attributes=0x17;
				console.cleartoeol();
				mainbar.draw();
			}
			else if(y<console.screen_rows-8) {
				console.attributes=LBShell_Attr;
				console.gotoxy(xpos,y);
				for(x=xpos; x<xpos+width; x++)
					console.write(' ');
			}
			else {
				console.gotoxy(1,y);
				switch(y) {
					case console.screen_rows-8:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n \x01n\x01h\xdc\xdc\xdc\xdc \xdb \xdc\xdc  \xdc\xdc\xde\xdb \xdc \xdc\xdc\xdc\xdc \xdc\xdc \xdc\xdc  \xdc\xdc\xdc\xdc\xdc\xdc\xdc \x01n\x01b\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc ");
						break;
					case console.screen_rows-7:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01h\x01c\x016\xdf\x01n\x01c\xdc\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n \x01n\x01c\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n \x01c\xdc\x01h\x016\xdf\x01n\x01c\xdc\x01h\x016\xdf\x01n\x01c\xdc\xdc\xdc\x01h\xdf \x016\xdf\x01n \x01h\x016\xdf\x01n \x01c\xdc \x01h\x016\xdf\x01n \x01h\x016\xdf\x01n\x01c\xdc\xdc \xdc\x01bgj \xdb\x01w\x014@TIME-L@ @DATE@  \x01y\x01h@BOARDNAME-L19@ \x01n ");
						break;
					case console.screen_rows-6:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n  \x01b\x01h\x014\xdf\x01n\x01b\xdd\x01h\xdf\x014\xdf\x010\xdf \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010  \x014\xdf\x010 \x014\xdf\x01n\x01b\xdd\x01h\x014\xdf\x010 \x014\xdf\x01n\x01b\xdd\x01h\x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010 \x014\xdf\x010   \x014\xdf\x010   \x014\x01n\x01b\xdb\x01h\x01w\x014Last On\x01k: \x01n\x014@LASTDATEON@  \x01h\x01cNode \x01k\x01n\x01c\x014@NODE-L3@ \x01wUp \x01c@UPTIME-L8@\x01n ");
						break;
					case console.screen_rows-5:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n\x01b \xdf\xdf  \xdf  \xdf \xdb\xdd\xdf\xdf\xdf\xdf \xdb\xdd  \xdb\xdb\xdf\xdf  \xdf \xdb\xdd\xdf\xdf\xdf \xdf   \xdb\x014\x01h\x01wFirstOn\x01k:\x01n\x014 @SINCE@  \x01h\x01cCalls\x01n\x01c\x014@SERVED-R4@ \x01wof\x01c @TCALLS-L7@\x01n ");
						break;
					case console.screen_rows-4:
						console.attributes=LBShell_Attr;
						console.putmsg("\x01n                                       \x01b\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf ");
						break;
					case console.screen_rows-3:
						console.attributes=7;
						console.cleartoeol();
						console.putmsg(" \x01n\x01c[\x01h@GN@\x01n\x01c] @GRP@ [\x01h@SN@\x01n\x01c] @SUB@\x01n\r\n");
						break;
					case console.screen_rows-2:
						console.attributes=7;
						console.cleartoeol();
						console.putmsg(" \x01n\x01c(\x01h@LN@\x01n\x01c) @LIB@ (\x01h@DN@\x01n\x01c) @DIR@\x01n\r\n");
						break;
					case console.screen_rows-1:
						console.attributes=LBShell_Attr;
						console.cleartoeol();
						break;
					case console.screen_rows:
						console.attributes=LBShell_Attr;
						console.cleartoeol();
						break;
				}
			}
		}
	}
}

