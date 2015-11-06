load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + 'auth.js');

var response = JSON.stringify(
	{	'authenticated' : (user.alias !== settings.guest),
		'user' : (user.alias === settings.guest ? 0 : user.number)
	}
);

http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = response.length;

write(response);