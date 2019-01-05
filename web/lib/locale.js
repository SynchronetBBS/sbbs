if (!settings.locale) {
    load(backslash(settings.web_lib + 'locale/') + 'en_us.js');
} else {
    load(backslash(settings.web_lib + 'locale/') + settings.locale + '.js');
}

var locale = new Locale();
