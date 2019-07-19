function EN_US(name) {

    const ini_path = backslash(settings.web_lib + 'locale/') + 'en_us.ini';
    if (typeof name == 'string') {
        const mod_path = backslash(settings.web_lib + 'locale/') + name + '.ini';
    }

    var f = new File(ini_path);
    f.open('r');
    var sections = f.iniGetSections();
    f.close();
    f = undefined;

    const _strings = {};
    const strings = {};
    sections.forEach(function (e) {
        Object.defineProperty(strings, e, {
            enumerable: true,
            get: function () {
                if (_strings[e]) return _strings[e];
                var f = new File(ini_path);
                f.open('r');
                const o = f.iniGetObject(e);
                f.close();
                if (mod_path) {
                    f = new File(mod_path);
                    f.open('r');
                    const oo = f.iniGetObject(e);
                    f.close();
                    if (oo !== null) {
                        Object.keys(oo).forEach(function (ee) {
                            o[ee] = oo[ee];
                        });
                    }
                }
                f = undefined;
                _strings[e] = o;
                return o;
            }
        });
    });
    Object.defineProperty(this, 'strings', { value: strings });

    var active_section = 'page_main';
    Object.defineProperty(this, 'section', {
        get: function () {
            return active_section;
        },
        set: function (s) {
            active_section = s;
        }
    });

}

EN_US.prototype.group_numbers = function (n) {
    n = n + '';
    const d = n.indexOf('.');
    return n.substring(0, d > -1 ? d : n.length).split('').reverse().reduce(
        function (a, c, i) {
            return a + (i > 0 && !(i % 3) ? ',' + c : c);
        }, ''
    ).split('').reverse().join('') + (d > -1 ? n.substring(d) : '');
}

EN_US.prototype.write = function (str, sec) {
    write(this.strings[sec || this.section][str]);
}

var Locale = EN_US;
