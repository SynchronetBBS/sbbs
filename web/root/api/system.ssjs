load('sbbsdefs.js');
load('nodedefs.js');
load('modopts.js');
var settings = get_mod_options('web');

load(settings.web_lib + 'init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'language.js');

var _language = getLanguage(settings.language_file || 'english.ini', 'api_system');

var reply = {};

if ((http_request.method === 'GET' || http_request.method === 'POST') &&
	typeof http_request.query.call !== 'undefined' &&
	user.number > 0
) {

	switch (http_request.query.call[0]) {

		case 'node-list':
			reply = system.node_list.map(
				function (node) {
					if (node.status === 3) var usr = new User(node.useron);
					return (
						{	status : format(
								NodeStatus[node.status], node.aux, node.extaux
							),
							action : format(
								NodeAction[node.action], node.aux, node.extaux
							),
							user : (typeof usr === 'undefined' ? '' : usr.alias)
						}
					);
				}
			);
			var usr = new User(1);
			for (var un = 1; un < system.lastuser; un++) {
				usr.number = un;
				if (usr.connection !== 'HTTP') continue;
				if (usr.alias === settings.guest) continue;
				if (usr.settings&USER_QUIET) continue;
				if (usr.logontime < time() - settings.inactivity) continue;
				var webAction = getSessionValue(usr.number, 'action');
				if (webAction === null) continue;
				reply.push(
					{	status : '',
						action : _language.nodelist_action_prefix + ' ' + webAction,
						user : usr.alias
					}
				);
			}
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
				un,
				format(
					_language.telegram_header_format,
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
