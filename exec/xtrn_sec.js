// xtrn_sec.js

// Synchronet External Program Section
// Menus displayed to users via Terminal Server (Telnet/RLogin/SSH)

// To jump straight to a specific xtrn section, pass the section code as an argument

"use strict";

require("sbbsdefs.js", "P_NOERROR");

/* text.dat entries */
require("text.js", "XtrnProgLstFmt");
require("select_item.js", "select_item");

/* See if an xtrn section code was passed as an argument */
/* must parse argv before calling load() */
var xsec=-1;
{
    var i,j;
    for(i in argv) {
        for(j in xtrn_area.sec_list) {
            if(argv[i].toLowerCase()==xtrn_area.sec_list[j].code)
                xsec=j;
        }
    }
}

var options;
if((options=load("modopts.js","xtrn_sec")) == null)
	options = {multicolumn: true, sort: false};	// default values

if(options.multicolumn === undefined)
	options.multicolumn = true;

if(options.multicolumn_separator === undefined)
	options.multicolumn_separator = " ";

if(options.multicolumn_fmt === undefined)
	options.multicolumn_fmt = bbs.text(XtrnProgLstFmt);

if(options.singlecolumn_fmt === undefined)
	options.singlecolumn_fmt = "\x01h\x01c%3u \xb3 \x01n\x01c%s\x01h ";

if(options.singlecolumn_margin == undefined)
	options.singlecolumn_margin = 7;

if(options.singlecolumn_height == undefined)
	options.singlecolumn_height = console.screen_rows - options.singlecolumn_margin;

if(console.screen_columns < 80) {
	options.multicolumn = false;
	options.center = false;
}

if(options.restricted_user_msg === undefined)
	options.restricted_user_msg = bbs.text(R_ExternalPrograms);

if(options.no_programs_msg === undefined)
	options.no_programs_msg = bbs.text(NoXtrnPrograms);

if(options.header_fmt === undefined)
	options.header_fmt = bbs.text(XtrnProgLstHdr);

if(options.titles === undefined)
	options.titles = bbs.text(XtrnProgLstTitles);

if(options.underline === undefined)
	options.underline = bbs.text(XtrnProgLstUnderline);

if(options.which === undefined)
	options.which = bbs.text(WhichXtrnProg);

if(options.clear_screen === undefined)
	options.clear_screen = true;

if (options.section_fmt === undefined)
	options.section_fmt = bbs.text(SelectItemFmt);

if (options.multicolumn_section_fmt === undefined)
	options.multicolumn_section_fmt = "\x01U%3d: \x01u%-32.32s ";

if (options.section_header_fmt === undefined)
	options.section_header_fmt = bbs.text(SelectItemHdr);

if(options.section_which === undefined)
	options.section_which = bbs.text(SelectItemWhich);

if(options.section_header_title === undefined)
	options.section_header_title = "External Program Section";

function sort_by_name(a, b)
{
	if(a.name.toLowerCase()>b.name.toLowerCase()) return 1;
	if(a.name.toLowerCase()<b.name.toLowerCase()) return -1;
	return 0;
}

function digits(n)
{
    if (n/10 == 0)
        return 1;
    return 1 + digits(Math.floor(n / 10));
}

// modopts.ini [xtrn_sec] default_item_prefix, nondefault_item_prefix and
// default_item_attr options: what marks the default item of a list (and what
// takes its place before the other items) and the attributes of its
// highlighted name. Without any of them, no default item is marked and the
// navigation keys only page through a list that doesn't fit on the screen.
var marked = options.default_item_prefix !== undefined
	|| options.nondefault_item_prefix !== undefined
	|| Boolean(options.default_item_attr);

function item_prefix(is_default)
{
	var prefix = is_default ? options.default_item_prefix : options.nondefault_item_prefix;
	return prefix === undefined ? "" : prefix;
}

function prefix_width()
{
	return Math.max(strip_ctrl(item_prefix(true)).length, strip_ctrl(item_prefix(false)).length);
}

// The name field of a list item format: a plain %s (width: 0) or one of a
// fixed width (e.g. %-32.32s). The name of the default item is highlighted
// only in such a field, so that a customized format is never misaligned.
function name_field(fmt)
{
	var m = fmt.match(/%(?:-(\d+)\.(\d+))?s/);
	if(!m || m[1] != m[2])
		return null;
	return { spec: m[0], width: m[1] ? parseInt(m[1], 10) : 0 };
}

