// Message Listing Module

/* To install manually into a Baja command shell (*.src) file,
   e.g. at the main or message menu, add a command key handler, like so:

   cmdkey L
		exec_bin "msglist"
		end_cmd

   ... and then recompile with baja (e.g. "baja default.src").
 */

/* To install as mail reading module and message list module, run:
   jsexec msglist -install
 */

/* TODO:
 * - display poll messages "correctly"
 * - support voting
 * - fix issues with operator sub-menu display
 * - complete operator sub-menu commands
 * - consider whether "source" view should be available to all
 * - message attributes displayed in header aren't always current
 * - regexp-style text searches
 * - highlight found text in messages (?)
 */

"use strict";

function install()
{
	var f = new File(system.ctrl_dir + "main.ini");
	if(!f.open(f.exists ? 'r+':'w+'))
		return "Failed to open " + f.name;
	f.iniSetValue("module", "readmail", "msglist mail -preview");
	f.iniSetValue("module", "listmsgs", "msglist");
	f.close();
	file_touch(system.ctrl_dir + "recycle");
	return true;
}

if(argv.indexOf('-install') >= 0)
{
	var result = install();
	if(result !== true)
		alert(result);
	exit(result === true ? 0 : 1);
}

if(argv.indexOf('-?') >= 0 || argv.indexOf('-help') >= 0)
{
	writeln("usage: [-options] [sub-code]");
	writeln("options:");
	writeln("  -preview        enable the message preview pane by default");
	writeln("  -sort=prop      sort message list by property");
	writeln("  -reverse        reverse the sort order");
	writeln("  -new=<days>     include new messages added in past <days>");
	writeln("  -p=<list>       specify comma-separated list of property names to print");
	writeln("  -fmt=<fmt>      specify format string");
	writeln("  -hdr            include list header");
	writeln("  -sent           list sent mail messages");
	writeln("  -all            list all mail messages");
	writeln("  -unread         list only unread messages");
	writeln("  -count          show count of messages only");
	writeln("   -hide_zero     do not print message count of 0 when no messages are listed")
	writeln("  -all_subs       list all sub-boards")
	writeln("  -nospam         do not load SPAM-tagged messages");
	writeln("  -spam           only load SPAM-tagged messages");
	exit(0);
}
require('sbbsdefs.js', 'LEN_ALIAS');
require('nodedefs.js', 'NODE_RMSG');
require("utf8_cp437.js", 'utf8_cp437');
require("file_size.js", 'file_size_str');
require("html2asc.js", 'html2asc');
require("graphic.js", 'Graphic');
var ansiterm = require("ansiterm_lib.js", 'expand_ctrl_a');
load('822header.js');
var hexdump = load('hexdump_lib.js');
var mimehdr = load('mimehdr.js');

var node_action = NODE_RMSG;

const age = load('age.js');

var color_cfg = {
	selection: BG_BLUE,
	column: [ WHITE ],
	sorted: BG_RED,
	preview_separator_active: LIGHTGRAY,
	preview_separator_inactive: LIGHTCYAN,
	preview_active: LIGHTCYAN,
	preview_inactive: LIGHTGRAY,
};

String.prototype.capitalize = function(){
   return this.replace( /(^|\s)([a-z])/g , function(m,p1,p2){ return p1+p2.toUpperCase(); } );
};

function columnHeading(name)
{
	var heading_option = 'heading_' + name;
	if(options[heading_option])
		return options[heading_option];
	switch(name) {
		case 'date_time':			return "Date             Time";
		case 'from_net_addr':		return "Net Address";
		case 'id':					return "Message-ID";
		case 'ftn_pid':				return "Program-ID";
		case 'ftn_msgid':			return "FidoNet Message-ID";
		case 'ftn_reply':			return "FidoNet Reply-ID";
		case 'ftn_tid':				return "FidoNet Tosser-ID";
		case 'priority':			return "Prio";
	}
	return name.replace(/_/g, ' ').capitalize();
}

/* Supported list formats */
var list_format = 0;
var list_formats = [];
var list_format_str;
var list_hdr = false;

const sub_list_formats = [
	[ "to", "subject" ],
];

const local_sub_list_formats = [
	[ "attr", "score",  "age", "date_time"],
];

const net_sub_list_formats = [
	[ "from_net_addr", "date_time", "zone"],
	[ "attr", "score", "subject" ],
	[ "id" ],
	[ "ftn_pid" ],
	[ "received", "age" ],
	[ "tags" ],
];

const fido_sub_list_formats = [
	[ "ftn_msgid" ],
	[ "ftn_reply" ],
	[ "ftn_tid" ],
];

const mail_sent_list_formats = [
	[ "attributes", "to_list" ]
];

const mail_list_formats = [
	[ "attributes", "subject" ],
	[ "priority", "date_time", "zone" ],
	[ "to_list" , "to_ext"],
	[ "cc_list", "to_net_addr" ],
	[ "from_net_addr" ],
	[ "received" ],
	[ "replyto_net_addr" ],
];

const extra_list_formats = [
	[ "size", "text_length", "lines" ],
];

function max_len(name)
{
	switch(name /*.toLowerCase().replace(/\s+/g, '') */) {
		case 'spam':
		case 'read':
			return 5;
		case 'age':
		case 'attr':
			return 8;
		case 'attributes':
			return 16;
		case 'date':
			return 10;
		case 'data_length':
		case 'text_length':
			return 11;
		case 'from':
		case 'to':
			return 25;
		case 'date_time':
			return 24;		// Date and time
		case 'zone':
			return 10;
		case 'received':
			return 34;		// Date, time and zone
		case 'score':
			return 5;
		case 'from_net_addr':
			return 15;		// 333:6404/1384.1253
		case 'priority':
			return 4;
		case 'size':
			return 8;
	}
	return 39;
}

var cmd_prompt_fmt = "\x01n\x01c\xfe \x01h%s \x01n\x01c\xfe ";
if(js.global.console==undefined || !console.term_supports(USER_ANSI))
	cmd_prompt_fmt = "%s: ";

function cp437(msg, prop)
{
	if(!msg[prop])
		return null;
	var xlatprop = prop + '_cp437';
	if(msg[xlatprop])
		return msg[xlatprop];
	if((msg.auxattr&MSG_HFIELDS_UTF8) /* && !console.term_supports(USER_UTF8) */)
		msg[xlatprop] = utf8_decode(msg[prop]);
//	log('decoding ' + prop + ' : ' + msg[prop]);
	if(msg[xlatprop] == undefined)
		msg[xlatprop] = mimehdr.to_cp437(msg[prop]);
	return msg[xlatprop];
}

function property_value(msg, prop, is_operator)
{
	var val;
	
	switch(prop) {
		case 'to_list':
			return cp437(msg, prop) || msg.to;
		case 'attr':
			return msg_attributes(msg, msgbase, /* short: */true);
		case 'spam':
			if(msg.attr & MSG_SPAM)
				val = "SPAM";
			break;
		case 'age':
			return age.string(msg.when_written_time - (msg.when_written_zone_offset * 60)
							, /* adjust_for_zone: */true);
		case 'date':
			return options.date_fmt ? strftime(options.date_fmt, msg.when_written_time) 
									: system.datestr(msg.when_written_time);
		case 'date_time':
			return options.date_time_fmt ? strftime(options.date_time_fmt, msg.when_written_time)
										: system.timestr(msg.when_written_time);
		case 'zone':
			return system.zonestr(msg.when_written_zone);
		case 'received':
			val = options.date_time_fmt ? strftime(options.date_time_fmt, msg.when_imported_time)
										: system.timestr(msg.when_imported_time);
			return format("%s %s", val, system.zonestr(msg.when_imported_zone));
		case 'id':
			if(msg.id) {
				val = msg.id.match(/[^<>]+/);
			}
			break;
		case 'size':
			return file_size_str(msg.data_length);
		case 'from':
			if((msg.attr&MSG_ANONYMOUS) && !is_operator)
				return bbs.text(Anonymous);
		case 'subject':
			return cp437(msg, prop);
		default:
			val = msg[prop];
			break;
	}
	if(val == undefined)
		return '';
	return val;
}

