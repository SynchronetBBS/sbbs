load("menulib.js");
load("uifcdefs.js");

main(argc, argv);

function edit_menu(fname)
{
	var menu = read_menu(fname);
	if(!menu)
		return menu;
	var options=[];
	for(var i in menu)
		options.push(format("%-15s ", i) + menu[i]);
	uifc.list(WIN_MID,"Options",options);
}

function main(argc, argv)
{
	uifc.init("Synchronet Menu Editor");
	js.on_exit("uifc.bail()");

	if(argc)
		return edit_menu(argv[0]);
		
	while(1) {
		var menu_list = directory(system.exec_dir + "*.menu");
		var selection = uifc.list(WIN_ORG|WIN_MID|WIN_INS|WIN_XTR,"Menu Files",menu_list);
		if(selection<0)
			break;
		edit_menu(file_getname(menu_list[selection]));
	}
}
