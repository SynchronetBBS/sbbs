require('sbbsdefs.js', 'SYS_CLOSED');
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');

var reply = {};

function validateRequest() {
    if (!user.is_sysop) {
        reply = { error: 'You must be a SysOp to use this API' }
        return false;
    } else if (!validateCsrfToken()) {
        reply = { error: 'Invalid or missing CSRF token' }
        return false;
    }

    return true;
}

if (validateRequest()) {
    // Read terminal server settings
    var f = new File(system.ctrl_dir + 'sbbs.ini');
    if (f.open("r")) {
        try {
            reply = f.iniGetObject('BBS');
        } catch (error) {
            reply = { error: error.message };
        } finally {
            f.close();
        }
    } else {
        reply = { error: 'Unable to open sbbs.ini for reading' }
    }
}

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);