function property_sort_value(msg, prop)
{
	prop = prop; //.toLowerCase().replace(/\s+/g, '');
	switch(prop) {
		case 'attr':
			return msg.attr;
		case 'age':
			return age.seconds(msg.when_written_time - (msg.when_written_zone_offset * 60)
				, /* adjust_for_zone: */true);
			break;
		case 'date':
		case 'date_time':
			return msg.when_written_time;
		case 'zone':
			return msg.when_written_zone_offset;
		case 'received':
			return msg.when_imported_time;
		case 'to_ext':
			return parseInt(msg.to_ext, 10);
		case 'priority':
			return msg.priority ? msg.priority : SMB_PRIORITY_NORMAL;
		case 'size':
			return msg.data_length;
	}
	return property_value(msg, prop);
}

var sort_property;
var sort_reversed = false;
function sort_compare(a, b)
{
	var val1="";
	var val2="";
	
	if(sort_reversed) {
		var tmp = a;
		a = b;
		b = tmp;
	}

	val1 = property_sort_value(a, sort_property);
	val2 = property_sort_value(b, sort_property);

	if(typeof(val1) == "string")
		val1=val1.toLowerCase();
	if(typeof(val2) == "string")
		val2=val2.toLowerCase();
	else { /* Sort numbers backwards on purpose */
		var tmp = val1;
		val1 = val2;
		val2 = tmp;
	}
	if(val1>val2) return 1;
	if(val1<val2) return -1;
	return 0;
}

function console_color(arg, selected)
{
	if(selected)
		arg |= BG_BLUE|BG_HIGH;
	if(js.global.console != undefined)
		console.attributes = arg;
}

function console_beep()
{
	if(js.global.console != undefined && options.beep !== false)
		console.beep();
}

function help()
{
	console.clear();
	bbs.menu("msglist");
	console.pause();
}

function pause()
{
	if(typeof options.pause == "number")
		mswait(Number(options.pause) * 1000);
	else if(options.pause !== false)
		console.pause();
}

function list_msg(msg, digits, selected, sort, msg_ctrl, exclude, is_operator)
{
	var color = color_cfg.column[0];
	var color_mask = msg_ctrl ? 7 : 0xff;
	
	console_color(color&color_mask, selected);
	printf("%-*u%c", digits, msg.num, msg.flagged ? 251 : msg.attr & MSG_DELETE ? '-' : ' ');
	color = LIGHTMAGENTA;
	if(color_cfg.column[1] != undefined)
		color = color_cfg.column[1];
	if(sort == "from")
		color |= color_cfg.sorted;
	color &= color_mask;
	console_color(color++, selected);
	printf("%-*.*s%c", LEN_ALIAS, LEN_ALIAS, property_value(msg, 'from', is_operator), selected ? '<' : ' ');
		
	if(!js.global.console || console.screen_columns >= 80) {
		for(var i = 0; i < list_formats[list_format].length; i++) {
			var prop = list_formats[list_format][i];
			if(exclude.indexOf(prop) >= 0)
				continue;
			var heading = columnHeading(prop);
			var fmt = "%-*.*s";
			var len = Math.max(heading.length, max_len(prop));
			if(i > 0)
				len++;
			var last_column = (i == list_formats[list_format].length - 1);
			if(js.global.console) {
				if(i > 0) {
					if(last_column)
						fmt = "%*.*s";
					else
						console.print(" ");
				}
				var cols_remain = console.screen_columns - console.current_column - 1;
				if(last_column)
					len = cols_remain;
				else
					len = Math.min(len, cols_remain);
			}
			if(color > WHITE)
				color = DARKGRAY;
			var custom_color = color_cfg.column[i+2];
			if(custom_color != undefined)
				color = custom_color;
			if(prop == sort)
				color |= color_cfg.sorted;
			color &= color_mask;
			console_color(color++, selected);
			if(typeof msg[prop] == 'number')	// Right-justify numbers
				fmt = "%*.*s";
			printf(fmt, len, len, property_value(msg, prop));
		}
	}
}

function msg_pmode(msgbase, msg)
{
	var pmode = msg.is_utf8 ? P_UTF8 : P_NONE;
	if(msg.from_ext !== "1")
		pmode |= P_NOATCODES;
	if(msgbase.cfg) {
		pmode |= msgbase.cfg.print_mode;
		pmode &= ~msgbase.cfg.print_mode_neg;
	}
	return pmode;
}

function view_msg(msgbase, msg, lines, total_msgs, grp_name, sub_name, is_operator)
{
	var show_hdr = true;
	var line_num = 0;
	var hdr_len;
//	console.clear();
	msg.lines = lines.length;
	
	while(bbs.online && !js.terminated) {
		if(show_hdr) {
			console.home();
			console.status |= CON_CR_CLREOL;
			bbs.show_msg_header(msg
				, property_value(msg, 'subject')
				, property_value(msg, 'from', is_operator)
				, msg.forward_path || msg.to_list || msg.to);
			console.status &= ~CON_CR_CLREOL;
			hdr_len = console.line_counter;
			if(console.term_supports(USER_ANSI))
				show_hdr = false;
		} else
			console.gotoxy(1, hdr_len + 1);
		var pagesize = console.screen_rows - (hdr_len + 2);
		if(line_num + pagesize >= lines.length)
			line_num = lines.length - pagesize;
		else if(lines.length > pagesize && line_num >= lines.length - pagesize)
			line_num = lines.length - (pagesize + 1);
		if(line_num < 0)
			line_num = 0;
		var i = line_num;
		var row = hdr_len;
		var pmode = msg_pmode(msgbase, msg);
		while(row < (console.screen_rows - 2) && bbs.online) {
			console.line_counter = 0;
			if(i < lines.length)
				console.putmsg(lines[i++].trimRight(), pmode);
			console.cleartoeol();
			console.crlf();
			row++;
		}
//		if(line_num == 0)
//			bbs.download_msg_attachments(msg);
		var line_range = "no content";
		if(lines && lines.length) {
			line_range = format(options.view_lines_fmt || "%slines %u-%u"
				,content_description(msg), line_num + 1, i);
			line_range += format(options.view_total_lines_fmt || " of %u", lines.length);
		}
		right_justify(format(options.view_line_range_fmt || "\x01n\x01h\x01k[%s]", line_range));
		console.crlf();
		console.print(format("\x01n\x01c\%s \x01h\x01bReading \x01n\x01c\%s %s \x01h%s " +
			"\x01n\x01c(\x01h?\x01n\x01c=Menu) (\x01h%u\x01n\x01c of \x01h%u\x01n\x01c): \x01n\x01>"
			, line_num ? "\x18" : "\xfe"
			, line_num + pagesize < lines.length ? "\x19" : "\xfe"
			, grp_name
			, sub_name
			, msg.num
			, total_msgs));
		if((msgbase.attributes & SMB_EMAIL) && !(msg.attr&MSG_READ) && (msg.to_ext == user.number)) {
			if(!update_msg_attr(msgbase, msg, msg.attr |= MSG_READ))
				alert("failed to add read attribute");
		}
		bbs.node_action = node_action;
		bbs.nodesync(/* clearline: */false);
		// Only message text nav keys are handled here
		var key = console.getkeys(total_msgs, K_UPPER|K_NOCRLF);
		switch(key) {
			case KEY_DOWN:
				if(i < lines.length)
					line_num++;
				break;
			case KEY_UP:
				if(line_num)
					line_num--;
				break;
			case KEY_HOME:
				line_num = 0;
				break;
			case KEY_END:
				line_num = lines.length;
				break;
			case KEY_PAGEUP:
				line_num -= pagesize;
				break;
			case KEY_PAGEDN:
			case ' ':
				line_num += pagesize;
				break;
			/* TODO: support F5? */
			case CTRL_R:	/* refresh */
				show_hdr = true;
				break;
			default:
				return key;
		}
	}
	return null;
}

