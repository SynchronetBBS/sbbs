// menu.js

// Execute a Synchronet v3.11c menu (.mnu) file

load("sbbsdefs.js");

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
		alert(log("!No menu file specified"));
		return(false);
	}

	if(!file_getext(fname))
		fname += ".mnu";

	// Is menu already running (on stack)? If so, pop contexts to return to it
	for(var i in menu_stack)
		if(menu_stack[i].toUpperCase()==fname.toUpperCase()) {
			pop_count=menu_stack.length-(i+1);
			return;
		}

	var file = new File(system.exec_dir + fname);
	if(!file.open("r")) {
		alert(log("!ERROR " + file.error + " opening " + fname));
		return(false);
	}

	menu_stack.push(fname);

	// Global options
	var section = null;	// "root" section
	var prompt = file.iniGetValue(section,"prompt","'Command: '");
	var menu_file = file.iniGetValue(section,"menu_file");
	var menu_fmt  = file.iniGetValue(section,"menu_format", "\1n\1h\1w%s \1b%s");
	var menu_colwidth = file.iniGetValue(section,"menu_column_width", 39);
	var menu_reverse = file.iniGetValue(section, "menu_reverse", false);

	var hotkeys = file.iniGetValue(section,"hotkeys",true);
	var hotkey_ext = file.iniGetValue(section,"hotkey_ext");

	var maxnum = file.iniGetValue(section,"maxnum",0);
	var num_exec = file.iniGetValue(section,"num_exec");

	var expert = file.iniGetValue(section,"expert",true);
	var show_menu = file.iniGetValue(section,"show_menu",false);

	var exec = file.iniGetValue(section,"exec");

	// Commands
	var command = file.iniGetAllObjects("key");

	file.close();

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
