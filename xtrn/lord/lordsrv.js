// LORD data server... whee!
js.load_path_list.unshift(js.exec_dir+"load/");
require('recorddefs.js', 'Player_Def');
require('recordfile.js', 'RecordFile');

var SPlayer_Def = Player_Def.slice();
var settings = {
	hostnames:['0.0.0.0', '::'],
	port:0xdece,
	retry_count:30,
	retry_delay:15,
	player_file:js.exec_dir + 'splayer.dat'
};
var pfile = new RecordFile(settings.player_file, SPlayer_Def);
var sock;
var socks;
var pdata = [];
var idx;
var whitelist = ['Record', 'Yours'];

// TODO: This is obviously silly.
function validate_user(user, pass)
{
	return true;
}

// TODO: Blocking Locks
// TODO: socket_select with a read array and a write array
function handle_request() {
	var buf = '';
	var tmp;
	var block;
	var req;

	function close_sock(sock) {
		log("Closed connection from "+sock.remote_ip_address+'.'+sock.remote_port);
		socks.splice(socks.indexOf(sock), 1);
		delete sock.LORD;
		sock.close();
		return;
	}

	function handle_command_data(sock, data) {
		var tmph;

		// TODO: This should be in a require()d file
		function update_player(from, to) {
			if (from.hp < 0) {
				from.hp = 0;
			}
			if (from.hp > 32000) {
				from.hp = 32000;
			}
			if (from.hp_max < 0) {
				from.hp_max = 0;
			}
			if (from.hp_max > 32000) {
				from.hp_max = 32000;
			}
			if (from.forest_fights < 0) {
				from.forest_fights = 0;
			}
			if (from.forest_fights > 32000) {
				from.forest_fights = 32000;
			}
			if (from.pvp_fights < 0) {
				from.pvp_fights = 0;
			}
			if (from.pvp_fights > 32000) {
				from.pvp_fights = 32000;
			}
			if (from.gold < 0) {
				from.gold = 0;
			}
			if (from.gold > 2000000000) {
				from.gold = 2000000000;
			}
			if (from.bank < 0) {
				from.bank = 0;
			}
			if (from.bank > 2000000000) {
				from.bank = 2000000000;
			}
			if (from.def < 0) {
				from.def = 0;
			}
			if (from.def > 32000) {
				from.def = 32000;
			}
			if (from.str < 0) {
				from.str = 0;
			}
			if (from.str > 32000) {
				from.str = 32000;
			}
			if (from.cha < 0) {
				from.cha = 0;
			}
			if (from.cha > 32000) {
				from.cha = 32000;
			}
			if (from.exp < 0) {
				from.exp = 0;
			}
			if (from.exp > 2000000000) {
				from.exp = 2000000000;
			}
			if (from.laid < 0) {
				from.laid = 0;
			}
			if (from.laid > 32000) {
				from.laid = 32000;
			}
			if (from.kids < 0) {
				from.kids = 0;
			}
			if (from.kids > 32000) {
				from.kids = 32000;
			}
			Player_Def.forEach(function(o) {
				if (to[o.prop] !== undefined)
					to[o.prop] = from[o.prop];
			});
		}

		if (data.substr(-2) !== '\r\n') {
			return false;
		}
		data = data.substr(0, data.length - 2);

		switch(sock.LORD.cmd) {
			case 'PutPlayer':
				try {
					tmph = JSON.parse(data);
				}
				catch(e) {
					return false;
				}
				if (tmph.Record !== undefined && tmph.Record !== sock.LORD.record) {
					return false;
				}
				update_player(tmph, pdata[sock.LORD.record]);
				pdata[sock.LORD.record].put();
				break;
			default:
				return false;
		}
		return true;
	}

	function handle_command(sock, request) {
		var tmph;
		var cmd;

		function validate_record(sock, vrequest, fields, bbs_check) {
			var tmpv = vrequest.split(' ');
			if (tmpv.length !== fields) {
				return undefined;
			}
			tmpv = parseInt(tmpv[1], 10);
			if (isNaN(tmpv)) {
				return undefined;
			}
			if (pdata.length === 0 || tmpv < 0 || tmpv >= pdata.length) {
				return undefined;
			}
			if (bbs_check) {
				if (pdata[tmpv].SourceSystem !== sock.LORD.bbs) {
					return undefined;
				}
			}
			return tmpv;
		}

		function parse_pending(sock, prequest, field) {
			var tmpp = prequest.split(' ');

			tmpp = parseInt(tmpp[field-1], 10);
			if (isNaN(tmpp)) {
				return undefined;
			}
			// TODO: Better sanity checking...
			if (tmpp > 10240) {
				return undefined;
			}
			if (tmpp < 0) {
				return undefined;
			}
			return tmpp;
		}

		tmph = request.indexOf(' ');
		if (tmph === -1)
			cmd = request;
		else
			cmd = request.substr(0, tmph);

		if (sock.LORD.auth === false) {
			switch(cmd) {
				case 'Auth':
					tmph = request.split(' ');
					if (tmph.length < 3) {
						return false;
					}
					else if (validate_user(sock, tmph[1], tmph[2])) {
						log('Auth from: '+sock.remote_ip_address+'.'+sock.remote_port+' as '+tmph[1]);
						sock.LORD.auth = true;
						sock.LORD.bbs = tmph[1];
						sock.writeln('OK');
					}
					else {
						return false;
					}
					break;
				default:
					return false;
			}
			return true;
		}
		else {
			switch(cmd) {
				case 'GetPlayer':
					tmph = validate_record(sock, request, 2, false);
					if (tmph === undefined)
						return false;
					tmph = JSON.stringify(pdata[tmph], whitelist);
					sock.write('PlayerRecord '+tmph.length+'\r\n'+tmph+'\r\n');
					break;
				case 'PutPlayer':
					tmph = validate_record(sock, request, 3, true);
					if (tmph === undefined)
						return false;
					sock.LORD.record = tmph;
					tmph = parse_pending(sock, request, 3);
					if (tmph === undefined)
						return false;
					sock.LORD.cmd = cmd;
					sock.LORD.pending = tmph + 2;
					break;
				case 'NewPlayer':
					if (request.indexOf(' ') !== -1) {
						return false;
					}
					if (pdata.length > 150) {
						// TODO: Check for deleted players.
						sock.writeln('Game Is Full');
						break;
					}
					tmph = pfile.new();
					if (tmph === null) {
						sock.writeln('Server Error');
						break;
					}
					tmph.SourceSystem = sock.LORD.bbs;
					tmph.Yours = true;
					tmph.put();
					pdata.push(tmph);
					tmph = JSON.stringify(tmph, whitelist);
					sock.write('PlayerRecord '+tmph.length+'\r\n'+tmph+'\r\n');
					break;
				case 'RecordCount':
					if (request.indexOf(' ') !== -1) {
						return false;
					}
					sock.writeln(pdata.length);
					break;
				default:
					return false;
			}
			return true;
		}
	}

	do {
		block = this.recv(4096);
		if (block === null)
			break;
		buf += block;
	} while(block.length > 0);

	this.LORD.buf += buf;

	do {
		if (this.LORD.pending > 0) {
			if (this.LORD.buf.length < this.LORD.pending) {
				if (this.buf.length === 0) {
					close_sock(this);
					return
				}
				break;
			}
			else {
				tmp = this.LORD.buf.substr(0, this.LORD.pending);
				this.LORD.buf = this.LORD.buf.substr(this.LORD.pending);
				this.LORD.pending = 0;
				if (!handle_command_data(this, tmp)) {
					close_sock(this);
					return;
				}
			}
		}
		else {
			// TODO: Better sanity checking...
			if (this.LORD.buf.length > 10240) {
				close_sock(this);
				return;
			}
			tmp = this.LORD.buf.indexOf('\n');
			if (tmp === -1)
				break;
			if (tmp !== -1) {
				req = this.LORD.buf.substr(0, tmp + 1);
				this.LORD.buf = this.LORD.buf.substr(tmp + 1);
				req = req.replace(/[\r\n]/g,'');
				if (!handle_command(this, req)) {
					close_sock(this);
					return;
				}
			}
		}
	} while(true);
}