function right_justify(text)
{
	console.print(format("%*s%s", console.screen_columns - console.current_column - (console.strlen(text) + 1), "", text));
}

function find(msgbase, search_list, txt)
{
	var new_list=[];
	var text=txt.toUpperCase();
	for(var i = 0; i < search_list.length; i++) {
		var msg = search_list[i];
		var msgtxt = msg.text;
		if(msgtxt === undefined)
			msgtxt = msgbase.get_msg_body(msg);
		if(!msgtxt)
			continue;
		if(msgtxt.toUpperCase().indexOf(text) >= 0)
			new_list.push(msg);
		else {
			for(var p in msg) {
				if(typeof msg[p] != 'string')
					continue;
				if(msg[p].toUpperCase().indexOf(text) >= 0) {
					new_list.push(msg);
					break;
				}
			}
		}
	}
	return new_list;
}

function line_split(text, chop)
{
	var maxlen = console.screen_columns - 1;
	text = text.split('\n');
	for(var i = 0; i < text.length; i++) {
		// truncate trailing white-space and replace tabs w/spaces
		text[i] = text[i].trimRight().replace(/\x09/g, '    ');
		if(console.strlen(text[i]) > maxlen) {
			if(chop !== true)
				text.splice(i + 1, 0, text[i].substring(maxlen));
			text[i] = text[i].substring(0, maxlen);
		}
	}
	return text;
}

// Return an array of lines of text from the message body/tails/header fields
function get_msg_lines(msgbase, msg, hdr, source, hex, wrap, chop)
{
	if(msg.text && msg.hdr === hdr && msg.source === source && msg.hex == hex && msg.wrapped == wrap) {
		msg.lines = msg.text.length;
		return msg.text;
	}
	const preparing_fmt = options.preparing_preview_fmt || "%25s";
	msg.hex = hex;
	msg.hdr = hdr;
	msg.source = (source===true && !hex);
	msg.wrapped = false;
	msg.html = false;
	msg.ansi = false;
	var text;
	if(!hdr) {
		console.print(format(preparing_fmt, options.reading_message_text || "\x01[Reading message text ..."));
		text = msgbase.get_msg_body(msg
					,/* strip ctrl-a */msg.source
					,/* dot-stuffing */false
					,/* tails */true
					,/* plain-text */!msg.source);
		if(!text) {
			console.clearline();
			return [];
		}
		if(text.length <= options.large_msg_threshold)
			msg.wrapped = (wrap!==false && !hex);
	}
	if(hdr) {
		text = line_split(msgbase.dump_msg_header(msg).join('\n'));
	} else if(hex) {
		console.print(format(preparing_fmt, options.preparing_hex_dump || "\x01[Preparing hex-dump ..."));
		text = text.slice(0, options.large_msg_threshold);
		text = hexdump.generate(undefined, text, /* ASCII: */true, /* offsets: */true);
	} else {
		if(msg.source)
			text = msg.get_rfc822_header(/* force_update: */false, /* unfold: */false) + text;
		else {
			if(msg.is_utf8 && !console.term_supports(USER_UTF8)) {
				console.print(format(preparing_fmt, options.translating_charset || "\x01[Translating charset ..."));
				text = utf8_cp437(text);
			}
			if((msg.text_subtype && msg.text_subtype.toLowerCase() == 'html')
				|| (msg.content_type && msg.content_type.toLowerCase().indexOf("text/html") == 0)) {
				console.print(format(preparing_fmt, options.decoding_html || "\x01[Decoding HTML ..."));
				text = html2asc(text);
				msg.html = true;
				// remove excessive blank lines after HTML-translation
				text = text.replace(/\r\n\r\n\r\n/g, '\r\n\r\n');
			}
		}
		text = text.replace(/\xff/g, ' ');	// Use a regular old space for nbsp
		msg.ansi = text.indexOf("\x1b[") >= 0;
		if(msg.ansi) {
			var graphic = new Graphic(console.screen_columns, 10);
			graphic.auto_extend = true;
			graphic.ANSI = ansiterm.expand_ctrl_a(text);
			graphic.width = console.screen_columns - 1;
			text = graphic.MSG.split('\n');
		} else {
			if(msg.wrapped) {
				console.print(format(preparing_fmt, options.wrapping_lines || "\x01[Wrapping lines ..."));
				text = word_wrap(text, console.screen_columns - 1, (msg.columns || 80) - 1).split('\n');
			} else {
				console.print(format(preparing_fmt, options.splitting_lines || "\x01[Splitting lines ..."));
				text = line_split(text, chop);
			}
		}
		while(text.length && !text[0].trim().length)
			text.shift(); // Remove initial blank lines
		while(text.length && !text[text.length - 1].trim())
			text.pop();	// Remove trailing blank lines
	}
	msg.lines = text.length;
	if(options.cache_msg_text && text.length < options.large_msg_threshold)
		msg.text = text;
	console.clearline();
	return text;
}

function prop_val(msg, prop)
{
	var val = msg[prop].toLowerCase();

	if(prop === 'subject') {
		while(val.slice(0, 3) == 're:') {
			val = val.slice(3);
			while(val.slice(0, 1) == ' ')
				val = val.slice(1);
		}
	}
	return val;
}

function next_msg(list, current, prop)
{
	const val = prop_val(list[current], prop);
	var next = current + 1;
	while(next < list.length) {
		if(prop_val(list[next], prop) == val)
			break;
		next++;
	}
	if(next < list.length)
		return next;
	console_beep();
	return current;
}

function prev_msg(list, current, prop)
{
	const val = prop_val(list[current], prop);
	var prev = current - 1;
	while(prev >= 0) {
		if(prop_val(list[prev], prop) == val)
			break;
		prev--;
	}
	if(prev >= 0)
		return prev;
	console_beep();
	return current;
}

function mail_reply(msg, reply_all)
{
	console.clear();
	var success = false;
	if(msg.from_net_type || msg.replyto_net_type) {
		var addr;
		if(msg.replyto_net_type)
			addr = msg.replyto_net_addr;
		else if(msg.from_net_addr.indexOf('@') < 0)
			addr = msg.from + '@' + msg.from_net_addr;
		else
			addr = msg.from_net_addr;
		if(reply_all) {
			if(msg.to_list && msg.to_list != msg.to)
				addr += "," + msg.to_list;
			if(msg.cc_list && msg.cc_list != msg.to)
				addr += "," + msg.cc_list;
		}
		console.putmsg(bbs.text(EnterNetMailAddress));
		addr = console.getstr(addr, 128, K_EDIT|K_AUTODEL|K_LINE);
		if(!addr || console.aborted)
			return false;
		success = bbs.netmail(addr.split(','), msg);
	} else if(msg.from_ext)
		success = bbs.email(parseInt(msg.from_ext, 10), msg);
	if(!success) {
		alert("Failed to send");
		console.pause();
	}
	return success;
}