function item_str(fmt, num, item, widest, is_default)
{
	var name = item.name;
	var field = (is_default && options.default_item_attr) ? name_field(fmt) : null;
	if(field) {
		name = options.default_item_attr + format("%-*s", widest, name.substr(0, widest)) + "\x01n";
		if(field.width > widest)
			name += format("%*s", field.width - widest, "");
		fmt = fmt.replace(field.spec, "%s");
	}
	// select_item() ends the line
	return format(fmt, num, name, item.cost).replace(/[\r\n]+$/, "");
}

// Display a numbered list of sections or programs, in one or two columns (or
// the menu file that takes its place) and prompt the user to choose from it.
// Returns what select_item() does: the index of the item chosen, -1 if the
// user quit or -2 for Enter when no item is the default.
// 'lst' describes the list:
// - fmt, multicolumn_fmt: the item formats (number, name, cost)
// - align: right-justify the item numbers
// - menu: function() displaying a menu file in place of the list, if there's one
// - has_menu: there's such a menu file
// - header: function(margin, indent, multicolumn) displaying what's above the items
// - footer: function() displaying what's below them
// - which: function(num) returning the prompt, num being the default item's number
function select_from(list, cur, lst)
{
	var multicolumn = options.multicolumn && list.length > options.singlecolumn_height;
	var center = options.center && !multicolumn;
	var fmt = multicolumn ? lst.multicolumn_fmt : lst.fmt;
	var field = name_field(fmt);
	var max_digits = digits(list.length);
	var pw = marked ? prefix_width() : 0;
	var widest_item = 0;	// as displayed, without attribute codes
	var i;

	for(i = 0; i < list.length; i++) {
		var w = strip_ctrl(item_str(fmt, i + 1, list[i], 0, false)).length;
		if(lst.align)
			w += max_digits - digits(i + 1);
		if(w > widest_item)
			widest_item = w;
	}
	// A marker that would make two columns wider than the screen is left out
	// (the highlighted name then marks the default item)
	if(pw && multicolumn && ((widest_item + pw) * 2)
		+ strip_ctrl(options.multicolumn_separator).length >= console.screen_columns)
		pw = 0;
	var has_default = marked && !lst.has_menu && Boolean(pw || (options.default_item_attr && field));

	// Center on the widest row's DISPLAYED width
	var margin = "";
	if(center) {
		var pad = Math.floor((console.screen_columns - (widest_item + pw)) / 2);
		if(pad > 0)
			margin = format("%*s", pad, "");
	}
	var indent = format("%*s", pw, "");	// what the titles have in place of the marker

	var name_max = (field && field.width) ? field.width
		: Math.max(console.screen_columns - margin.length - pw - 10, 1);
	var widest_name = 0;
	for(i = 0; i < list.length; i++)
		widest_name = Math.max(widest_name, Math.min(list[i].name.length, name_max));

	if(options.clear_screen)
		console.attributes = LIGHTGRAY;
	return select_item({
		count: list.length,
		current: has_default ? Math.min(cur, list.length - 1) : -1,
		no_default: !has_default,
		no_clear: !options.clear_screen,
		columns: multicolumn ? 2 : 1,
		column_separator: options.multicolumn_separator,
		header: function() { lst.header(margin, indent, multicolumn); },
		footer: lst.footer,
		menu: lst.menu,
		prompt: function(num) {
			if(lst.has_menu)	// the menu file includes its own prompt, if any
				return "";
			var which = lst.which(has_default ? num : cur + 1);
			return center ? "\r\n" + margin + which.trimLeft() : which;
		},
		print_item: function(i, is_default, redraw) {
			write(margin);
			if(pw)
				console.print(item_prefix(is_default));
			if(!redraw) {
				var hotspot = i + 1;
				if(digits(hotspot) < digits(list.length))
					hotspot += '\r';
				console.add_hotspot(hotspot);
			}
			if(lst.align)
				printf("%*s", max_digits - digits(i + 1), ""); // Indent to right justify number
			write(item_str(fmt, i + 1, list[i], widest_name, is_default));
		},
		more_fmt: options.more_fmt,
		more_indent: margin.length + pw + 1
	});
}

