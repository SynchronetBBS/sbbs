// xtrn_sec.js

// Synchronet External Program Section
// Menus displayed to users via Terminal Server (Telnet/RLogin/SSH)

// To jump straight to a specific xtrn section, pass the section code as an argument

// $Id: xtrn_sec.js,v 1.29 2020/05/09 10:11:23 rswindell Exp $

"use strict";

load("sbbsdefs.js");

/* text.dat entries */
load("text.js");

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

function sort_by_name(a, b)
{
	if(a.name.toLowerCase()>b.name.toLowerCase()) return 1;
	if(a.name.toLowerCase()<b.name.toLowerCase()) return -1;
	return 0;
}

function exec_xtrn(prog)
{
	console.attributes = LIGHTGRAY;
	if(options.clear_screen_on_exec)
		console.clear();
	if(options.eval_before_exec)
		eval(options.eval_before_exec);
	load('fonts.js', 'xtrn:' + prog.code);
	bbs.exec_xtrn(prog.code);
	console.attributes = 0;
	console.attributes = LIGHTGRAY;
	load('fonts.js', 'default');
	if(options.eval_after_exec)
		eval(options.eval_after_exec);

	if(prog.settings&XTRN_PAUSE)
		console.pause();
	else
		console.line_counter=0;
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
			exec_xtrn(prog_list[0]);
			break;
		}
		
		if(options.clear_screen)
			console.clear(LIGHTGRAY);

		var secnum = xtrn_area.sec_list[xsec].number+1
		if(bbs.menu_exists("xtrn" + secnum + "_head")) {
			bbs.menu("xtrn" + secnum + "_head");
		}
		if(bbs.menu_exists("xtrn" + secnum)) {
			bbs.menu("xtrn" + secnum);
		}
		else {
			var multicolumn = options.multicolumn && prog_list.length > options.singlecolumn_height;
			if(options.sort)
				prog_list.sort(sort_by_name);
			printf(options.header_fmt, xtrn_area.sec_list[xsec].name);
			write(options.titles);
			if(multicolumn) {
				write(options.multicolumn_separator);
				write(options.titles);
			}
			console.crlf();
			write(options.underline);
			if(multicolumn) {
				write(options.multicolumn_separator);
				write(options.underline);
			}
			console.crlf();
			var n;
			if(multicolumn)
				n=Math.floor(prog_list.length/2)+(prog_list.length&1);
			else
				n=prog_list.length;

			for(i=0;i<n && !console.aborted;i++) {
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
			bbs.node_sync();
			console.mnemonics(options.which);
		}
		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();
		if((i=console.getnum(prog_list.length))<1)
			break;
		i--;
		if(bbs.menu_exists("xtrn/" + prog_list[i].code)) {
			bbs.menu("xtrn/" + prog_list[i].code);
			console.line_counter=0;
		}
		exec_xtrn(prog_list[i]);
	}
}

function external_section_menu()
{
    var i,j;

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

	    var xsec=0;
	    var sec_list=xtrn_area.sec_list.slice();    /* sec_list is a possibly-sorted copy of xtrn_area.sect_list */

		system.node_list[bbs.node_num-1].aux=0; /* aux is 0, only if at menu */
		bbs.node_action=NODE_XTRN;
		bbs.node_sync();

		if(bbs.menu_exists("xtrn_sec")) {
			bbs.menu("xtrn_sec");
			xsec=console.getnum(sec_list.length);
			if(xsec<=0)
				break;
			xsec--;
		}
		else {

			if(options.sort)
				sec_list.sort(sort_by_name);
			for(i in sec_list)
				console.uselect(Number(i),"External Program Section"
					,sec_list[i].name);
			xsec=console.uselect();
		}
	    if(xsec<0)
		    break;

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