function download_msg(msg, msgbase, plain_text)
{
	var fname = system.temp_dir + "msg_" + msg.number + ".txt";
	var f = new File(fname);
	if(!f.open("wb"))
		return false;
	var text = msgbase.get_msg_body(msg
				,/* strip ctrl-a */plain_text
				,/* dot-stuffing */false
				,/* tails */true
				,plain_text);
	f.write(msg.get_rfc822_header(/* force_update: */false, /* unfold: */false
		,/* default_content_type */!plain_text));
	f.writeln(text);
	f.close();
	return bbs.send_file(fname);
}

function download_thread(thread_id, msgbase, plain_text)
{
	var fname = system.temp_dir + "thread_" + thread_id + ".txt";
	var f = new File(fname);
	if(!f.open("wb"))
		return false;
	const total_msgs = msgbase.total_msgs;
	for (var i = 0; i < total_msgs; ++i) {
		var msg = msgbase.get_msg_header(true, i);
		if (msg == null || msg.thread_id !== thread_id)
			continue;
		var text = msgbase.get_msg_body(msg
				,/* strip ctrl-a */plain_text
				,/* dot-stuffing */false
				,/* tails */true
				,plain_text);
		f.write(msg.get_rfc822_header(/* force_update: */false, /* unfold: */false
			,/* default_content_type */!plain_text));
		f.writeln(text);
	}
	f.close();
	return bbs.send_file(fname);
}

function content_description(msg)
{
	var desc = [];
	if(msg.hdr)
		desc.push('header');
	else if(msg.hex)
		desc.push('hex-dumpped');
	else if(msg.ansi)
			desc.push('ANSI');
	else {
		if(msg.wrapped)
			desc.push('wrapped');
		if(msg.source)
			desc.push('source');
		else {
			if(msg.text_charset)
				desc.push(msg.text_charset.toUpperCase());
			else if(msg.is_utf8)
				desc.push("UTF-8");
			if(msg.html)
				desc.push('HTML');
		}
	}
	var retval = desc.join(' ');
	if(retval) retval += ' ';
	return retval;
}

function remove_extra_blank_lines(list)
{
	for(var i = 0; i < list.length; i++) {
		if(truncsp(list[i]).length < 1 
			&& list[i + 1] !== undefined
			&& truncsp(list[i + 1]).length < 1)
			list.splice(i + 1, 1);
	}
}

