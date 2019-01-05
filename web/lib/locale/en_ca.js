load(backslash(settings.web_lib + 'locale/') + 'en_us.js');

function EN_CA() {
    EN_US.call(this, 'en_ca');
}
EN_CA.prototype = Object.create(EN_US.prototype);
EN_CA.prototype.constructor = EN_US;

var Locale = EN_CA;
