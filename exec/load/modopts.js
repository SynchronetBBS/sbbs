/* modopts.js */

/* $Id$ */

/* Load Synchronet JS Module Control/Enable options from ctrl/modopts.ini */
/* Parse a single .ini section using the argument (to load) as the section name */
/* and return an object containing the key=value pairs as properties */


function get_mod_options(modname)
{
	var ini_file = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
	if(!ini_file.open("r")) {
		delete ini_file;
		return undefined;
	}

	var obj = ini_file.iniGetObject(modname);
	delete ini_file;
	return obj;
}

return get_mod_options(argv[0]);