function list_msgs(msgbase, list, current, preview, grp_name, sub_name)
{
	var mail = (msgbase.attributes & SMB_EMAIL);	
	if(!current)
		current = 0;
	else {
		for(var i = 0; i <= list.length; i++) {
			if(list[i] && list[i].number == current) {
				current = i;
				break;
			}
		}
	}
	var top = current;
	var sort;
	var reversed = false;
	var orglist = list.slice();
	var digits = format("%u", list.length).length;
	if(!preview)
		preview = false;
	var msg_line = 0;
	var msg_ctrl = false;
	var spam_visible = true;
	var view_hdr = false;
	var view_hex = false;
	var view_wrapped = true;
	var view_source = false;
	var last_msg;
	var area_name = format(options.area_name_fmt || "\x01n\x01k\x017%s / %s\x01h"
						,grp_name, sub_name);
	var is_operator = user.is_sysop || (msgbase.cfg && msg_area.sub[msgbase.cfg.code].is_operator);

	console.line_counter = 0;
	console.status |= CON_MOUSE_SCROLL;
	while(bbs.online && !js.terminated) {
		if(!last_msg)
			last_msg = msgbase.last_msg;
		else if(msgbase.last_msg != last_msg)
			return true;	// Reload new messages
		var pagesize = console.screen_rows - 3;
		if(preview)
			pagesize = Math.floor(pagesize / 2);
		console.clear_hotspots();
		console.home();
		console.current_column = 0;
		console.print(area_name);

		/* Bounds checking: */
		if(current < 0) {
			console_beep();
			current = 0;
		} else if(current >= list.length) {
			console_beep();
			current = list.length-1;
		}
		
		if(list[current]) {
			if(msgbase.cfg) // Update "last read" pointer
				msg_area.sub[msgbase.cfg.code].last_read = list[current].number;
			else
				userprops.last_read_mail = list[current].number;
		}
		if(console.screen_columns >= 80)
			right_justify(format(options.area_nums_fmt || "[message %u of %u]"
				, current + 1, list.length));
		console.cleartoeol();
		console.crlf();
		if(list_format >= list_formats.length)
			list_format = 0;
		else if(list_format < 0)
			list_format = list_formats.length-1;
		
		var exclude_heading = [];
		if(!spam_visible)
			exclude_heading.push("spam");
		
		/* Column headings */
		console.add_hotspot(CTRL_S);
		printf("%-*s ", digits, "#");
		console.add_hotspot(CTRL_T);
		printf("%-*s ", LEN_ALIAS, sort=="from" ? "FROM" : "From");
		if(console.screen_columns >= 80) {
			for(var i = 0; i < list_formats[list_format].length; i++) {
				var prop = list_formats[list_format][i];
				if(exclude_heading.indexOf(prop) >= 0)
					continue;
				var fmt = "%-*.*s";
				var heading = columnHeading(prop);
				var last_column = (i == (list_formats[list_format].length - 1));
				if(last_column) {
					if(i > 0)
						fmt = "%*.*s";
				} else {
					if(i > 0 && max_len(prop) >= heading.length)
						console.print(" ");
				}
				var cols_remain = console.screen_columns - console.current_column - 1;
				var len = cols_remain;
				if(!last_column) {
					len = Math.min(max_len(prop), cols_remain);
					len = Math.max(heading.length, len);
				}
				console.add_hotspot(ascii(ascii(CTRL_U) + i));
	//			console.add_hotspot(CTRL_U);
				printf(fmt
					,len
					,len
					,sort==prop ? heading.toUpperCase() : heading);
			}
		}
		console.cleartoeol();
		console.crlf();

		if(top > current)
			top = current;
		if(top && (top + pagesize) > list.length)
			top = list.length-pagesize;
		if(top + pagesize <= current)
			top = (current+1) - pagesize;
		if(top < 0)
			top = 0;

		for(var i = top; i - top < pagesize; i++) {
			console.line_counter = 0;
			console.attributes = LIGHTGRAY;
			if(list[i] !== undefined && list[i] !== null) {
				console.add_hotspot(CTRL_G + list[i].num + '\r');
				list_msg(list[i], digits, i == current, sort, msg_ctrl, exclude_heading, is_operator);
			}
			console.cleartoeol();
			console.crlf();
		}
		if(preview && list[current]) {
			var msg = list[current];
			var text = get_msg_lines(msgbase, msg);
			remove_extra_blank_lines(text);
			var default_separator = "\xc4";
			console.attributes = msg_ctrl ? color_cfg.preview_separator_active : color_cfg.preview_separator_inactive;
			while(console.current_column < digits - 1)
				write(options.preview_separator || default_separator);
			write(options.preview_label || (console.screen_columns < 80 ? "\xd9" : "\xd9 Preview"));
			var offset = pagesize + 4;
			if(text) {
				if(text.length) {
					if(msg_line < 0 || !msg_ctrl)
						msg_line = 0;
					else {
						var max = Math.max(text.length - 1, text.length - offset);
						if(msg_line > max)
							msg_line = max;
					}
	//				log(format("text lines(%s): %d\n", typeof text, text.length));
					var max = Math.min(console.screen_rows - offset, text.length);
					if(msg_line + max > text.length)
						msg_line = text.length - max;
					write(format(options.preview_lines_fmt || " Lines %u-%u"
						, msg_line + 1, msg_line + max));
					if(max < text.length)
						write(format(options.preview_total_lines_fmt || " of %u", text.length));
				}
				write(options.preview_label_terminator || " \xc0");
				if(options.preview_properties) {
					var array = options.preview_properties.split(',');
					var propval = [];
					for(var i in array) {
						if(options.hide_redundant_properties !== false
							&& list_formats[list_format].indexOf(array[i]) >= 0)
							continue;
						var val = property_value(msg, array[i]);
						if(val)
							propval.push(format("%.*s", options.preview_properties_maxlen || 10, val));
					}
					propval = propval.join(options.preview_properties_separator || ', ').trim();
					if(propval.length) {
						while(console.current_column < console.screen_columns - (4 + propval.length + digits))
							write(options.preview_separator || default_separator);
						write(format(options.preview_properties_fmt || "\xd9 %s \xc0", propval));
					}
				}
				while(console.current_column < console.screen_columns - 1)
					write(options.preview_separator || default_separator);
			} else {
				write("!Error getting msg text: " + msgbase.error);
				msg_line = 0;
			}
			console.cleartoeol();
			console.crlf();
			console.attributes = msg_ctrl ? color_cfg.preview_active : color_cfg.preview_inactive;
			var pmode = msg_pmode(msgbase, msg);
			for(var i = offset; i < console.screen_rows; i++) {
				if(text !== null && text[msg_line + (i - offset)])
					console.putmsg(strip_ctrl(text[msg_line + (i - offset)]), pmode);
				console.cleartoeol();
				console.crlf();
			}
		}
		
		var cmds = [];
		if(!preview)
			cmds.push("~Preview");
		if(list[current]) {
			if(!(list[current].attr&MSG_NOREPLY)) {
				if(mail)
					cmds.push("~Reply");
				else
					cmds.push("~Reply/~Mail");
			}
			if(list[current].auxattr&(MSG_FILEATTACH|MSG_MIMEATTACH))
				cmds.push("~Dload");
		}
		cmds.push("~Goto");
		cmds.push("~Find");
		var fmt = ", fmt:0-%u"
		if(console.term_supports(USER_ANSI))
			fmt += ", Hm/End/\x18\x19/PgUpDn";
		fmt += ", ~Quit or [~View] ~?";
		if(console.screen_columns < 64) {
			for(var i = 0; i < cmds.length; i++)
				cmds[i] = cmds[i].substring(0, 2);
		}
		console.mnemonics(cmds.join(", ") + format(fmt, list_formats.length-1));
		console.cleartoeol();
		bbs.node_action = node_action;
		bbs.nodesync(/* clearline: */false);
//		console.mouse_mode = 1<<1;
		var key = console.getkey();
		switch(key.toUpperCase()) {
			case CTRL_A:
				var flagged = true;
				if(list[0] && list[0].flagged)
					flagged = false;
				for(var i in list)
					list[i].flagged = flagged;
				break;
			case CTRL_D:
			case KEY_DEL:
				if(!list[current])
					break;
				if(msgbase.cfg && !msg_area.sub[msgbase.cfg.code].is_operator)
					break;
				flagged = 0;
				for(var i in list)
					if(list[i].flagged) {
						if(!update_msg_attr(msgbase, list[i], list[i].attr | MSG_DELETE))
							alert("Delete failed");
						flagged++;
					}
				if(!flagged) {
					if(!update_msg_attr(msgbase, list[current], list[current].attr ^ MSG_DELETE))
						alert("Delete failed");
					current++;
				}
				break;
			case CTRL_G:
				var num = console.getstr(digits,K_LINE|K_NUMBER|K_NOCRLF|K_NOECHO);
				if(num) {
					for(var i = 0; i < list.length; i++) {
						if(list[i].num == num) {
							top = current = i;
							break;
						}
					}
				} else
					break;
				// fall-through
			case 'V':
			case '\r':
				console.clear();
				var viewed_msg = current;
				while(bbs.online && !js.terminated && list[current]
					&& (key = view_msg(msgbase, list[current]
						,get_msg_lines(msgbase, list[current], view_hdr, view_source, view_hex, view_wrapped)
						,orglist.length
						,grp_name, sub_name
						,is_operator
						)) != 'Q') {
					switch(key) {
						case '?':
							console.clear();
							bbs.menu("msgview");
							console.pause();
							break;
						case '\r':
						case '\n':
							current++;
							break;
						case '\b':
						case '-':
							current--;
							break;
						case CTRL_D:
						case KEY_DEL:
							if(msgbase.cfg && !msg_area.sub[msgbase.cfg.code].is_operator)
								break;
							if(!update_msg_attr(msgbase, list[current], list[current].attr ^ MSG_DELETE))
								alert("Delete failed");
							break;
						case KEY_LEFT:
							if(!list[current].columns)
								list[current].columns = 79;
							else
								list[current].columns--;
							break;
						case KEY_RIGHT:
							if(!list[current].columns)
								list[current].columns = 81;
							else
								list[current].columns++;
							break;							
						case '>':
							current = next_msg(list, current, 'subject');
							break;
						case '<':
							current = prev_msg(list, current, 'subject');
							break;
						case '}':
							current = next_msg(list, current, 'from');
							break;
						case '{':
							current = prev_msg(list, current, 'from');
							break;
						case ']':
							current = next_msg(list, current, 'to');
							break;
						case '[':
							current = prev_msg(list, current, 'to');
							break;
						case 'A':
						case 'R':
							if(mail) {
								if(mail_reply(list[current], key == 'A'))
									update_msg_attr(msgbase, list[current], list[current].attr | MSG_REPLIED);
								pause();
							} else {
								console.clear();
								bbs.post_msg(msgbase.subnum, list[current]);
								pause();
								return true;
							}
							break;
						case 'E':
							if((msgbase.cfg && msg_area.sub[msgbase.cfg.code].is_operator)
								|| list[current].from_ext == user.number)
								bbs.edit_msg(list[current]);
							break;
						case 'F':
							if(mail) {
								console.clearline();
								var dest = prompt("To", "", K_NOCRLF);
								if(dest) {
									console.clear();
									if(!bbs.forward_msg(list[current], dest))
										alert("failed");
									pause();
								}
							}
							break;
						case 'M':
							if(mail_reply(list[current]) && mail)
								update_msg_attr(msgbase, list[current], list[current].attr | MSG_REPLIED);
							pause();
							break;
						case 'D':
							console.clearline();
							if(list[current].thread_id && !console.noyes("Download thread", P_NOCRLF)) {
								if(!download_thread(list[current].thread_id, msgbase, console.yesno("Plain-text only")))
									alert("failed");
							}
							else if(!console.noyes("Download message", P_NOCRLF)) {
								if(!download_msg(list[current], msgbase, console.yesno("Plain-text only")))
									alert("failed");
							}
							console.creturn();
							bbs.download_msg_attachments(list[current]);
							break;
						case 'S':
							view_source = !view_source;
							view_hex = false;
							list[current].text = null;
							break;
						case 'H':
							view_hex = !view_hex;
							list[current].text = null;
							break;
						case 'W':
							view_wrapped = !view_wrapped;
							list[current].text = null;
							break;
						case 'O':
							if(!is_operator)
								break;
							while(bbs.online && !js.terminated) {
								if(!(user.settings & USER_EXPERT)) {
									console.clear(LIGHTGRAY);
									bbs.menu("sysmscan");
									console.crlf();
								}
								console.putmsg(options.operator_prompt || "\x01[\x01>\x01y\x01hOperator: ", K_NOCRLF);
								switch(console.getkeys("?QHC\r", 0, K_NOCRLF|K_UPPER)) {
									case '?':
										if(user.settings & USER_EXPERT) {
											console.line_counter = 0;
											bbs.menu("sysmscan", P_NOCRLF);
											console.crlf();
										}
										continue;
									case 'H':
										view_hdr = !view_hdr;
										list[current].text = null;
										break;
									case 'C':
									{
										var attr = bbs.change_msg_attr(list[current]);
										if(attr == list[current].attr)
											break;
										if(!update_msg_attr(msgbase, list[current], attr))
											alert("Message attribute update failed");
										break;
									}
								}
								break;
							}
							break;
						default:
							if(typeof key == "number") {
								for(var i = 0; i < list.length; ++i) {
									if(list[i].num == key) {
										current = i;
										break;
									}
								}
								break;
							}
							break;
					}
					if(viewed_msg != current) {
						console.clear();
						viewed_msg = current;
					}
				}
				break;
			case 'D':
				console.clearline();
				if(!console.noyes("Download message", P_NOCRLF)) {
					if(!download_msg(list[current], msgbase, console.yesno("Plain-text only")))
						alert("failed");
					continue;
				}
				console.creturn();
				bbs.download_msg_attachments(list[current]);
				continue;
			case 'Q':
				console.clear();
				return false;
			case CTRL_R:				
				return true;
			case KEY_HOME:
				if(msg_ctrl)
					msg_line = 0;
				else
					current=top=0;
				continue;
			case KEY_END:
				if(msg_ctrl)
					msg_line = 9999999;
				else
					current=list.length-1;
				continue;
			case KEY_UP:
				if(msg_ctrl)
					msg_line--;
				else
					current--;
				break;
			case KEY_DOWN:
				if(msg_ctrl)
					msg_line++;
				else
					current++;
				break;
			case ' ':
				if(list[current].flagged)
					list[current].flagged = false;
				else
					list[current].flagged = true;
				if(!msg_ctrl)
					current++;
				break;
			case KEY_PAGEDN:
			case 'N':
				current += pagesize;
				top += pagesize;
				break;
			case KEY_PAGEUP:
				current -= pagesize;
				top -= pagesize;
				break;
			case 'P':
				preview = !preview;
				break;
			case '\t':
				preview = true;
				msg_ctrl = !msg_ctrl;
				break;
			case '/':
			case 'F':
			{
				console.clearline();
				console.print("\x01n\x01y\x01hFind: ");
				var search = console.getstr(60,K_LINE|K_UPPER|K_NOCRLF|K_TRIM);
				console.clearline(LIGHTGRAY);
				if(search && search.length) {
					console.print("Searching \x01i...\x01n");
					var found = find(msgbase, orglist, search);
					if(found.length) {
						list = found;
						current = 0;
					} else {
						console.print("\x01[Text not found: " + search);
						sleep(1500);
					}
				}
				break;
			}
			case 'G':
				console.clearline();
				console.print("\x01n\x01y\x01hGo to Message: ");
				var num = console.getstr(digits,K_LINE|K_NUMBER|K_NOCRLF);
				if(num) {
					for(var i = 0; i < list.length; i++) {
						if(list[i].num == num) {
							top = current = i;
							break;
						}
					}
				}
				console.clearline(LIGHTGRAY);
				break;
			case 'A':
			case 'R':
				if(mail) {
					if(mail_reply(list[current], key.toUpperCase() == 'A'))
						update_msg_attr(msgbase, list[current], list[current].attr | MSG_REPLIED);
					pause();
				} else {
					console.clear();
					bbs.post_msg(msgbase.subnum, list[current]);
					pause();
					return true; // reload msgs
				}
				break;
			case 'M':
				if(mail_reply(list[current]) && mail)
					update_msg_attr(msgbase, list[current], list[current].attr | MSG_REPLIED);
				pause();
				break;
			case 'S':
				if(sort == undefined)
					sort = (key == 'S') ? list_formats[list_format][list_formats[list_format].length - 1] : "from";
				else {
					var sort_field = list_formats[list_format].indexOf(sort);
					if(sort_field >= list_formats[list_format].length)
						sort = undefined;
					else if(key == 'S')	{ /* capital 'S', move backwards through sort fields */
						if(sort == "from")
							sort = undefined;
						else if(sort_field)
							sort = list_formats[list_format][sort_field-1];
						else
							sort = "from";
					} else {
						if(sort_field < 0)
							sort = list_formats[list_format][0];
						else
							sort = list_formats[list_format][sort_field+1];
					}
				}
				if(sort == undefined)
					list = orglist.slice();
				else {
					sort_reversed = reversed;
					sort_property = sort;
					write("\x01[\x01iSorting...\x01n\x01>");
					list.sort(sort_compare);
				}
				break;
			case CTRL_S:
				if(sort_property != 'num') {
					list = orglist.slice();
					sort_property = 'num';
					sort = undefined;
				} else {
					list.reverse();
					reversed = !reversed;
				}
				break;
			case 'T':
				list = orglist.slice();
				spam_visible = !spam_visible;
				if(!spam_visible) {
					list = [];
					for(i in orglist) {
						if(!(orglist[i].attr & MSG_SPAM))
							list.push(orglist[i]);
					}
				}
				break;
			case '!':
				list.reverse();
				reversed = !reversed;
				break;
			case KEY_LEFT:
				list_format--;
				break;
			case KEY_RIGHT:
				list_format++;
				break;
			case '?':
			case 'H':
				help();
				break;
			default:
				if(key>='0' && key <='9')
					list_format = parseInt(key);
				
				else if(key >= CTRL_T && key < ' ') {
					var sort = "from";
					if(key !== CTRL_T)
						sort = list_formats[list_format][ascii(key) - ascii(CTRL_U)];
					if(sort == sort_property) {
						list.reverse();
						reversed = !reversed;
						break;
					}
					sort_reversed = false;
					sort_property = sort;
					write("\x01[\x01iSorting...\x01n\x01>");
					list.sort(sort_compare);
				}
				break;
		}
	}
	return false;
}

