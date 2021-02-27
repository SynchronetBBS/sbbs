/* modopts.js */

/* $Id: modopts.js,v 1.4 2019/01/11 09:26:34 rswindell Exp $ */

/* Load Synchronet JS Module Control/Enable options from ctrl/modopts.ini */
/* Parse a single .ini section using the argument (to load) as the section name */
/* and return an object containing the key=value pairs as properties */

/* To avoid over-writing the parent script's "argc" and "argv" values, 
   pass a scope object to load(), like this:

   options=load(new Object, "modopts.js", "your_module_name");
*/

// Another valid use is to request the value of a single module option, like this:
//		opt_value = load({}, "modopts.js", "your_module", "opt_name");
// or:
//		opt_value = load({}, "modopts.js", "your_module", "opt_name", default_opt_value);

"use strict";
function get_mod_options(modname, optname, default_optval)
{
	var ini_file = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
	if(!ini_file.open("r")) {
		return undefined;
	}

	var val;
	if(optname)	// Get a specific option value (optionally, with default value)
		val = ini_file.iniGetValue(modname, optname, default_optval);
	else // Get all options
		val = ini_file.iniGetObject(modname);
	ini_file.close();
	return val;
}

get_mod_options(argv[0], argv[1], argv[2]);