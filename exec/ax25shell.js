// ax25shell.js
// echicken -at- bbs.electronicchicken.com
// A simple command shell for packet radio users.

load("sbbsdefs.js");

// Colours, in CTRL-A format
var MENU_SECTION = "\1h\1w";
var MENU_BRACKET = "\1h\1c";
var MENU_COLDKEY = "\1h\1w";
var MENU_SUFFIX = "\1n\1c";
var PROMPT_COLOR = "\1h\1w";
var PROMPT_KEY = "\1h\1c";

var attempts = 5;
var userSettings;
var userEditor;

// This has some limitations that are probably a non-issue except on systems
// with excessive numbers of message areas.  Should be fixed at some point.
var messageAreaSelector = function() {
	var out = PROMPT_COLOR + "Choose a message group:\r\n\r\n";
	var promptKeys = [];
	// I will probably rework the prompt() and menuItem() stuff at some point,
	// since there's every reason *not* to limit menu commands to one character
	var theKeys = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!?@#$%&*+=-^./\[]{}<>;:',`~_";
	for(var g = 0; g < msg_area.grp_list.length; g++) {
		out += menuItem(theKeys[g] + " " + msg_area.grp_list[g].name + "\r\n");
		promptKeys.push(theKeys[g]);
	}
	console.putmsg(out + "\r\n");
	var userInput = prompt("Message group:", promptKeys);
	if(typeof userInput == "undefined" || userInput == "")
		return;
	bbs.curgrp = theKeys.indexOf(userInput);
	out = PROMPT_COLOR + "\r\nChoose a message area:\r\n\r\n";
	promptKeys = [];
	for(var s = 0; s < msg_area.grp_list[userInput].sub_list.length; s++) {
		out += menuItem(theKeys[s] + " " + msg_area.grp_list[theKeys.indexOf(userInput)].sub_list[s].name + "\r\n");
		promptKeys.push(theKeys[s]);
	}
	console.putmsg(out + "\r\n");
	userInput = prompt("Message area:", promptKeys);
	if(typeof userInput == "undefined" || userInput == "")
		return;
	bbs.cursub = theKeys.indexOf(userInput);
	return;
}

var mainMenu = [
	{	'section' : 'Messages',
		'items' : [
			{	'description' : "New message scan",
				'action' : bbs.scan_subs
			},
			{	'description' : "Read messages",
				'action' : bbs.scan_msgs
			},
			{	'description' : "Post a message",
				'action' : bbs.post_msg
			},
			{	'description' : "Area change",
				'action' : messageAreaSelector
			},
			{	'description' : "Check email",
				'action' : bbs.read_mail
			},
			{	'description' : "Write email",
				'action' : bbs.email
			},
		]
	},
	{	'section' : 'System',
		'items' : [
			{	'description' : "Your settings",
				'action' : bbs.user_config
			
			},
			{	'description' : "Goodbye",
				'action' : false
			}
		]
	}
];

var init = function() {
	bbs.replace_text(559, "@SETSTR:Y@@QUESTION@ [Y,N] ");
	bbs.replace_text(562, "@SETSTR:N@@QUESTION@ [N,Y] ");
	console.putmsg("Welcome, " + user.alias + "!\r\n");
	userSettings = user.settings;
	userEditor = user.editor;
	getPreferences();
	user.editor = "";
}

var prompt = function(promptString, keys) {
	var attr = console.attributes;
	console.putmsg(
		format(
			"%s%s [%s%s%s]\r\n",
			PROMPT_COLOR, promptString, PROMPT_KEY, keys.join(), PROMPT_COLOR
		)
	);
	console.attributes = attr;
	var userInput = "";
	var tries = 0;
	while(keys.indexOf(userInput[0]) < 0 && tries < attempts) {
		userInput = console.getstr("", console.screen_columns, K_UPPER|K_NOECHO).replace(/\[\d*\;\d*\w/g, "");
		console.putmsg("\r");
		tries++;
	}
	return userInput;
}

var getPreferences = function() {
	console.status|=(0<<0); // Unset the CON_R_ECHO bit
	console.status|=CON_RAW_IN;
	var fileName = format("%suser/%04u.ax25.ini", system.data_dir, user.number);
	var f = new File(fileName);
	if(!f.exists || (f.exists && prompt("Change your terminal settings?", ["Y","N"]) == "Y")) {
		var us = USER_COLDKEYS|USER_NOPAUSESPIN;
		if(prompt("Do you have a color terminal?", ["Y","N"]) == "Y")
			us|=USER_COLOR|USER_ANSI;
		if(prompt("Does your terminal support IBM extended ASCII?", ["Y","N"]) == "N")
			us|=USER_NO_EXASCII;
		if(prompt("Pause after each screenful of data?", ["Y","N"]) == "Y")
			us|=USER_PAUSE;
		if(prompt("Clear the screen before new messages?", ["Y","N"]) == "Y")
			us|=USER_CLRSCRN;
		f.open((f.exists) ? "r+" : "w+");
		f.iniSetValue(null, "userSettings", us);
		f.close();
		console.putmsg(
			"Visit us on the internet at " + system.inet_addr + "!\r\n"
			+ "A password will be required. You can set it via the\r\n"
			+ "(Y)our settings option on the main menu.\r\n"
		);
	} else {
		f.open("r");
		var us = f.iniGetValue(null, "userSettings", "number");
		f.close();
	}
	user.settings = us;
}

var menuItem = function(str) {
	var coldkey = str[0];
	var suffix = str.substr(1);
	str = format(
		"%s(%s%s%s)%s%s",
		MENU_BRACKET, MENU_COLDKEY, coldkey, MENU_BRACKET, MENU_SUFFIX, suffix
	);
	return str;
}

var menu = function() {
	while(!js.terminated) {
		bbs.menu("head");
		var out = "";
		var promptKeys = [];
		var actions = {};
		for(var s = 0; s < mainMenu.length; s++) {
			out += MENU_SECTION + mainMenu[s].section + "\r\n\r\n";
			var lineLength = 0;
			for(var i = 0; i < mainMenu[s].items.length; i++) {
				lineLength += mainMenu[s].items[i].description.length + 1;
				if(lineLength > 80) {
					out += "\r\n";
					lineLength = mainMenu[s].items[i].description.length + 1;
				}
				out += menuItem(mainMenu[s].items[i].description) + " ";
				promptKeys.push(mainMenu[s].items[i].description[0]);
				actions[mainMenu[s].items[i].description[0]] = mainMenu[s].items[i].action;
			}
			out += "\r\n\r\n";
		}
		console.putmsg(out);
		var userInput = prompt("Your choice:", promptKeys);
		if(typeof userInput == "undefined" || userInput == "")
			break;
		if(!actions[userInput])
			break;
		actions[userInput]();
	}
}

var main = function() {
	menu();
}

var cleanUp = function() {
	user.settings = userSettings;
	user.editor = userEditor;
}

js.branch_limit = 0;
js.on_exit("cleanUp()");

init();
main();