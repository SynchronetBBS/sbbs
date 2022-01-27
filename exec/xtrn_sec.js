// xtrn_sec.js

// Synchronet External Program Section
// Menus displayed to users via Terminal Server (Telnet/RLogin/SSH)

// To jump straight to a specific xtrn section, pass the section code as an argument

// $Id: xtrn_sec.js,v 1.29 2020/05/09 10:11:23 rswindell Exp $

"use strict";

require("sbbsdefs.js", "P_NOERROR");

/* text.dat entries */
require("text.js", "XtrnProgLstFmt");

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
if((options=load({}, "modopts.js","xtrn_sec")) == null)
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

if(console.screen_columns < 80)
	options.multicolumn = false;

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

function external_program_menu(xsec)
{
    var i,j;

	while(bbs.online) {

		console.aborted = false;
		if(user.security.restrictions&UFLAG_X) {
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
		
		if(options.clear_screen)
			console.clear(LIGHTGRAY);

		var show_header = true;
		var secnum = xtrn_area.sec_list[xsec].number+1;
		var seccode = xtrn_area.sec_list[xsec].code;

		if (bbs.menu("xtrn" + secnum + "_head", P_NOERROR)) {
			show_header = false;
		} else if (bbs.menu("xtrn" + seccode + "_head", P_NOERROR)) {
			show_header = false;
		} else {
			bbs.menu("xtrn_head", P_NOERROR);
		}

		if(bbs.menu("xtrn" + secnum, P_NOERROR) || bbs.menu("xtrn" + seccode, P_NOERROR)) {
			if(!bbs.menu("xtrn" + secnum + "_tail", P_NOERROR) &&
				!bbs.menu("xtrn" + seccode + "_tail", P_NOERROR)) {
				bbs.menu("xtrn_tail", P_NOERROR);
			}
		}
		else {
			var multicolumn = options.multicolumn && prog_list.length > options.singlecolumn_height;
			var center = options.center && !multicolumn;
			var margin = center ? format("%*s", (console.screen_columns * 0.25) - 1, "") : "";
			if(options.sort)
				prog_list.sort(sort_by_name);
			if(show_header)
				write(margin, format(options.header_fmt, xtrn_area.sec_list[xsec].name));
			if(options.titles.trimRight() != '')
				write(margin, options.titles);
			if(multicolumn) {
				write(options.multicolumn_separator);
				if (options.titles.trimRight() != '')
					write(options.titles);
			}
			if(options.underline.trimRight() != '') {
				console.crlf();
				write(margin, options.underline);
			}
			if(multicolumn) {
				write(options.multicolumn_separator);
				if (options.underline.trimRight() != '')
					write(options.underline);
			}
			console.crlf();
			var n;
			if(multicolumn)
				n=Math.floor(prog_list.length/2)+(prog_list.length&1);
			else
				n=prog_list.length;

			for(i=0;i<n && !console.aborted;i++) {
				write(margin);
				console.add_hotspot(i+1);
				printf(multicolumn ? options.multicolumn_fmt : options.singlecolumn_fmt
					,i+1
					,prog_list[i].name
					,prog_list[i].cost);

				if(multicolumn) {
					j=Math.floor(prog_list.length/2)+i+(prog_list.length&1);
					if(j<prog_list.length) {
						write(options.multicolumn_separator);
						console.add_hotspot(j+1);
						printf(options.multicolumn_fmt, j+1
							,prog_list[j].name
							,prog_list[j].cost);
					}
				}
				console.crlf();
			}
			if(!bbs.menu("xtrn" + secnum + "_tail", P_NOERROR)
				&& !bbs.menu("xtrn" + seccode + "_tail", P_NOERROR)) {
				bbs.menu("xtrn_tail", P_NOERROR);
			}
			bbs.node_sync();
			if(margin) {
				console.crlf();
				write(margin);
				console.mnemonics(options.which.trimLeft());
			}
			else
				console.mnemonics(options.which);
		}
		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();
		if((i=console.getnum(prog_list.length))<1)
			break;
		i--;

		bbs.exec_xtrn(prog_list[i].code);
	}
}

function external_section_menu()
{
    var i,j;
    var xsec=0;
	var longest = 0;
	for(i = 0; i < xtrn_area.sec_list.length; i++)
		longest = Math.max(xtrn_area.sec_list[i].name.length, longest);
	var margin = options.center ? format("%*s", ((console.screen_columns - longest)/2) - 5, "") : "";

    while(bbs.online) {

		console.aborted = false;
	    if(user.security.restrictions&UFLAG_X) {
		    write(options.restricted_user_msg);
		    break;
	    }

	    if(!xtrn_area.sec_list.length) {
		    write(options.no_programs_msg);
		    break;
	    }

	    var sec_list=xtrn_area.sec_list.slice();    /* sec_list is a possibly-sorted copy of xtrn_area.sect_list */

		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();

		if(options.clear_screen)
			console.clear(LIGHTGRAY);

		var show_header = !bbs.menu("xtrn_sec_head", P_NOERROR);

		if(bbs.menu_exists("xtrn_sec")) {
			bbs.menu("xtrn_sec");
			bbs.menu("xtrn_sec_tail", P_NOERROR);
		}
		else {
			if(options.sort)
				sec_list.sort(sort_by_name);

			if(show_header)
				printf(margin + options.section_header_fmt.replace('\x01l', ''), options.section_header_title);
			for (i = 0; i < sec_list.length; i++) {
				console.add_hotspot(i+1);
				printf(margin + options.section_fmt, i + 1, sec_list[i].name);
			}

			bbs.menu("xtrn_sec_tail", P_NOERROR);
			
			bbs.node_sync();
			if(options.center) {
				console.crlf();
				write(margin);
				console.mnemonics(format(options.section_which, xsec + 1).trimLeft());
			}
			else
				console.mnemonics(format(options.section_which, xsec + 1));
		}

		bbs.node_sync();
		var num = console.getnum(sec_list.length);
		if(num < 0)
			break;
		if(num > 0)
			xsec = num - 1;
		
		external_program_menu(sec_list[xsec].index);
    }
}

/* main: */
if(xsec >= 0)
    external_program_menu(xsec);
else {
    if(xtrn_area.sec_list.length == 1)
        external_program_menu(0);
    else
        external_section_menu();
}
