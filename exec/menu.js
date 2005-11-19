// menu.js

// $Id$

// Execute a Synchronet menu (.menu) file

load("sbbsdefs.js");
load("menulib.js");

var menu_stack = new Array();
var pop_count = 0;

menu(argv[0]);

function isdigit(ch)
{
	var val = ascii(ch.toUpperCase());

	return(val>=ascii('0') && val<=ascii('9'));
}

function menu(fname)
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
		if(hotkey_ext)
			cmd_keys+=hotkey_ext;
		for(i in command) {
			if(command[i].ars && !user.compare_ars(command[i].ars))
				continue;
			cmd_keys+=command[i].key;
			cmd_list.push(command[i]);
		}

		if(show_menu || !expert || !(user.settings&USER_EXPERT)) {
			if(menu_file)	/* static menu */
				bbs.menu(menu_file);
			else {			/* dynamic menu */
				var column=0;
				for(var i in cmd_list) {
					if(cmd_list[i].description) {
						if(column+menu_colwidth>=console.screen_columns)
							console.crlf(), column=0;
						var str=format(menu_fmt
							,menu_reverse ? cmd_list[i].description : cmd_list[i].key.toUpperCase()
							,menu_reverse ? cmd_list[i].key.toUpperCase() : cmd_list[i].description);
						console.putmsg(str);
						console.right(menu_colwidth-console.strlen(str));
						if(console.strlen(str)>menu_colwidth)
							column+=console.strlen(str);
						else
							column+=menu_colwidth;
					}
				}
			}
			show_menu = false;
		}
		bbs.nodesync();
		console.aborted=false;
		if(exec)
			eval(exec);

		if(prompt)
			console.putmsg(eval(prompt));

		var cmd;
		if(hotkeys)
			console.write(cmd=console.getkey(K_UPPER));
		else
			cmd=console.getstr(K_UPPER);

		if(cmd==undefined)
			continue;

		if(isdigit(cmd) && num_exec) {
			if(parseInt(cmd)*10 > maxnum)
				eval(num_exec);
			continue;
		}

		if(hotkeys && cmd.toUpperCase()==hotkey_ext)
			cmd+=console.getkey();

		print();

		for(var i in cmd_list)
			if(cmd_list[i].key.toUpperCase() == String(cmd).toUpperCase())
				eval(cmd_list[i].exec);
	}

	menu_stack.pop();
}
