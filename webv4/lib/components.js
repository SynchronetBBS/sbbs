// Load with: var components = require({}, settings.web_lib + 'components.js', 'components');
require('xjs.js', 'xjs_compile');

var components = {
    load: function loadComponent(fn, scope) {
        const cdir = backslash(fullpath(settings.web_mods + 'components'));
        if (file_isdir(cdir) && file_exists(cdir + fn)) {
            js.exec(xjs_compile(cdir + fn), scope || new function () {});
            return true;
        }
        if (file_exists(settings.web_components + fn)) {
            js.exec(xjs_compile(settings.web_components + fn), scope || new function () {});
            return true;
        }
        return false;
	}
}

components;