
require('sbbsdefs.js', 'SYS_CLOSED'); // Is this actually used?
require('nodedefs.js', 'NODE_WFC'); // Is this actually used?
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
var sbbsimsg = load({}, "sbbsimsg_lib.js");

var reply = {};

if (user.number > 0 && user.alias != settings.guest
    && typeof http_request.query.call != 'undefined'
    && http_request.query.call[0] == 'send_telegram'
    && typeof http_request.query.host != 'undefined'
    && typeof http_request.query.username != 'undefined'
    && typeof http_request.query.message != 'undefined'
) {
    sbbsimsg.send_msg(
        http_request.query.username[0] + '@' + http_request.query.host[0],
        http_request.query.message[0],
        user.alias
    );
}

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);
