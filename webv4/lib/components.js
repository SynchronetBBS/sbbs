// Load with: var components = require({}, settings.web_lib + 'components.js', 'components');
require('xjs.js', 'xjs_compile');

var components = {
    load: function loadComponent(fn, args) {
        const cdir = backslash(fullpath(settings.web_mods + 'components'));
        var sf;
        if (file_isdir(cdir) && file_exists(cdir + fn)) {
            sf = xjs_compile(cdir + fn);
        } else if (file_exists(settings.web_components + fn)) {
            sf = xjs_compile(settings.web_components + fn);
        } else {
            return false;
        }
        js.exec.apply(null, [].concat(sf, new function () {}, args));
        return true;

	}
}

components;