function external_program_menu(xsec)
{
	var curprog = 0;

	js.global.xtrn_sec = xtrn_area.sec_list[xsec].name;

	while(bbs.online) {

		console.aborted = false;
		if(user.security.restrictions&UREST_XTRN) {
			write(options.restricted_user_msg);
			break;
		}

		var prog_list=xtrn_area.sec_list[xsec].prog_list.slice();   /* prog_list is a possibly-sorted copy of xtrn_area.sec_list[x].prog_list */

		if(!prog_list.length) {
			write(options.no_programs_msg);
			console.pause();
			break;
		}

		// If there's only one program available to the user in the section, just run it (or try to)
		if(options.autoexec && prog_list.length == 1) {
			bbs.exec_xtrn(prog_list[0].code);
			break;
		}

		var secnum = xtrn_area.sec_list[xsec].number+1;
		var seccode = xtrn_area.sec_list[xsec].code;
		var menu = null;
		if(bbs.menu_exists("xtrn" + secnum))
			menu = "xtrn" + secnum;
		else if(bbs.menu_exists("xtrn" + seccode))
			menu = "xtrn" + seccode;
		else if(options.sort)
			prog_list.sort(sort_by_name);

		// Returns true if the section has its own (in place of the list's) header
		var show_head = function() {
			if(bbs.menu("xtrn" + secnum + "_head", P_NOERROR)
				|| bbs.menu("xtrn" + seccode + "_head", P_NOERROR))
				return true;
			bbs.menu("xtrn_head", P_NOERROR);
			return false;
		};
		var show_tail = function() {
			if(!bbs.menu("xtrn" + secnum + "_tail", P_NOERROR)
				&& !bbs.menu("xtrn" + seccode + "_tail", P_NOERROR))
				bbs.menu("xtrn_tail", P_NOERROR);
		};

		var has_tail = bbs.menu_exists("xtrn" + secnum + "_tail")
			|| bbs.menu_exists("xtrn" + seccode + "_tail")
			|| bbs.menu_exists("xtrn_tail");

		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();

		var i = select_from(prog_list, curprog, {
			fmt: options.singlecolumn_fmt,
			multicolumn_fmt: options.multicolumn_fmt,
			align: options.align_prog_list,
			has_menu: menu !== null,
			menu: function() {
				if(menu === null)
					return false;
				show_head();
				bbs.menu(menu);
				show_tail();
				return true;
			},
			header: function(margin, indent, multicolumn) {
				if(!show_head())
					write(margin, format(options.header_fmt, xtrn_area.sec_list[xsec].name));
				if(options.titles.trimRight() != '')
					write(margin, indent, options.titles);
				if(multicolumn) {
					write(options.multicolumn_separator);
					if (options.titles.trimRight() != '')
						write(indent, options.titles);
				}
				if(options.underline.trimRight() != '') {
					console.crlf();
					write(margin, indent, options.underline);
				}
				if(multicolumn) {
					write(options.multicolumn_separator);
					if (options.underline.trimRight() != '')
						write(indent, options.underline);
				}
				console.crlf();
			},
			footer: has_tail ? show_tail : undefined,
			which: function() { return options.which; }
		});
		if(i < 0)	// Quit, or Enter with no default program
			break;
		curprog = i;

		bbs.exec_xtrn(prog_list[i].code);
	}
}

function external_section_menu()
{
    var xsec=0;

    while(bbs.online) {

		console.aborted = false;
	    if(user.security.restrictions&UREST_XTRN) {
		    write(options.restricted_user_msg);
		    break;
	    }

	    if(!xtrn_area.sec_list.length) {
		    write(options.no_programs_msg);
		    break;
	    }

	    var sec_list=xtrn_area.sec_list.slice();    /* sec_list is a possibly-sorted copy of xtrn_area.sec_list */

		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();

		var has_menu = bbs.menu_exists("xtrn_sec");
		if(options.sort && !has_menu)
			sec_list.sort(sort_by_name);

		var num = select_from(sec_list, xsec, {
			fmt: options.section_fmt,
			multicolumn_fmt: options.multicolumn_section_fmt,
			align: options.align_section_list,
			has_menu: has_menu,
			menu: function() {
				if(!has_menu)
					return false;
				bbs.menu("xtrn_sec_head", P_NOERROR);
				bbs.menu("xtrn_sec");
				bbs.menu("xtrn_sec_tail", P_NOERROR);
				return true;
			},
			header: function(margin) {
				if(!bbs.menu("xtrn_sec_head", P_NOERROR))
					printf(margin + options.section_header_fmt.replace('\x01l', ''), options.section_header_title);
			},
			footer: bbs.menu_exists("xtrn_sec_tail")
				? function() { bbs.menu("xtrn_sec_tail", P_NOERROR); } : undefined,
			which: function(num) { return format(options.section_which, num); }
		});
		if(num == -1)	// Quit
			break;
		if(num == -2) {	// Enter, with no marked default
			if(has_menu)	// Enter = Quit, when there's no prompt
				break;
		} else
			xsec = num;

		external_program_menu(sec_list[xsec].index);
    }
}

/* main: */
if(options.enter_sound)
	console.play_sound(options.enter_sound);
if(options.enter_music)
	console.play_music(options.enter_music);
if(options.eval_first)
	eval(options.eval_first);

if(xsec >= 0)
    external_program_menu(xsec);
else {
    if(xtrn_area.sec_list.length == 1)
        external_program_menu(0);
    else
        external_section_menu();
}
