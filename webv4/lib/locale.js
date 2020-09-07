if (!settings.locale) {
    require(backslash(settings.web_lib + 'locale/') + 'en_us.js', 'Locale');
} else {
    require(backslash(settings.web_lib + 'locale/') + settings.locale + '.js', 'Locale');
}

var locale = new Locale();