function list_msgs_offline(msgbase, list)
{
	if(sort_property) {
		list.sort(sort_compare);
		if(lm_mode & LM_REVERSE)
			list.reverse();
	}
	/* Column headings */
	var exclude_heading = [];
	var digits = format("%u", list.length).length;
	if(list_hdr) {
		if(list_format_str) {
			var args = [list_format_str];
			for(var j = 0; j < list_formats[list_format].length; j++) {
				if(exclude_heading.indexOf(prop) >= 0)
					continue;
				args.push(columnHeading(list_formats[list_format][j]));
			}
			print(format.apply(null, args));
		} else {
			printf("%-*s ", digits, "#");
			printf("%-*s ", LEN_ALIAS, "From");
			for(var i = 0; i < list_formats[list_format].length; i++) {
				var prop = list_formats[list_format][i];
				if(exclude_heading.indexOf(prop) >= 0)
					continue;
				var fmt = "%-*.*s";
				var heading = columnHeading(prop);

				var last_column = (i == (list_formats[list_format].length - 1));
				if(last_column) {
					if(i > 0)
						fmt = "%*.*s";
				} else {
					if(i > 0 && max_len(prop) >= heading.length)
						printf(" ");
				}

				printf(fmt
					,max_len(prop)
					,max_len(prop)
					,heading);
			}
			print();
		}
	}
	for(var i = 0; i < list.length; i++) {
		var msg = list[i];
		if(list_format_str) {
			var args = [list_format_str];
			for(var j = 0; j < list_formats[list_format].length; j++) {
				var prop = list_formats[list_format][j];
				if(exclude_heading.indexOf(prop) >= 0)
					continue;
				args.push(property_value(msg, prop, /* is_operator */true));
			}
			print(format.apply(null, args));
		} else {
			list_msg(msg, digits, false, sort_property, /* msg_ctrl */false, exclude_heading, /* is_operator */true);
			print();
		}
	}
}

