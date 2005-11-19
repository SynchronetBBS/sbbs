load("menulib.js");
load("uifcdefs.js");

main(argc, argv);

function main(argc, argv)
{
	uifc.init("Synchronet Menu Editor", "ansi");
	js.on_exit("uifc.bail()");

	while(1) {
		var menu_list = directory(system.exec_dir + "*.menu");
		var selection = uifc.list(WIN_MID|WIN_INS|WIN_XTR,"Menu Files",menu_list);
		if(selection<0)
			break;
		var menu = read_menu(file_getname(menu_list[selection]));
		if(!menu)
			continue;
		var options=[];
		for(var i in menu)
			options.push(format("%-25s ", i) + menu[i]);
		uifc.list(WIN_MID,"Options",options);
	}
}
