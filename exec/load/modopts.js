/* Load Synchronet JS Module simple control/enable/string options from
 * ctrl/modopts.ini (and by extension ctrl/modopts.d/*.ini) or
 * ctrl/modopts/modname.ini
 *
 * For ctrl/modopts.ini (and ctrl/modopts.d/*.ini): 
 *   Parses a single .ini section using the first argument to load() as the
 *   section name and return an object containing the section keys as
 *   properties with Boolean, Number, or String values.
 *
 *   Or parses and return a single option/key value when a key name is
 *   specified in the fourth argument to load(). The default value of the
 *   option may be specified in the fifth argument to load() in this case.
 *
 *   If the section [module:lang=user-lang] exists in the modopts.ini file,
 *   that section (or key value from it) may be returned instead of the
 *   [module] section.
 *
 *   If the section [module:charset=terminal-charset] exists in the
 *   modopts.ini file, that section (or key value from it) may be returned
 *   instead of the [module] section.
 *
 * For ctrl/modopts/modname.ini:
 *   Parses the entire root section of the modname.ini file and for each
 *   [section] in the .ini file, treat the section name as an Access
 *   Requirement String (ARS) and if the current user meets the requirements,
 *   parse the keys from that section and add the keys/values as properties to
 *   the returned object. Later sections/keys can override previous property
 *   values.
 *
 *   If an option/key name is passed to load(), returns just that one option
 *   value (String, Number, or Boolean, not an object).
 *
 * Examples:
 *
 * var options = load({}, "modopts.js", "your_module_name");
 *
 * var opt = load({}, "modopts.js", "your_module_name", "opt_name", default_opt_value);
 */

"use strict";
function get_mod_options(modname, optname, default_optval)
{
	if(typeof modname !== "string")
		throw new Error("Module name specified is wrong type: " + typeof modname);
	if(!modname)
		throw new Error("No module name specified");
	var ini_file = new File(system.ctrl_dir + "modopts/" + modname.toLowerCase().replace(":", "-").replace(" ", "_") + ".ini");
	if(ini_file.open("r")) {
		var obj = ini_file.iniGetObject(/* lowercase */false, /* blanks: */true);
		var sections = ini_file.iniGetSections();
		for(var i in sections) {
			var section = sections[i];
			if((js.global.bbs && bbs.compare_ars(section)) || (js.global.user && user.compare_ars(section))) {
				var subobj = ini_file.iniGetObject(section, /* lowercase */false, /* blanks: */true);
				for(var j in subobj)
					obj[j] = subobj[j];
			}
		}
		ini_file.close();
		if(optname)
			return (obj[optname] === undefined) ? default_optval : obj[optname];
		return obj;
	}
	ini_file = new File(file_cfgname(system.ctrl_dir, "modopts.ini"));
	if(!ini_file.open("r")) {
		return undefined;
	}
	var section;
	if(js.global.console || user.lang) {
		var sections = ini_file.iniGetSections();
		if(js.global.console)
			section = modname + ':charset=' + console.charset.toLowerCase();
		if(user.lang) {
			if(section === undefined || sections.indexOf(section) < 0)
				section = modname + ':lang=' + user.lang;
		}
		if(section && sections.indexOf(section) < 0)
			section = undefined;
	}
	if(!section)
		section = modname;
	var val;
	if(optname)	// Get a specific option value (optionally, with default value)
		val = ini_file.iniGetValue(section, optname, default_optval);
	else // Get all options
		val = ini_file.iniGetObject(section, /* lowercase */false, /* blanks: */true);
	ini_file.close();
	return val;
}

if(argv.length)
	get_mod_options.apply(null, argv);