SPlayer_Def.push({
	prop:'SourceSystem',
	name:'Source Account',
	type:'String:20',	// TODO: This is the max BBS ID length
	def:''
});

if (this.server !== undefined)
	sock = this.server.socket;
else
	sock = new ListeningSocket(settings.hostnames, settings.port, 'LORD', {retry_count:settings.retry_count, retry_delay:settings.retry_delay});
if (sock === null)
	throw('Unable to bind listening socket');
sock.LORD_callback = function() {
	var nsock;

	nsock = this.accept();
	nsock.ssl_server = true;
	nsock.nonblocking = true;
	nsock.LORD = {};
	nsock.LORD_callback = handle_request;
	nsock.LORD.auth = false;
	nsock.LORD.pending = 0;
	nsock.LORD.buf = '';
	socks.push(nsock);
	log('Connection accepted from: '+nsock.remote_ip_address+'.'+nsock.remote_port);
};
sock.sock = sock;

socks = [sock];

for (idx = 0; idx < pfile.length; idx++)
	pdata.push(pfile.get(idx));
for (idx = 0; idx < Player_Def.length; idx++)
	whitelist.push(Player_Def[idx].prop);

while(true) {
	var ready;

	ready = socket_select(socks, 60);
	ready.forEach(function(s) {
		socks[s].LORD_callback();
	});
}