function msg_attributes(msg, msgbase, short)
{
	var result = [];
	var str = '';
	if(msg.attr&MSG_SPAM)							result.push(options.attr_spam || "Sp"), str += 'S';
	if(msg.auxattr&(MSG_FILEATTACH|MSG_MIMEATTACH))	result.push(options.attr_attach || "Att"), str += 'A';
	if(msg.attr&MSG_DELETE)							result.push(options.attr_delete || "Del"), str += 'D';
	if(msgbase.cfg 
		&& msg.number > msg_area.sub[msgbase.cfg.code].scan_ptr)
													result.push(options.attr_new || "New"), str += 'N';
	if(msg.attr&MSG_REPLIED)						result.push(options.attr_replied || "Re"), str += 'r';
	if(msg.attr&MSG_READ)							result.push(options.attr_read ||"Rd"), str += 'R';
	if(msg.attr&MSG_PERMANENT)						result.push(options.attr_perm ||"Perm"), str += 'P';
	if(msg.attr&MSG_ANONYMOUS)						result.push(options.attr_anon || "Anon"), str += 'A';
	if(msg.attr&MSG_LOCKED)							result.push(options.attr_locked || "Lck"), str += 'L';
	if(msg.attr&MSG_KILLREAD)						result.push(options.attr_kill || "KR"), str += 'K';
	if(msg.attr&MSG_NOREPLY)						result.push(options.attr_noreply || "NoRe"), str += 'n';
	if(msg.attr&MSG_MODERATED)						result.push(options.attr_mod || "Mod"), str += 'M';
	if(msg.attr&MSG_VALIDATED)						result.push(options.attr_valid || "Valid"), str += 'V';
	if(msg.attr&MSG_PRIVATE)						result.push(options.attr_private || "Priv"), str += 'p';
	if(msg.attr&MSG_POLL)							result.push(options.attr_poll || "Poll"), str += '?';
	if(msg.netattr&NETMSG_SENT)						result.push(options.attr_sent || "Sent"), str += 's';
	if(msg.netattr&NETMSG_KILLSENT)					result.push(options.attr_sent || "KS"), str += 'k';
	if(msg.netattr&NETMSG_INTRANSIT)				result.push(options.attr_intransit || "InTransit"), str += 'T';
	/*
	if(sub_op(subnum) && msg->hdr.attr&MSG_ANONYMOUS)	return 'A';
	*/
	if(short) {
		return str;
	}
	return result.join(options.attr_sep || ' ');
}

// Update a message header's attributes (only)
function update_msg_attr(msgbase, msg, attr)
{
	var hdr = msgbase.get_msg_header(msg.number, /* expand: */false);
	if(hdr == null) {
		alert("get_msg_header(" + msg.number + ") failed: " + msgbase.error);
		return false;
	}
	hdr.attr = attr;
	var result =  msgbase.put_msg_header(hdr);
	if(!result)
		alert("put_msg_header(" + msg.number + ") failed: " + msgbase.error);
	else {
		msg.attr = attr;
		msg.attributes = msg_attributes(msg, msgbase);
	}
	return result;
}

// Return an array of msgs
function load_msgs(msgbase, which, mode, usernumber, since)
{
	var mail = (msgbase.attributes & SMB_EMAIL);
	var list = [];
	msgbase.last_msg;	// Work around bug in MsgBase.get_index()/get_all_msg_headers() in v3.17c
	if(mail && which !== undefined && which != MAIL_ALL) {
		var idxlist = msgbase.get_index();
		var total_msgs = idxlist.length;
		for(var i = 0; i < total_msgs; i++) {
			var idx = idxlist[i];
			if((idx.attr&MSG_SPAM)) {
				if(mode&LM_NOSPAM)
					continue;
			} else {
				if(mode&LM_SPAMONLY)
					continue;
			}
			if((idx.attr&MSG_READ) && (mode&LM_UNREAD))
				continue;
			switch(which) {
				case MAIL_YOUR:
					if(idx.to != usernumber)
						continue;
					break;
				case MAIL_SENT:
					if(idx.from != usernumber)
						continue;
					break;
				case MAIL_ANY:
					if(idx.to != usernumber
						&& idx.from != usernumber)
						continue;
					break;
			}
			list.push(msgbase.get_msg_header(/* by_offset: */true, i, /* expand_fields: */false));
		}
	} else {
		list = msgbase.get_all_msg_headers(/* votes: */false, /* expand_fields: */false);
	}
	var msgs = [];
	for(var i in list) {
		var msg = list[i];
		if(since !== undefined && msg.when_imported_time < since)
			continue;
		msg.attributes = msg_attributes(msg, msgbase);
		msg.num = msgs.length + 1;
		msg.score = 0;
		if((msg.attr&MSG_DELETE) && !(mode&LM_INCDEL))
			continue;
		if(msg.can_read === false)
			continue;
		if(msg.upvotes)
			msg.score += msg.upvotes;
		if(msg.downvotes)
			msg.score -= msg.downvotes;
		msgs.push(msg);
	}
	if(mode&LM_REVERSE)
		msgs.reverse();
	return msgs;
}

var which;
var usernumber;
var lm_mode;
var preview;
var msgbase_code;
var since;
var count_only;
var all_subs;
var hide_zero;

for(var i in argv) {
	var arg = argv[i].toLowerCase();
	if(arg.indexOf("-sort=") == 0) {
		sort_property = arg.slice(6);
		continue;
	}
	if(arg.indexOf("-new=") == 0) {
		var days = parseInt(arg.slice(5), 10);
		since = time() - (days * (24 * 60 * 60));
		continue;
	}
	if(arg.indexOf("-fmt=") == 0) {
		list_format_str = arg.slice(5);
		continue;
	}
	if(arg.indexOf("-p=") == 0) {
		list_formats = [arg.slice(3).split(',')];
		list_format = 0;
		continue;
	}

	switch(arg) {
		case '-p':
		case '-preview':
			preview = true;
			break;
		case '-nospam':
			if(lm_mode === undefined)
				lm_mode = 0;
			lm_mode |= LM_NOSPAM;
			break;
		case '-spam':
			if(lm_mode === undefined)
				lm_mode = 0;
			lm_mode |= LM_SPAMONLY;
			break;
		case '-unread':
			if(lm_mode === undefined)
				lm_mode = 0;
			lm_mode |= LM_UNREAD;
			break;
		case '-reverse':
			if(lm_mode === undefined)
				lm_mode = 0;
			lm_mode |= LM_REVERSE;
			break;
		case '-all':
			which = MAIL_ALL;
			break;
		case '-sent':
			which = MAIL_SENT;
			break;
		case '-sort':
			sort_property = argv[++i];
			break;
		case '-hdr':
			list_hdr = true;
			break;
		case '-count':
			count_only = true;
			break;
		case '-all_subs':
			all_subs = true;
			break;
		case '-hide_zero':
			hide_zero = true;
			break;
		default:
			if(msgbase_code === undefined)
				msgbase_code = arg;
			else if(which === undefined)
				which = parseInt(arg, 10);
			else if(usernumber === undefined)
				usernumber = parseInt(arg, 10);
			else if(lm_mode === undefined)
				lm_mode = parseInt(arg, 10);
			break;
	}
}

