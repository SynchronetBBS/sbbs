require("sbbsdefs.js", 'SS_PAUSEOFF');
var dk_old_ctrlkey_passthru = console.ctrlkey_passthru;
var dk_old_pauseoff = bbs.sys_status & SS_PAUSEOFF;
js.on_exit("console.ctrlkey_passthru=dk_old_ctrlkey_passthru;bbs.sys_status=(bbs.sys_status &~ SS_PAUSEOFF)|dk_old_pauseoff");
console.ctrlkey_passthru=0x7fffffff;	// Disable all parsing.

/*
 * SBBS Console doesn't support local conio screens.
 */
dk.console.local = false;
delete dk.console.local_screen;

/*
 * Clears the current screen to black and moves to location 1,1
 */

dk.console.remote_io = {
	clear:function() {
		'use strict';
		console.line_counter = 0;
		console.clear();
		console.line_counter = 0;
	},

	/*
	 * Clears to end of line.
	 * Not available witout ANSI (???)
	 */
	cleareol:function() {
		'use strict';
		console.line_counter = 0;
		console.cleartoeol();
		console.line_counter = 0;
	},

	/*
	 * Moves the cursor to the specified position.
	 * returns false on error.
	 * Not available without ANSI
	 */
	gotoxy:function(x,y) {
		'use strict';
		console.line_counter = 0;
		console.gotoxy(x+1,y+1);
		console.line_counter = 0;
	},

	movex:function(pos) {
		'use strict';
		console.line_counter = 0;
		if (pos > 0) {
			console.right(pos);
		}
		if (pos < 0) {
			console.left(0-pos);
		}
		console.line_counter = 0;
	},

	movey:function(pos) {
		'use strict';
		console.line_counter = 0;
		if (pos > 0) {
			console.down(pos);
		}
		if (pos < 0) {
			console.up(0-pos);
		}
		console.line_counter = 0;
	},

	/*
	 * Writes a string unmodified.
	 */
	print:function(string) {
		'use strict';
		console.line_counter = 0;
		console.write(string);
		console.line_counter = 0;
	}
};

var input_queue = load(true, "sbbs_input.js", bbs.node_num);
js.on_exit("input_queue.write(''); input_queue.poll(0x7fffffff);");

// Get stuff that would come from the dropfile if there was one.
// From the bbs object.
dk.connection.node = bbs.node_num;
dk.connection.time = strftime("%H:%M", bbs.logon_time);
dk.user.seconds_remaining_from = time();
dk.user.seconds_remaining = bbs.get_time_left();
dk.user.minutes_remaining = parseInt(bbs.get_time_left() / 60, 10);

// From the client object...
dk.connection.type = client.protocol;
dk.connection.socket = client.socket.descriptor;
dk.connection.telnet = client.protocol === 'Telnet';

// From the console object
dk.user.ansi_supported = (console.autoterm & USER_ANSI) === USER_ANSI;
if (console.screen_rows !== dk.console.rows || console.screen_columns !== dk.console.cols) {
	dk.console.rows = console.screen_rows;
	dk.console.cols = console.screen_columns;
	dk.console.remote_screen = new Screen(dk.console.cols, dk.console.rows, 7, ' ');
}

// From the user object...
dk.user.full_name = user.name;
dk.user.location = user.location;
dk.user.home_phone = user.phone;
dk.user.pass = user.security.password;
dk.user.level = user.security.level;
dk.user.times_on = user.stats.total_logons;
dk.user.last_called = strftime("%m/%d/%y", user.laston_date);
dk.user.expires = strftime("%m/%d/%y", user.expiration_date);
dk.user.number = user.number;
dk.user.default_protocol = user.download_protocol;
dk.user.uploads = user.stats.files_uploaded;
dk.user.upload_kb = parseInt(user.stats.bytes_uploaded/1024, 10);
dk.user.downloads = user.stats.files_downloaded;
dk.user.download_kb = parseInt(user.stats.bytes_downloaded/1024, 10);

// TODO: How do credits map to bytes?
delete dk.user.max_download_kb_per_day;
// TODO: Time credits
delete dk.user.time_credits;

// TODO: de-euroify if needed.
dk.user.birthdate = user.birthdate;

dk.user.alias = user.alias;
dk.user.last_new_file_scan_date = strftime("%m/%d/%y", user.new_file_time);
dk.user.last_call_time = strftime("%H:%M", user.laston_date);
dk.user.comment = user.comment;
dk.user.messages_left = user.stats.total_posts;
dk.user.expert_mode = (user.settings & USER_EXPERT) === USER_EXPERT;

// From the system object (TODO: These could also be populated in JSExec)
dk.system.main_dir = system.node_dir;
dk.system.gen_dir = system.data_dir;
dk.system.sysop_name = system.operator;

// TODO: cfg->color array not available.
delete dk.system.default_attr;
// TODO: Next event time...
delete dk.system.event_time;
dk.system.record_locking = true;

