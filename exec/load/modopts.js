/* modopts.js */

/* $Id$ */

/* Load Synchronet JS Module Control/Enable options from ctrl/modopts.ini */
/* Parse a single .ini section using the argument (to load) as the section name */
/* and return an object containing the key=value pairs as properties */

/* To avoid over-writing the parent script's "argc" and "argv" values, 
   pass a scope object to load(), like this:

   options=load(new Object, "modopts.js", "your_module_name");
*/


function get_mod_options(modname)
{
	var ini_file = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
	if(!ini_file.open("r")) {
		delete ini_file;
		return undefined;
	}

	var obj = ini_file.iniGetObject(modname);
	ini_file.close();
	delete ini_file;
	return obj;
}

get_mod_options(argv[0]);