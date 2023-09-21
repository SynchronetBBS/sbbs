require('sbbsdefs.js', 'SYS_CLOSED');
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
var request = require({}, settings.web_lib + 'request.js', 'request');

var reply = { errors: [], skipped: [], updated: [] };

function validateRequest() {
    if (!user.is_sysop) {
        reply.errors.push({ key: '', message: 'You must be a SysOp to use this API' });
        return false;
    } else if (!validateCsrfToken()) {
        reply.errors.push({ key: '', message: 'Invalid or missing CSRF token' });
        return false;
    }

    return true;
}

if (validateRequest()) {
    // Update terminal server settings
    var f = new File(system.ctrl_dir + 'sbbs.ini');
    if (f.open("r+")) {
        try {
            updateIntValue(f, 'FirstNode', 1, 255);
            updateIntValue(f, 'LastNode', 1, 255);
            updateIntValue(f, 'MaxConcurrentConnections', 0, 255);
            updateBoolValue(f, 'AutoStart');
            updateOptions(f, 'XTRN_MINIMIZED');
            updateOptions(f, 'NO_EVENTS');
            updateOptions(f, 'NO_QWK_EVENTS');
            updateOptions(f, 'NO_DOS');
            updateOptions(f, 'NO_HOST_LOOKUP');
        } catch (error) {
            reply.errors.push({ key: '', message: error.message });
        } finally {
            f.close();
        }
    } else {
        reply.errors.push({ key: '', message: 'Unable to open sbbs.ini for writing' });
    }
}

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);



function updateBoolValue(f, key) {
    // Check if parameter was sent in request
    if (!request.has_param(key)) {
        reply.errors.push({ key: key, message: 'Missing parameter' });
        return;
    }

    // Check if parameter has a valid value
    var re = /^(true|false)$/i;
    if (!re.test(request.get_param(key))) {
        reply.errors.push({ key: key, value: request.get_param(key), message: 'Value is not boolean' });
        return;
    }

    // Skip if new value matches existing value
    var newValue = JSON.parse(request.get_param(key).toLowerCase());
    if (newValue == f.iniGetValue('BBS', key, false)) {
        reply.skipped.push({ key: key, value: request.get_param(key) });
        return;
    }

    // Update the ini
    f.iniSetValue('BBS', key, newValue);
    reply.updated.push({ key: key, value: newValue });
}

function updateIntValue(f, key, min, max) {
    // Check if parameter was sent in request
    if (!request.has_param(key)) {
        reply.errors.push({ key: key, message: 'Missing parameter' });
        return;
    }

    // Check if parameter has a valid value
    var re = /^\d+$/;
    if (!re.test(request.get_param(key))) {
        reply.errors.push({ key: key, value: request.get_param(key), message: 'Value is not numeric' });
        return;
    }

    // Check if value is in allowed range
    var newValue = parseInt(request.get_param(key), 10);
    if ((newValue < min) || (newValue > max)) {
        reply.errors.push({ key: key, value: request.get_param(key), message: 'Value is not in range (' + min + '-' + max + ')' });
        return;
    }
     
    // Skip if new value matches existing value
    if (newValue == f.iniGetValue('BBS', key, 0)) {
        reply.skipped.push({ key: key, value: request.get_param(key) });
        return;
    }

    // Update the ini
    f.iniSetValue('BBS', key, newValue);
    reply.updated.push({ key: key, value: newValue });
}

function updateOptions(f, option) {
    // Check if parameter was sent in request
    if (!request.has_param(option)) {
        reply.errors.push({ key: option, message: 'Missing parameter' });
        return;
    }

    // Check if option has a valid name
    var re = /^[A-Z0-9_]+$/;
    if (!re.test(option)) {
        reply.errors.push({ key: option, value: request.get_param(option), message: 'Option is not valid' });
        return;
    }

    // Check if parameter has a valid value
    var re = /^(true|false)$/i;
    if (!re.test(request.get_param(option))) {
        reply.errors.push({ key: option, value: request.get_param(option), message: 'Value is not boolean' });
        return;
    }

    // Read the existing options
    var options = f.iniGetValue('BBS', 'Options', '');
    if (options) {
        options = options.split(/\s*[|]\s*/);
    } else {
        options = [];
    }

    // Skip if new value matches existing value
    var enableOption = JSON.parse(request.get_param(option).toLowerCase());
    if (enableOption == options.includes(option)) {
        reply.skipped.push({ key: option, value: request.get_param(option) });
        return;
    }

    // Add or remove option based on value
    if (enableOption) {
        options.push(option);
    } else {
        options.splice(options.indexOf(option), 1);
    }

    // Update the ini
    f.iniSetValue('BBS', 'Options', options.join(' | '));
    reply.updated.push({ key: option, value: enableOption });
}