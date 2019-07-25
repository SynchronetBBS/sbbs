var settings = load('modopts.js', 'web');

require(settings.web_directory + '/lib/init.js', 'WEBV4_INIT');
require(settings.web_lib + 'auth.js', 'WEBV4_AUTH');

var response = JSON.stringify({
	authenticated: (user.alias !== settings.guest)
});

http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = response.length;

write(response);
