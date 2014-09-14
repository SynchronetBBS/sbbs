load("sbbsdefs.js");
load("menu-commands.js");

var menu;
for(var m in Commands.Menus) {
	if(!Commands.Menus[m].Default)
		continue;
	menu = m;
	break;
}

var putMsg = function(str) {
	str = str.replace(/\\1/g, ascii(1));
	console.putmsg(str);
}

var doMenu = function() {

	console.clear(LIGHTGRAY);

	if(Commands.Menus[menu].Header != "")
		bbs.menu(Commands.Menus[menu].Header.replace(/\..*$/, ""));
	else
		putMsg(Commands.Menus[menu].Title);
	console.crlf();

	var keys = [];
	for(var command in Commands.Menus[menu].Commands) {
		if(!user.compare_ars(Commands.Menus[menu].Commands[command].ARS)) {
			keys.push(null);
			continue;
		}
		keys.push(Commands.Menus[menu].Commands[command].hotkey);
		if(!Commands.Menus[menu].List)
			continue;
		putMsg(Commands.Menus[menu].Commands[command].text);
		console.crlf();
	}
	console.crlf();

	putMsg(Commands.Menus[menu].Prompt);
	var userInput = console.getkeys(keys.join(""));
	if(typeof userInput == "undefined" || userInput == "")
		return;
	var command = Commands.Menus[menu].Commands[keys.indexOf(userInput)];

	if(typeof command.menu != "undefined") {
		menu = command.menu;
	} else {
		var path = command.command.split(".");
		if(path[1] == "Externals")
			bbs.exec_xtrn(path[2]);
		else
			Commands[path[1]][path[2]].Action();
	}

}

var main = function() {
	while(bbs.online) {
		getMenus();
		doMenu();
	}
}

try {
	main();
} catch(err) {
	log(LOG_ERR, err);
}
bbs.hangup();
exit();
