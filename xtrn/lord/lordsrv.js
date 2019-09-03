'use strict';

// LORD data server... whee!
js.load_path_list.unshift(js.exec_dir+"load/");
require('recorddefs.js', 'Player_Def');
require('recordfile.js', 'RecordFile');

var settings = {
	hostnames:['0.0.0.0', '::'],
	port:0xdece,
	retry_count:30,
	retry_delay:15,
	file_prefix:js.exec_dir + 's'
};
var settingsmap = {
	hostnames:'Hostnames',
	port:'Port',
	retry_count:'RetryCount',
	retry_delay:'RetryDelay',
	file_prefix:'GamePrefix'
};
var pfile = new RecordFile(settings.file_prefix+'player.bin', SPlayer_Def);;
var sfile = new RecordFile(settings.file_prefix+'state.bin', Server_State_Def);
var lfile = new File(settings.file_prefix+'logall.lrd');
var socks;
var pdata = [];
var sdata;
var whitelist = ['Record', 'Yours'];
var swhitelist = [];
var logdata = [];
var conversations = {
	bar:{file:new File(settings.file_prefix+'bar.lrd'), lines:[], default_files:[js.exec_dir + 'start1.lrd', js.exec_dir + 'start2.lrd', js.exec_dir + 'start3.lrd', js.exec_dir + 'start4.lrd', js.exec_dir + 'start5.lrd']},
	darkbar:{file:new File(settings.file_prefix+'darkbar.lrd'), lines:[], default_files:[js.exec_dir + 'dstart.lrd']},
	garden:{file:new File(settings.file_prefix+'garden.lrd'), lines:[], default_files:[js.exec_dir + 'gstart.lrd']},
	dirt:{file:new File(settings.file_prefix+'dirt.lrd'), lines:[]}
};

// TODO: This is obviously silly.
function validate_user(user, pass)
{
	return true;
}

