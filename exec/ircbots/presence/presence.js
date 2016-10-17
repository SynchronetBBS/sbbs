/*	'Presence' IRC bot module; echicken -at- bbs.electronicchicken.com

	-	Provides the 'nodelist' command, to output a "Who's Online" listing to
		an IRC channel or private message.

	-	Provides optional (default: enabled) log-on / log-off notifications, and
		external program launch notifications.

	Setup:

	-	Paste the following section into your 

		[module_presence]
		channels = #mybbs
		dir = /sbbs/exec/ircbots/presence/
		global = false
		lib = nodedefs.js
		notify_logon = true
		notify_logoff = true
		notify_xtrn = true

	-	Alter the 'channels', 'global', 'notify_logon', 'notify_logoff', and
		'notify_xtrn' settings as desired

*/

const MODULE_NAME = 'presence';	// The name of the module

// Load this module's configuration from ircbot.ini
function get_config(module) {
	var f = new File(system.ctrl_dir + 'ircbot.ini');
	f.open('r');
	var cfg = f.iniGetObject(module);
	f.close();
	if (typeof cfg.channels !== 'undefined') {
		// Filter out channel keys, we don't need them and they shouldn't be here
		cfg.channels = cfg.channels.split(',').map(
			function (e) { return e.replace(/\s.*/g, ''); }
		);
	}
	return cfg;
}

var cfg = get_config('module_' + MODULE_NAME);
var usr = new User(1);

var shadow_node_list = system.node_list.map(
	function (e) {
		return {
			useron : e.useron,
			status : e.status,
			action : e.action,
			aux : e.aux
		};
	}
);

function notify (srv, msg) {
	cfg.channels.forEach(function (e) { srv.o(e, msg); });
}

/*	Compare the current status of nodes to what was seen last time.  If a user
	has logged on or off, send a notification.  If they've launched an external
	program, send a notification. */
function node_scan (srv) {
	var fmt = 'Node %s: %s logged %s';
	system.node_list.forEach(
		function (e, i) {
			// If somebody was online but isn't any longer
			if (cfg.notify_logoff &&
				shadow_node_list[i].status === NODE_INUSE &&
				e.status !== NODE_INUSE
			) {
				usr.number = shadow_node_list[i].useron;
				notify(srv, format(fmt, i + 1, usr.alias, 'off'));
			// If somebody is online
			} else if (e.status === NODE_INUSE) {
				usr.number = e.useron;
				// If they weren't online the last time we checked
				if (cfg.notify_logon &&
					shadow_node_list[i].status !== NODE_INUSE
				) {
					notify(srv, format(fmt, i + 1, usr.alias, 'on'));
				}
				// If we want to notify when someone launches a door
				if (cfg.notify_xtrn &&
					e.action === NODE_XTRN && // If they're in the xtrn section
					e.aux && // And actually running an external program
					// And if we haven't notified about this already
					(	shadow_node_list[i].action !== NODE_XTRN ||
						shadow_node_list[i].aux !== e.aux
					)
				) {
					var xtrn = xtrn_area.prog[usr.curxtrn].name;
					notify(srv, usr.alias + ' is running ' + xtrn);
				}
			}
			// Update the stored node data for comparison next time around
			shadow_node_list[i] = { 
				useron : e.useron,
				status : e.status,
				action : e.action,
				aux : e.aux
			};
		}
	);
}

/*	The IRC bot will call this function each time through its main loop.
	If you want your IRC bot to do things independently / automatically (rather
	than simply reacting to a command) you can trigger those things in your
	'main' function. */
function main (srv) {
	node_scan(srv);
	yield();
}

// The 'nodelist' command displays users online, if any
Bot_Commands["NODELIST"] = new Bot_Command(0, false, false); // min_security, args_needed, ident_needed
Bot_Commands["NODELIST"].usage = "lol :|";
Bot_Commands["NODELIST"].command = function (target, onick, ouh, srv, lbl, cmd) {
	var shown = false;
	system.node_list.forEach(
		function (e, i) {
			// If somebody is online
			if (e.status === NODE_INUSE) {
				var u = new User(e.useron);
				// Mark the 'shown' flag true and display the header
				if (!shown) {
					shown = true;
					srv.o(target, 'Users online: ');
				}
				srv.o(
					target,
					format(
						'Node %s: %s %s', i + 1, u.alias, NodeAction[e.action]
					)
				);
			}
		}
	);
	// If nobody is online
	if (!shown) srv.o(target, 'No users online.');
}