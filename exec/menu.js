// menu.js

// $Id: menu.js,v 1.5 2005/11/19 05:46:44 rswindell Exp $

// Execute a Synchronet menu (.menu) file

load("sbbsdefs.js");
load("menulib.js");

var menu_stack = new Array();
var pop_count = 0;

exec_menu(argv[0]);

function isdigit(ch)
{
	var val = ascii(ch.toUpperCase());

	return(val>=ascii('0') && val<=ascii('9'));
}

function exec_menu(fname)
{
	if(!fname) {
		alert(log(LOG_WARNING,"!No menu file specified"));
		return(false);
	}

	if(!file_getext(fname))
		fname += ".menu";

	// Is menu already running (on stack)? If so, pop contexts to return to it
	for(var i in menu_stack)
		if(menu_stack[i].toUpperCase()==fname.toUpperCase()) {
			pop_count=menu_stack.length-(i+1);
			return;
		}

	log("Reading menu file" + fname);
	var menu;
	if((menu=read_menu(fname))==undefined)
		return(false);

	menu_stack.push(fname);

	var done = false;

	while(bbs.online && !done) {
		if(pop_count) {
			pop_count--;
			break;
		}
		var cmd_keys="";
		var cmd_list=new Array();
		if(menu.hotkey_ext)
			cmd_keys+=menu.hotkey_ext;
		for(i in menu.command) {
			if(menu.command[i].ars && !user.compare_ars(menu.command[i].ars))
				continue;
			cmd_keys+=menu.command[i].key;
			cmd_list.push(menu.command[i]);
		}
		log("menu.show: " + menu.show);
		
		if(menu.show || !menu.expert || !(user.settings&USER_EXPERT)) {
			if(menu.file)	/* static menu */
				bbs.menu(menu.file);
			else {			/* dynamic menu */
				var column=0;
				for(var i in cmd_list) {
					if(cmd_list[i].description) {
						if(column+menu.colwidth>=console.screen_columns)
							console.crlf(), column=0;
						var str=format(menu.fmt
							,menu.reverse ? cmd_list[i].description : cmd_list[i].key.toUpperCase()
							,menu.reverse ? cmd_list[i].key.toUpperCase() : cmd_list[i].description);
						console.putmsg(str);
						console.right(menu.colwidth-console.strlen(str));
						if(console.strlen(str)>menu.colwidth)
							column+=console.strlen(str);
						else
							column+=menu.colwidth;
					}
				}
			}
			menu.show = false;
		}
		bbs.nodesync();
		console.aborted=false;
		if(menu.exec)
			eval(menu.exec);

		if(menu.prompt)
			console.putmsg(eval(menu.prompt));

		var cmd;
		if(menu.hotkeys)
			console.write(cmd=console.getkey(K_UPPER));
		else
			cmd=console.getstr(K_UPPER);

		if(cmd==undefined)
			continue;

		log("menu.num_exec = " + menu.num_exec);
		log("menu.maxnum = " + menu.maxnum);
		if(isdigit(cmd) && menu.num_exec) {
			if(parseInt(cmd)*10 <= menu.maxnum)
				eval(menu.num_exec);
			continue;
		}

		if(menu.hotkeys && cmd.toUpperCase()==menu.hotkey_ext)
			cmd+=console.getkey();

		print();

		for(var i in cmd_list)
			if(cmd_list[i].key.toUpperCase() == String(cmd).toUpperCase())
				eval(cmd_list[i].exec);
	}

	menu_stack.pop();
}

log("exiting");