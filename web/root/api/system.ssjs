load('sbbsdefs.js');
load('nodedefs.js');
load('modopts.js');
var settings = get_mod_options('web');

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'language.js');

var reply = {};

if ((http_request.method === 'GET' || http_request.method === 'POST') &&
	typeof http_request.query.call !== 'undefined' &&
	user.number > 0
) {

	switch (http_request.query.call[0]) {

		case 'node-list':
            var usr = new User(1);
			reply = system.node_list.reduce(function (a, c, i) {
                if (c.status !== 3) return a;
                usr.number = c.useron;
				a.push({
                    node: i,
                    status: format(NodeStatus[c.status], c.aux, c.extaux),
					action: format(NodeAction[c.action], c.aux, c.extaux),
                    user: usr.alias,
                    connection: usr.connection
				});
                return a;
			}, []);
			for (var un = 1; un < system.lastuser; un++) {
				usr.number = un;
				if (usr.connection !== 'HTTP') continue;
				if (usr.alias === settings.guest) continue;
				if (usr.settings&USER_QUIET) continue;
				if (usr.logontime < time() - settings.inactivity) continue;
				var webAction = getSessionValue(usr.number, 'action');
				if (webAction === null) continue;
				reply.push({
                    status: '',
					action: locale.strings.api_system.nodelist_action_prefix + ' ' + webAction,
					user: usr.alias,
                    connection: 'W'
				});
			}
            usr = undefined;
			break;

		case 'send-telegram':
			if (user.alias === settings.guest) break;
			if (typeof http_request.query.user === 'undefined') break;
			if (typeof http_request.query.telegram === 'undefined' ||
				http_request.query.telegram[0] === ''
			) {
				break;
			}
			if (http_request.query.telegram[0].length >
				settings.maximum_telegram_length
			) {
				break;
			}
			var un = system.matchuser(http_request.query.user[0]);
			if (un < 1) break;
			system.put_telegram(
				un, format(
					locale.strings.api_system.telegram_header_format,
					user.alias, (new Date()).toLocaleString()
				) + '\r\n' + http_request.query.telegram[0] + '\r\n'
			);
			break;

		case 'get-telegram':
			if (user.alias === settings.guest) break;
			reply.telegram = system.get_telegram(user.number);
			break;

		case 'set-xtrn-intent':
			if (user.alias === settings.guest) break;
			if (typeof http_request.query.code === 'undefined') break;
			if (http_request.query.code[0].length > 8) break;
			if (typeof xtrn_area.prog[http_request.query.code[0]] === 'undefined') {
				break;
			}
			setSessionValue(user.number, 'xtrn', http_request.query.code[0]);
			break;

		default:
			break;

	}

}

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);