// TODO: Blocking Locks (?)
// TODO: socket_select with a read array and a write array
function handle_request() {
	var buf = '';
	var tmp;
	var block;
	var req;

	function close_sock(sock) {
		var put = false;

		log("Closed connection "+sock.descriptor+" from "+sock.remote_ip_address+'.'+sock.remote_port);
		if (sock.LORD !== undefined) {
			if (sock.LORD.player_on !== undefined) {
				if (pdata[sock.LORD.player_on].InIGM.length > 0 && pdata[sock.LORD.player_on].IGMCommand.length === 0) {
					pdata[sock.LORD.player_on].InIGM = '';
					put = true;
				}
				if (pdata[sock.LORD.player_on].in_battle !== -1) {
					// Player on loses...
					pdata[sock.LORD.player_on].gem -= parseInt(pdata[sock.LORD.player_on].gem / 2, 10);
					pdata[sock.LORD.player_on].exp = pdata[sock.LORD.player_on].exp - parseInt(pdata[sock.LORD.player_on].exp / 10, 10);
					pdata[sock.LORD.player_on].gold = 0;
					pdata[sock.LORD.player_on].dead = true;
					pdata[sock.LORD.player_on].inn = false;
					pdata[sock.LORD.player_on].hp = 0;

					// Other player wins...
					pdata[pdata[sock.LORD.player_on].in_battle].pvp += 1;
					if (pdata[pdata[sock.LORD.player_on].in_battle].pvp > 32000) {
						pdata[pdata[sock.LORD.player_on].in_battle].pvp = 32000;
					}
					pdata[pdata[sock.LORD.player_on].in_battle].exp += parseInt(pdata[sock.LORD.player_on].exp / 2, 10);
					if (pdata[pdata[sock.LORD.player_on].in_battle].exp > 2000000000) {
						pdata[pdata[sock.LORD.player_on].in_battle].exp = 2000000000;
					}

					pdata[pdata[sock.LORD.player_on].in_battle].in_battle = -1;
					pdata[pdata[sock.LORD.player_on].in_battle].put();
					pdata[sock.LORD.player_on].on_now = false;
					pdata[sock.LORD.player_on].in_battle = -1;
					put = true;
				}
				if (pdata[sock.LORD.player_on].on_now === true) {
					pdata[sock.LORD.player_on].on_now = false;
					put = true;
				}
				if (put)
					pdata[sock.LORD.player_on].put();
			}
		}
		socks.splice(socks.indexOf(sock), 1);
		delete sock.LORD;
		sock.close();
		return;
	}

	function handle_command_data(sock, data) {
		var tmph;
		var mf;

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
			if (from.name !== undefined && to.name !== undefined) {
				if (from.name !== to.name) {
					if (from.name === sdata.latesthero) {
						sdata.latesthero = to.name;
						sdata.put();
					}
				}
			}
			Player_Def.forEach(function(o) {
				if (to[o.prop] !== undefined)
					to[o.prop] = from[o.prop];
			});
		}

		function logit(data) {
			var lines = data.split(/\r?\n/);
			var line;
			var now = new Date();
			var nv = now.valueOf();
			var oldest = new Date();
			oldest.setHours(0, 0, 0, 0);
			oldest.setDate(oldest.getDate() - 2);

			while(logdata.length > 0 && logdata[0].date < oldest) {
				logdata.shift();
			}
			while(lines.length) {
				line = lines.shift();
				lfile.writeln(nv+':'+line);
				logdata.push({date:now, line:line});
			}
			lfile.writeln(nv+':'+'`.                                `2-`0=`2-`0=`2-`0=`2-');
			logdata.push({date:now, line:'`.                                `2-`0=`2-`0=`2-`0=`2-'});
		}

		log(LOG_DEBUG, sock.descriptor+': '+sock.LORD.cmd+' got '+(data.length - 2)+' bytes of data');
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
				sock.writeln('OK');
				break;
			case 'WriteMail':
				mf = new File(settings.file_prefix +'mail'+sock.LORD.record+'.lrd');
				if (!mf.open('a')) {
					sock.writeln('Unable to send mail');
					break;
				}
				mf.writeln(data);
				mf.close();
				sock.writeln('OK');
				break;
			case 'LogEntry':
				logit(data);
				sock.writeln('OK');
				break;
			case 'AddToConversation':
				tmph = data.split(/\r?\n/);

				if (sock.LORD.conv === 'dirt')
					conversations.dirt.lines = [];
				tmph.forEach(function(l) {
					conversations[sock.LORD.conv].lines.push(l);
				});
				switch(sock.LORD.conv) {
					case 'bar':
						if (sock.LORD.player_on !== undefined) {
							sdata.last_bar = sock.LORD.player_on;
							sdata.put();
						}
					case 'darkbar':
						while (conversations[sock.LORD.conv].lines.length > 18)
							conversations[sock.LORD.conv].lines.shift();
						break;
					case 'garden':
						while (conversations[sock.LORD.conv].lines.length > 20)
							conversations[sock.LORD.conv].lines.shift();
						break;
				}
				conversations[sock.LORD.conv].file.position = 0;
				conversations[sock.LORD.conv].file.truncate(0);
				conversations[sock.LORD.conv].file.writeAll(conversations[sock.LORD.conv].lines);
				sock.writeln('OK');
				break;
			case 'IGMData':
				tmph = data.split(/\r?\n/);
				if (tmph.length !== 2)
					return false;
				pdata[sock.LORD.player_on].InIGM = tmph[0];
				pdata[sock.LORD.player_on].IGMCommand = tmph[1];
				sock.writeln('OK');
				pdata[sock.LORD.player_on].put();
				break;
			default:
				return false;
		}
		return true;
	}

	function handle_command(sock, request) {
		var tmph;
		var tmph2;
		var cmd;
		var mf;

		function validate_record(sock, vrequest, fields, field, bbs_check, allow_negone) {
			var tmpv = vrequest.split(' ');

			if (allow_negone === undefined)
				allow_negone = false;
			if (tmpv.length !== fields) {
				return undefined;
			}
			tmpv = parseInt(tmpv[field-1], 10);
			if (isNaN(tmpv)) {
				return undefined;
			}
			if (allow_negone) {
				if (tmpv < -1) {
					return undefined;
				}
			}
			else {
				if (tmpv < 0) {
					return undefined;
				}
			}
			if (pdata.length === 0 || tmpv >= pdata.length) {
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

		function parse_date(sock, prequest, field) {
			var tmpp = prequest.split(' ');

			tmpp = parseInt(tmpp[field-1], 10);
			if (isNaN(tmpp)) {
				return undefined;
			}
			// TODO: sanity checking...
			if (tmpp < 0) {
				return undefined;
			}
			return new Date(tmpp);
		}

		function get_conversation(sock, prequest) {
			var tmpp = prequest.split(' ');

			if (tmpp.length !== 2)
				return false;
			if (Object.keys(conversations).indexOf(tmpp[1]) === -1)
				return false;
			tmpp = conversations[tmpp[1]].lines.join('\n');
			sock.write('Conversation '+tmpp.length+'\r\n'+tmpp+'\r\n');
			return true;
		}

		function add_conversation(sock, prequest) {
			var tmpp = prequest.split(' ');

			if (tmpp.length !== 3)
				return false;
			if (Object.keys(conversations).indexOf(tmpp[1]) === -1)
				return false;
			sock.LORD.conv = tmpp[1];
			tmpp = parse_pending(sock, prequest, 3);
			if (tmpp === undefined)
				return false;
			sock.LORD.cmd = cmd;
			sock.LORD.pending = tmpp + 2;
			return true;
		}

		function send_log(sock, start, end) {
			var log = '';
			var md;
			var i;
			var ent;
			var happenings = ['`4  A Child was found today!  But scared deaf and dumb.',
			    '`4  More children are missing today.',
			    '`4  A small girl was missing today.',
			    '`4  The town is in grief.  Several children didn\'t come home today.',
			    '`4  Dragon sighting reported today by a drunken old man.',
			    '`4  Despair covers the land - more bloody remains have been found today.',
			    '`4  A group of children did not return from a nature walk today.',
			    '`4  The land is in chaos today.  Will the abductions ever stop?',
			    '`4  Dragon scales have been found in the forest today..Old or new?',
			    '`4  Several farmers report missing cattle today.'];

			log += '`2  The Daily Happenings....\n';
			log += '`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n';

			// Choose a first entry based on start time.
			md = md5_calc(start.toString());
			for (i = 0; i < md.length; i++) {
				ent = parseInt(md[i], 16);
				if (ent < happenings.length)
					break;
			}
			// Small bias...
			if (ent >= happenings.length)
				ent = 0;
			log += happenings[ent]+'\n'+'`.                                `2-`0=`2-`0=`2-`0=`2-\n';

			logdata.forEach(function(l) {
				if (l.date >= start && (end === undefined || l.date < end)) {
					log += l.line+'\n';
				}
			});
			sock.write('LogData '+log.length+'\r\n'+log+'\r\n');
		}

		log(LOG_DEBUG, sock.descriptor+': '+request);
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
						log('Auth '+sock.descriptor+' from: '+sock.remote_ip_address+'.'+sock.remote_port+' as '+tmph[1]);
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
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					tmph = JSON.stringify(pdata[tmph], whitelist);
					if (tmph.SourceSystem === sock.LORD.bbs)
						tmph.Yours = true;
					sock.write('PlayerRecord '+tmph.length+'\r\n'+tmph+'\r\n');
					break;
				case 'GetState':
					tmph = JSON.stringify(sdata, swhitelist);
					sock.write('StateData '+tmph.length+'\r\n'+tmph+'\r\n');
					break;
				case 'ClearPlayer':
					if (request.indexOf(' ') > -1)
						return false;
					delete sock.LORD.player_on;
					sock.LORD.cleared = true;
					sock.writeln('OK');
					break;
				case 'SetPlayer':
					tmph = validate_record(sock, request, 2, 2, true);
					if (tmph === undefined)
						return false;
					// Check if on another connection...
					if (sock.LORD.cleared === undefined) {
						socks.forEach(function(s) {
							if (s.LORD !== undefined && s.LORD.player_on === tmph)
								tmph = undefined;
						});
					}
					else {
						delete sock.LORD.cleared;
					}
					if (tmph === undefined)
						return false;
					sock.LORD.player_on = tmph;
					sock.writeln('OK');
					break;
				case 'CheckMail':
					tmph = validate_record(sock, request, 2, 2, true);
					if (tmph === undefined)
						return false;
					if (file_exists(settings.file_prefix +'mail'+tmph+'.lrd'))
						sock.writeln('Yes');
					else
						sock.writeln('No');
					break;
				case 'GetMail':
					tmph = validate_record(sock, request, 2, 2, true);
					if (tmph === undefined)
						return false;
					mf = settings.file_prefix +'mail'+tmph+'.lrd';
					tmph = file_size(mf);
					if (tmph === -1)
						sock.write('Mail 0\r\n\r\n');
					else {
						sock.writeln('Mail '+tmph);
						sock.sendfile(mf);
						sock.write('\r\n');
						file_remove(mf);
					}
					break;
				case 'KillMail':
					tmph = validate_record(sock, request, 2, 2, true);
					if (tmph === undefined)
						return false;
					file_remove('smail'+tmph+'.lrd');
					sock.writeln('OK');
					break;
				case 'NewPlayer':
					if (request.indexOf(' ') !== -1) {
						return false;
					}
					tmph = pdata.forEach(function(o) {
						if (tmph === undefined) {
							if (o.name === 'X')
								tmph = o;
						}
					});
					if (tmph === undefined) {
						if (pdata.length >= 150) {
							sock.writeln('Game Is Full');
							break;
						}
						tmph = pfile.new();
						if (tmph === null) {
							sock.writeln('Server Error');
							break;
						}
						pdata.push(tmph);
					}
					else
						tmph.reInit();
					tmph.SourceSystem = sock.LORD.bbs;
					tmph.Yours = true;
					tmph.in_battle = -1;
					tmph.put();
					tmph = JSON.stringify(tmph, whitelist);
					sock.write('PlayerRecord '+tmph.length+'\r\n'+tmph+'\r\n');
					break;
				case 'RecordCount':
					if (request.indexOf(' ') !== -1) {
						return false;
					}
					sock.writeln(pdata.length);
					break;
				case 'NewHero':
					tmph = request.split(' ');
					if (tmph.length < 2)
						return false;
					tmph[1] = tmph.slice(1).join(' ');
					if (tmph[1].length < 3)
						return false;
					sdata.latesthero = tmph[1];
					sdata.put;
					sock.writeln('OK');
					break;
				case 'SethMarried':
					tmph = validate_record(sock, request, 2, 2, true, true);
					if (tmph === undefined)
						return false;
					if (tmph === -1 && sdata.married_to_seth >= 0) {
						if (pdata[sdata.married_to_seth].SourceSystem !== sock.LORD.bbs)
							return false;
					}
					if (tmph >= 0) {
						if (sdata.married_to_seth !== -1) {
							sock.writeln('No');
							break;
						}
						if (pdata[tmph].SourceSystem !== sock.LORD.bbs)
							return false;
					}
					sdata.married_to_seth = tmph;
					sock.writeln('Yes');
					break;
				case 'VioletMarried':
					tmph = validate_record(sock, request, 2, 2, true, true);
					if (tmph === undefined)
						return false;
					if (tmph === -1 && sdata.married_to_violet >= 0) {
						if (pdata[sdata.married_to_violet].SourceSystem !== sock.LORD.bbs)
							return false;
					}
					if (tmph >= 0) {
						if (sdata.married_to_violet !== -1) {
							sock.writeln('No');
							break;
						}
						if (pdata[tmph].SourceSystem !== sock.LORD.bbs)
							return false;
					}
					sdata.married_to_violet = tmph;
					sock.writeln('Yes');
					break;
				case 'Marry':
					tmph = validate_record(sock, request, 3, 2, true);
					if (tmph === undefined)
						return false;
					tmph2 = validate_record(sock, request, 3, 3, false);
					if (tmph2 === undefined)
						return false;
					if (pdata[tmph].married_to !== -1 || pdata[tmph2].married_to !== -1) {
						sock.writeln('No');
						break;
					}
					pdata[tmph].married_to = tmph2;
					pdata[tmph2].married_to = tmph;
					pdata[tmph].put();
					pdata[tmph2].put();
					sock.writeln('Yes');
					break;
				case 'Divorce':
					tmph = validate_record(sock, request, 3, 2, true);
					if (tmph === undefined)
						return false;
					tmph2 = validate_record(sock, request, 3, 3, false);
					if (tmph2 === undefined)
						return false;
					if (pdata[tmph].married_to !== tmph2 || pdata[tmph2].married_to !== tmph) {
						sock.writeln('No');
						break;
					}
					pdata[tmph].married_to = -1;
					pdata[tmph2].married_to = -1;
					pdata[tmph].put();
					pdata[tmph2].put();
					sock.writeln('Yes');
					break;
				case 'AddForestGold':
					tmph = request.match(/^AddForestGold ([0-9]+)$/);
					if (tmph === null)
						return false;
					sdata.forest_gold += parseInt(tmph[1], 10);
					sdata.put();
					break;
				case 'GetForestGold':
					tmph = request.match(/^GetForestGold ([0-9]+)$/);
					if (tmph === null)
						return false;
					psock.writeln('ForestGold '+sdata.forest_gold);
					tmph = parseInt(tmph[1], 10);
					if (tmph < 100)
						tmph = 100;
					sdata.forest_gold = tmph;
					sdata.put();
					break;
				case 'BattleStart':
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[tmph].dead === true) {
						sock.writeln('Dead');
						break;
					}
					if (pdata[tmph].in_battle !== -1) {
						sock.writeln('InBattle '+pdata[tmph].in_battle);
						break;
					}
					if (pdata[tmph].InIGM.length > 0) {
						sock.writeln('Out: '+pdata[tmph].InIGM);
						break;
					}
					pdata[tmph].in_battle = sock.LORD.player_on;
					pdata[sock.LORD.player_on].in_battle = tmph;
					if (pdata[tmph].on_now === true) {
						sock.writeln('Online');
						break;
					}
					sock.writeln('OK');
					break;
				case 'WonBattle':
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					if (pdata[tmph].on_now === true)
						return false;
					if (pdata[tmph].dead === true)
						return false;
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[tmph].in_battle !== sock.LORD.player_on)
						return false;
					if (pdata[sock.LORD.player_on].in_battle !== tmph)
						return false;
					pdata[tmph].pvp += 1;
					if (pdata[tmph].pvp > 32000) {
						pdata[tmph].pvp = 32000;
					}
					pdata[tmph].exp += parseInt(pdata[sock.LORD.player_on].exp / 2, 10);
					if (pdata[tmph].exp > 2000000000) {
						pdata[tmph].exp = 2000000000;
					}
					pdata[tmph].in_battle = -1;
					pdata[tmph].put();
					pdata[sock.LORD.player_on].in_battle = -1;
					pdata[sock.LORD.player_on].put();
					sock.writeln('OK');
					break;
				case 'AbortBattleWait':
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[sock.LORD.player_on].in_battle === -1)
						return false;
					if (sock.LORD.online_battle_response !== undefined)
						delete sock.LORD.online_battle_response;
					if (pdata[pdata[sock.LORD.player_on].in_battle].on_now === false) {
						sock.writeln('OK');
						break;
					}
					if (pdata[pdata[sock.LORD.player_on].in_battle].in_battle !== sock.LORD.player_on) {
						sock.writeln('OK');
						break;
					}
					if (sock.LORD.online_battle_sock === undefined)
						pdata[pdata[sock.LORD.player_on].in_battle].online_battle_response = 'R';
					else {
						pdata[sock.LORD.player_on].online_battle_sock.writeln('R');
						delete sock.LORD.online_battle_sock;
					}
					sock.writeln('OK');
					break;
				case 'WaitBattleResponse':
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[sock.LORD.player_on].in_battle === -1)
						return false;
					if (pdata[pdata[sock.LORD.player_on].in_battle].on_now === false)
						sock.LORD.online_battle_response = 'R';
					if (pdata[pdata[sock.LORD.player_on].in_battle].in_battle !== sock.LORD.player_on)
						sock.LORD.online_battle_response = 'R';
					if (sock.LORD.online_battle_response === undefined)
						pdata[pdata[sock.LORD.player_on].in_battle].online_battle_sock = sock;
					else {
						sock.writeln(sock.LORD.online_battle_response);
						delete sock.LORD.online_battle_response;
					}
					break;
				case 'SendBattleResponse':
					tmph = request.split(' ');
					if (tmph.length < 2)
						return false;
					tmph[1] = tmph.slice(1).join(' ');
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[sock.LORD.player_on].in_battle === -1)
						return false;
					if (pdata[pdata[sock.LORD.player_on].in_battle].on_now === false) {
						sock.writeln('OK')
						break;
					}
					if (pdata[pdata[sock.LORD.player_on].in_battle].in_battle !== sock.LORD.player_on) {
						sock.writeln('OK');
						break;
					}
					if (pdata[sock.LORD.player_on].online_battle_sock === undefined)
						pdata[pdata[sock.LORD.player_on].in_battle].online_battle_response = tmph[1];
					else {
						pdata[sock.LORD.player_on].online_battle_sock.writeln(tmph[1]);
						delete sock.LORD.online_battle_sock;
					}
					sock.writeln('OK');
					break;
				case 'LostBattle':
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					if (pdata[tmph].on_now === true)
						return false;
					if (pdata[tmph].dead === true)
						return false;
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[tmph].in_battle !== sock.LORD.player_on)
						return false;
					if (pdata[sock.LORD.player_on] !== tmph)
						return false;
					pdata[tmph].gem -= parseInt(pdata[tmph].gem / 2, 10);
					pdata[tmph].exp = pdata[tmph].exp - parseInt(pdata[tmph].exp / 10, 10);
					pdata[tmph].gold = 0;
					pdata[tmph].dead = true;
					pdata[tmph].inn = false;
					pdata[tmph].hp = 0;
					pdata[tmph].in_battle = -1;
					pdata[tmph].put();
					pdata[sock.LORD.player_on].in_battle = -1;
					pdata[sock.LORD.player_on].put();
					sock.writeln('OK');
					break;
				case 'DoneOnlineBattle':
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[sock.LORD.player_on].in_battle === -1)
						return false;
					pdata[sock.LORD.player_on].in_battle = -1;
					pdata[sock.LORD.player_on].put();
					sock.writeln('OK');
					break;
				case 'RanFromBattle':
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					if (pdata[tmph].on_now === true)
						return false;
					if (pdata[tmph].dead === true)
						return false;
					if (sock.LORD.player_on === undefined)
						return false;
					if (pdata[tmph].in_battle !== sock.LORD.player_on)
						return false;
					if (pdata[sock.LORD.player_on] !== tmph)
						return false;
					pdata[tmph].in_battle = -1;
					pdata[tmph].put();
					pdata[sock.LORD.player_on].in_battle = -1;
					pdata[sock.LORD.player_on].put();
					sock.writeln('OK');
					break;
				case 'CheckBattle':
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					if (pdata[tmph].in_battle === -1) {
						sock.writeln('No');
						break;
					}
					sock.writeln(pdata[tmph].in_battle);
					break;
				case 'GetLogFrom':
					tmph = parse_date(sock, request, 2);
					if (tmph === undefined)
						return false;
					send_log(sock, tmph);
					break;
				case 'GetLogRange':
					tmph = parse_date(sock, request, 2);
					if (tmph === undefined)
						return false;
					tmph2 = parse_date(sock, request, 3);
					if (tmph2 === undefined)
						return false;
					send_log(sock, tmph, tmph2);
					break;
				case 'GetConversation':
					if (!get_conversation(sock, request))
						return false;
					break;
				case 'GetIGM':
					tmph = validate_record(sock, request, 2, 2, false);
					if (tmph === undefined)
						return false;
					sock.write('IGMData '+(pdata[tmph].InIGM.length + pdata[tmph].IGMCommand.length + 2)+'\r\n'+pdata[tmph].InIGM + '\r\n' + pdata[tmph].IGMCommand + '\r\n');
					break;
				case 'AddToConversation':
					if (!add_conversation(sock, request))
						return false;
					break;
				case 'WriteMail':
					tmph = validate_record(sock, request, 3, 2, false);
					if (tmph === undefined)
						return false;
					sock.LORD.record = tmph;
					tmph = parse_pending(sock, request, 3);
					if (tmph === undefined)
						return false;
					sock.LORD.cmd = cmd;
					sock.LORD.pending = tmph + 2;
					break;
				case 'PutPlayer':
					tmph = validate_record(sock, request, 3, 2, true);
					if (tmph === undefined) {
						return false;
					}
					sock.LORD.record = tmph;
					tmph = parse_pending(sock, request, 3);
					if (tmph === undefined) {
						return false;
					}
					sock.LORD.cmd = cmd;
					sock.LORD.pending = tmph + 2;
					break;
				case 'LogEntry':
					tmph = parse_pending(sock, request, 2);
					if (tmph === undefined)
						return false;
					sock.LORD.cmd = cmd;
					sock.LORD.pending = tmph + 2;
					break;
				case 'IGMData':
					tmph = parse_pending(sock, request, 2);
					if (tmph === undefined)
						return false;
					if (sock.LORD.player_on === undefined)
						return false;
					sock.LORD.cmd = cmd;
					sock.LORD.pending = tmph + 2;
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
			if (this.LORD.buf.length >= this.LORD.pending) {
				tmp = this.LORD.buf.substr(0, this.LORD.pending);
				this.LORD.buf = this.LORD.buf.substr(this.LORD.pending);
				this.LORD.pending = 0;
				if (!handle_command_data(this, tmp)) {
					close_sock(this);
					return;
				}
			}
			else
				break;
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

	if (buf.length === 0) {
		close_sock(this);
	}
}

