// $Id: menulib.js,v 1.1 2005/11/19 03:25:41 rswindell Exp $

function read_menu(fname)
{
	var file = new File(system.exec_dir + fname);
	if(!file.open("r")) {
		alert(log(LOG_ERR,"!ERROR " + file.error + " opening " + fname));
		return(undefined);
	}

	var menu = { };

	menu.prompt		= file.iniGetValue(null,"prompt","'Command: '");
	menu.file		= file.iniGetValue(null,"menu_file");
	menu.fmt		= file.iniGetValue(null,"menu_format", "\1n\1h\1w%s \1b%s");
	menu.colwidth	= file.iniGetValue(null,"menu_column_width", 39);
	menu.reverse	= file.iniGetValue(null,"menu_reverse", false);

	menu.hotkeys	= file.iniGetValue(null,"hotkeys",true);
	menu.hotkey_ext = file.iniGetValue(null,"hotkey_ext");

	menu.maxnum		= file.iniGetValue(null,"maxnum",0);
	menu.num_exec	= file.iniGetValue(null,"num_exec");

	menu.expert		= file.iniGetValue(null,"expert",true);
	menu.show		= file.iniGetValue(null,"show_menu",false);

	menu.exec		= file.iniGetValue(null,"exec");

	// Commands
	menu.command	= file.iniGetAllObjects("key");

	file.close();

	return(menu);
}