if (!all_subs) {
	if(!msgbase_code) {
		if(js.global.bbs === undefined) {
			alert("No msgbase code specified");
			exit();
		}
		msgbase_code = bbs.cursub_code;
	}
}

var msgbase = new MsgBase(msgbase_code);
if(!msgbase.open() && !all_subs) {
	alert(msgbase.error);
	exit();
}

var options=load({}, "modopts.js", "msglist:" + msgbase_code);
if(!options)
	options=load({}, "modopts.js", "msglist");
if(!options)
	options = {};
if(options.large_msg_threshold === undefined)
	options.large_msg_threshold = 0x10000;
if(options.preview_properties === undefined)
	options.preview_properties = "date,attributes,subject";
if(options.reverse_mail === undefined)
	options.reverse_mail = true;
if(options.reverse_msgs === undefined)
	options.reverse_msgs = true;
if(options.date_fmt === undefined)
	options.date_fmt = "%Y-%m-%d";
// options.date_time_fmt = "%a %b %d %Y %H:%M:%S";

if(!msgbase.total_msgs && !all_subs) {
	alert("No messages");
	exit();
}

function remove_list_format_property(name)
{
	for(var i in list_formats) {
		var j = list_formats[i].indexOf(name);
		if(j >= 0) {
			list_formats[i].splice(j, 1);
			if(!list_formats[i].length)
				list_formats.splice(i, 1);
		}
	}
}

if(msgbase.cfg) {
	if(list_formats.length < 1) {
		list_formats = sub_list_formats;
		if(msgbase.cfg.settings & (SUB_FIDO | SUB_QNET | SUB_INET)) {
			list_formats = list_formats.concat(net_sub_list_formats);
			if(msgbase.cfg.settings & SUB_FIDO)
				list_formats = list_formats.concat(fido_sub_list_formats);
		} else
			list_formats = list_formats.concat(local_sub_list_formats);
		if(!(msgbase.cfg.settings & SUB_MSGTAGS))
			remove_list_format_property("tags");
		if(msgbase.cfg.settings & SUB_NOVOTING)
			remove_list_format_property("score");
		if(!(msgbase.cfg.settings & SUB_TOUSER))
			remove_list_format_property("to");
	}
} else {
	if(which === undefined)
		which = MAIL_YOUR;
	if(which != MAIL_YOUR)
		list_formats = list_formats.concat(mail_sent_list_formats);
	list_formats = list_formats.concat(mail_list_formats);
}
list_formats = list_formats.concat(extra_list_formats);

if(lm_mode === undefined)
	lm_mode = 0;

if((system.settings&SYS_SYSVDELM) && (user.is_sysop || (system.settings&SYS_USRVDELM)))
	lm_mode |= LM_INCDEL;

if(js.global.bbs && bbs.online) {

	if(msgbase.attributes & SMB_EMAIL) {
		if(isNaN(which))
			which = MAIL_YOUR;
		if(options.reverse_mail)
			lm_mode |= LM_REVERSE;
		if(lm_mode&(LM_NOSPAM | LM_SPAMONLY))
			remove_list_format_property("spam");
	} else {
		if(options.reverse_msgs)
			lm_mode |= LM_REVERSE;
	}

	if(!usernumber)
		usernumber = user.number;

	var userprops_lib = bbs.mods.userprops;
	if(!userprops_lib)
		userprops_lib = load(bbs.mods.userprops = {}, "userprops.js");
	var userprops_section = "msglist:" + msgbase_code;
	if(!msgbase.cfg)
		userprops_section += (":" + ["your","sent","any","all"][which]);

	var userprops = userprops_lib.get(userprops_section);
	if(!userprops)
		userprops = {};
	if(!options.track_last_read_mail)
		delete userprops.last_read_mail;

	js.on_exit("console.status = " + console.status);
	//console.status |= CON_CR_CLREOL;
	js.on_exit("console.ctrlkey_passthru = " + console.ctrlkey_passthru);
	console.ctrlkey_passthru |= (1<<16);      // Disable Ctrl-P handling in sbbs
	console.ctrlkey_passthru |= (1<<20);      // Disable Ctrl-T handling in sbbs
	console.ctrlkey_passthru |= (1<<21);      // Disable Ctrl-U handling in sbbs
	console.ctrlkey_passthru |= (1<<26);      // Disable Ctrl-Z handling in sbbs
	js.on_exit("bbs.sys_status &= ~SS_MOFF");
	bbs.sys_status |= SS_MOFF; // Disable automatic messages
} else {
	var sub_list = [];
	if (all_subs) {
		for (var sub in msg_area.sub) {
			sub_list.push(sub);
		}
	} else {
		sub_list.push(msgbase_code)
	}

	for(var i = 0; i < sub_list.length; i++)
	{
		var local_msgbase_code = sub_list[i];
		var msgbase = new MsgBase(local_msgbase_code);

		if(!msgbase.open()) {
			alert(msgbase.error);
			exit();
		}

		var list = load_msgs(msgbase, which, lm_mode, /* usernumber; */0, since);

		if (count_only) {
			if ( list.length || !hide_zero )
				printf("%10s  %-40s %5s\n", msgbase.cfg.grp_name, msgbase.cfg.description, list.length)
		} else {
			list_msgs_offline(msgbase, list);
		}

	}
	
	exit(0);
}

var grp_name;
var sub_name = "???";
if(msgbase.cfg) {
	grp_name = msgbase.cfg.grp_name;
	sub_name = msgbase.cfg.name;
} else {
	grp_name = "E-mail";
	switch(which) {
		case MAIL_YOUR:
			sub_name = "Inbox";
			node_action = NODE_RMAL;
			break;
		case MAIL_SENT:
			sub_name = "Sent";
			node_action = NODE_RSML;
			break;
		case MAIL_ALL:
			sub_name = "All";
			node_action = NODE_SYSP;
			break;
		case MAIL_ANY:
			sub_name = "Any";
			node_action = NODE_RMAL;
			break;
		default:
			sub_name = String(which);
			break;
	}
}

if(sort_property === undefined)
	sort_property = 'num';
do {
	console.print("\x01[\x01>Loading messages \x01i...\x01n ");
	var list = load_msgs(msgbase, which, lm_mode, usernumber);
	if(!list || !list.length) {
		alert("No " + ((lm_mode&LM_UNREAD) ? "Unread ":"") 
			+ grp_name + " / " + sub_name + " messages");
		break;
	}
	var curmsg = 0;
	if(msgbase.cfg)
		curmsg = msg_area.sub[msgbase.cfg.code].last_read;
	else
		curmsg = userprops.last_read_mail;
} while(list_msgs(msgbase, list, curmsg, preview, grp_name, sub_name) === true && bbs.online && !js.terminated);

userprops_lib.set(userprops_section, userprops);