function main() {
	var tmpplayer;
	var lline;
	var lmatch;
	var sock;
	var idx;
	var ldate;
	var oldest;

	if (js.global.server !== undefined)
		sock = js.global.server.socket;
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
		log('Connection '+nsock.descriptor+' accepted from: '+nsock.remote_ip_address+'.'+nsock.remote_port);
	};
	sock.sock = sock;

	socks = [sock];

	for (idx = 0; idx < pfile.length; idx++) {
		tmpplayer = pfile.get(idx);
		if (tmpplayer.on_now) {
			tmpplayer.on_now = false;
			tmpplayer.put();
		}
		tmpplayer.in_battle = -1;
		pdata.push(tmpplayer);
	}
	for (idx = 0; idx < Player_Def.length; idx++)
		whitelist.push(Player_Def[idx].prop);
	file_touch(lfile.name);
	if (!lfile.open('a+'))
		throw('Unable to open logfile '+lfile.name);
	lfile.position = 0;
	// Calculate the oldest log entry we'll keep in memory.
	oldest = new Date();
	oldest.setHours(0, 0, 0, 0);
	oldest.setDate(oldest.getDate() - 2);
	while ((lline = lfile.readln()) !== null) {
		lmatch = lline.match(/^([0-9]+):(.*)$/);
		if (lmatch === null) {
			throw('Invalid line in log: '+lline);
		}

		ldate = new Date(parseInt(lmatch[1], 10));
		if (ldate >= oldest)
			logdata.push({date:ldate, line:lmatch[2]});
	}
	if (sfile.length < 1)
		sdata = sfile.new();
	else
		sdata = sfile.get(0);
	if (sdata === undefined) {
		throw('Unable to access '+sfile.file.name+' len: '+sfile.length);
	}
	for (idx = 0; idx < Server_State_Def.length; idx++)
		swhitelist.push(Server_State_Def[idx].prop);
	Object.keys(conversations).forEach(function(c) {
		if ((!file_exists(conversations[c].file.name)) && conversations[c].default_files !== undefined) {
			file_copy(conversations[c].default_files[random(conversations[c].default_files.length)], conversations[c].file.name);
		}
		if (!conversations[c].file.open('a+'))
			throw('Unable to open '+conversations[c].file.name);
		conversations[c].file.position = 0;
		conversations[c].lines = conversations[c].file.readAll();
	});

	while(true) {
		var ready;

		ready = socket_select(socks, 60);
		ready.forEach(function(s) {
			socks[s].LORD_callback();
		});
	}
}

