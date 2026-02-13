// $Id: nodelist_options.js,v 1.1 2019/01/29 22:49:08 rswindell Exp $

var options = bbs.mods.nodelist_options;
if(!options) {
    options = load({}, "modopts.js", "nodelist");
    if(!options)
        options = {};
    if(!options.format)
        options.format = "\x01n\x01h%3d  \x01u%s";
    if(!options.username_prefix)
        options.username_prefix = '\x01h';
    if(!options.status_prefix)
        options.status_prefix = '\x01u';
    if(!options.errors_prefix)
        options.errors_prefix = '\x01h\x01r';
    if(!options.web_inactivity)
        options.web_inactivity = load({}, "modopts.js", "web", "inactivity");
    bbs.mods.nodelist_options = options;	// cache the options
}
options;