function parse_settings()
{
	var i;
	var f;

	function fixup_prefix() {
		if (settings.file_prefix.length > 0) {
			if (settings.file_prefix[0] === '/' || settings.file_prefix[0] === '\\'
			    || (settings.file_prefix[1] === ':' && (settings.file_prefix[2] === '\\' || settings.file_prefix[2] === '/'))) {
				// Nothing...
			}
			else {
				settings.file_prefix = js.exec_dir + settings.game_prefix;
			}
		}
		if (file_isdir(settings.file_prefix)) {
			settings.file_prefix = backslash(settings.file_prefix);
		}
	}

	for (i = 0; i < argc; i++) {
		if (argv[i] === '-p' && argc > (i + 1)) {
			settings.file_prefix = argv[++i];
			fixup_prefix();
		}
	}

	f = new File(settings.file_prefix + 'lordsrv.ini');
	if (file_exists(f.name)) {
		if (!f.open('r')) {
			throw('Unable to open '+f.name);
		}
		Object.keys(settings).forEach(function(key) {
			if (settingsmap[key] === undefined) {
				throw('Unmapped setting "'+key+'"');
			}
			settings[key] = ini.iniGetValue(null, settingsmap[key], settings[key]);
		});
		fixup_prefix();
	}
	return true;
}

if (parse_settings()) {
	main();
}
