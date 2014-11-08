load("sbbsdefs.js");
load("hotline_funcs.js");

var logged_in = false;

var STRING = 0;
var INTEGER = 1;
var BINARY = 2;
var SHORT = 3;
var LONG = 4;
var usr;
var usrs=[];
var icon = 412;
var flags = 0;

var DELETE_FILE=0;
var UPLOAD_FILE=1;
var DOWNLOAD_FILE=2;
var RENAME_FILE=3;
var MOVE_FILE=4;
var CREATE_FOLDER=5;
var DELETE_FOLDER=6;
var RENAME_FOLDER=7;
var MOVE_FOLDER=8;
var READ_CHAT=9;
var SEND_CHAT=10;
var OPEN_CHAT=11;
var CLOSE_CHAT=12;
var SHOW_IN_LIST=13;
var CREATE_USER=14;
var DELETE_USER=15;
var OPEN_USER=16;
var MODIFY_USER=17;
var CHANGE_PASSWORD=18;
var SEND_PRIVATE_MESSAGE=19;
var NEWS_READ_ARTICLE=20;
var NEWS_POST_ARTICLE=21;
var DISCONNECT_USER=22;
var CANNOT_BE_DISCONNECTED=23;
var GET_CLIENT_INFO=24;
var UPLOAD_ANYWHERE=25;
var ANY_NAME=26;
var NO_AGREEMENT=27;
var SET_FILE_COMMENT=28;
var SET_FOLDER_COMMENT=29;
var VIEW_DROP_BOXES=30;
var MAKE_ALIAS=31;
var BROADCAST=32;
var NEWS_DELETE_ARTICLE=33;
var NEWS_CREATE_CATEGORY=34;
var NEWS_DELETE_CATEGORY=35;
var NEWS_CREATE_FOLDER=36;
var NEWS_DELETE_FILDER=37;
var UPLOAD_FOLDER=38;
var DOWNLOAD_FOLDER=39;
var SEND_MESSAGE=40;
var FAKE_RED=41;
var AWAY=42;
var CHANGE_NICK=43;
var CHANGE_ICON=44;
var SPEAK_BEFORE=45;
var REFUSE_CHAT=46;
var BLOCK_DOWNLOAD=47;
var VISIBLE=48;
var CAN_VIEW_INVISIBLE=49;
var CAN_FLOOD=50;
var VIEW_OWN_DROPBOX=51;
var DONT_QUEUE=52;
var ADMIN_IN_SPECTOR=53;
var POST_BEFORE=54;

var params = {
	100:{type:STRING, name:"Error Message"},
	101:{type:BINARY, name:"Data"},
	102:{type:STRING, name:"User Name"},
	103:{type:INTEGER, name:"User ID"},
	104:{type:INTEGER, name:"User Icon ID"},
	105:{type:STRING, name:"User Login"},
	106:{type:STRING, name:"User Password"},
	107:{type:INTEGER, name:"Reference Number"},
	108:{type:INTEGER, name:"Transfer Size"},
	109:{type:INTEGER, name:"Chat Options"},
	110:{type:BINARY, name:"User Access"},
	112:{type:INTEGER, name:"User Flags"},
	113:{type:INTEGER, name:"Options"},
	114:{type:INTEGER, name:"Chat ID"},
	116:{type:INTEGER, name:"Waiting Count"},
	151:{type:BINARY, name:"Server Banner"},
	152:{type:INTEGER, name:"Server Banner Type"},
	153:{type:BINARY, name:"Server Banner URL"},
	154:{type:INTEGER, name:"No Server Agreement"},
	160:{type:INTEGER, name:"Version"},
	161:{type:INTEGER, name:"Banner ID"},
	162:{type:STRING, name:"Server Name"},
	200:{type:BINARY, name:"File Name With Info"},
	201:{type:STRING, name:"File Name"},
	202:{type:BINARY, namme:"File Path"},
	203:{type:BINARY, name:"File Resume Data"},
	204:{type:INTEGER, name:"File Transfer Options"},
	207:{type:INTEGER, name:"File Size"},
	215:{type:STRING, name:"Automatic Response"},
	300:{type:BINARY, name:"User Name With Info", struct:[{type:SHORT, name:'uid'}, {type:SHORT, name:'iid'}, {type:SHORT, name:'flags'}, {type:SHORT, name:'nlen'}, {type:STRING, name:'name'}]},
	321:{type:BINARY, name:"News Article List Data"},
	323:{type:BINARY, name:"News Category List Data"},
	325:{type:BINARY, name:"News Path"},
	326:{type:INTEGER, name:"News Article ID"},
	327:{type:STRING, name:"News Article Data Flavour"},
	328:{type:STRING, name:"News Article Title"},
	329:{type:STRING, name:"News Article Poster"},
	330:{type:BINARY, name:"News Article Date"},
	331:{type:INTEGER, name:"Previous Article ID"},
	332:{type:INTEGER, name:"Next Article ID"},
	333:{type:STRING, name:"News Article Data"},
	335:{type:INTEGER, name:"Parent Article ID"},
	336:{type:INTEGER, name:"First Child ID"},
};

var client_version = 0;
var next_id = 1;

function add_privs(privs, ret)
{
	var i,k;
	if (ret == undefined)
		ret = '\x00\x00\x00\x00\x00\x00\x00\x00';
	while(ret.length < 8)
		ret += '\x00';

	for (i in privs) {
		k=ascii(ret.substr(Math.floor(privs[i]/8), 1));
		k |= 1<<(7-(privs[i] % 8));
		ret = ret.substr(0, Math.floor(privs[i]/8)) + ascii(k) + ret.substr(Math.floor(privs[i]/8)+1);
	}
	if (ret.length != 8)
		rage_quit("Unable to encode privs!");
	var dbg='';
	for (i=0; i<ret.length; i++) {
		dbg += format("%02x", ascii(ret.substr(i, 1)));
	}
	return ret;
}

function get_param_type(id)
{
	if (params[id] == undefined)
		return BINARY;
	return params[id].type;
}

function update_data()
{
	var i, j;
	var found;
	var params;
	var new_usrs = read_online_users();

	var msg = system.get_telegram(usr.number);
	if (msg != null) {
		params=[];
		msg = msg.replace(/\x01./g, '');
		msg = msg.replace(/\r\n/g, '\r');
		msg = msg.replace(/\n/g, '\r');
		msg = msg.replace(/\t/g, ' ');
		msg = msg.replace(/[\x00-\x0C\x0E-\x1f]/g, '');
		var m = msg.match(/^Hotline message from ([^:]+):\r([\x00-\xff]*)$/);
		if (m != null) {
			params.push({id:102, value:m[1]});
			var alias = m[1];
			msg = m[2];
			for (i in new_usrs) {
				if (alias == new_usrs[i].alias && new_usrs[i].id < 0x8000)
					params.push({id:103, value:new_usrs[i].id});
			}
		}
		params.push({id:101, value:msg});
		send_message(104, params);
	}

	var f = new File(system.temp_dir+'hotline.chat.'+usr.number);
	if (f.open("r+b")) {
		var cmsgs = f.readAll(5120);
		f.rewind();
		f.truncate();
		f.close();

		for (i in cmsgs) {
			m = cmsgs[i].match(/^([0-9]+) ([0-9]+) ([\x00-\xff]*)$/);
			if (m != null) {
				var alias = new User(parseInt(m[2], 10)).alias;
				params = [{id:101, value:'\r'+alias+': '+m[3]}, {id:103, value:parseInt(m[2])}];
				var chatid = parseInt(m[1], 10);
				if (chatid)
					params.push({id:114, value:chatid});
				send_message(106, params);
			}
		}
	}

	for (i in new_usrs) {
		/*
		 * For each user in new_usrs, check if it's in usrs and update/notify as needed.
		 * Mark matched ones as such so unmatched ones can be notified of deletion
		 */
		if (new_usrs[i].id == usr.number)
			continue;
		found = false;
		if (!new_usrs[i].id)
			continue;
		for (j in usrs) {
			if (usrs[j].id == new_usrs[i].id) {
				found = true;
				usrs[j].matched=true;
				if (usrs[j].icon != new_usrs[i].icon || usrs[j].flags != new_usrs[i].flags)
					send_message(301, [{id:103, value:new_usrs[i].id}, {id:104, value:new_usrs[i].icon}, {id:112, value:new_usrs[i].flags}, {id:102, value:new_usrs[i].alias}]);
			}
		}
		if (!found)
			send_message(301, [{id:103, value:new_usrs[i].id}, {id:104, value:new_usrs[i].icon}, {id:112, value:new_usrs[i].flags}, {id:102, value:new_usrs[i].alias}]);
	}
	for (i in usrs) {
		if (usrs[i].id == usr.number)
			continue;
		if (!usrs[i].matched) {
			if (usrs[i].id)
				send_message(302, [{id:103, value:usrs[i].id}]);
		}
	}
	usrs = new_usrs;
}

function read_msg(sock)
{
	var i;
	var msg={};

	msg.hdr={};

	while(sock.is_connected) {
		if(logged_in)
			update_data();
		if (sock.poll(0.25))
			break;
	}

	msg.hdr.flags = sock.recvBin(1);
	msg.hdr.is_reply = sock.recvBin(1);
	msg.hdr.type = sock.recvBin(2);
log(LOG_DEBUG, "Type: "+msg.hdr.type);
	msg.hdr.id = sock.recvBin(4);
	msg.hdr.err = sock.recvBin(4);
	msg.hdr.total_size = sock.recvBin(4);
	msg.hdr.data_size = sock.recvBin(4);
	msg.param_count = sock.recvBin(2);
log(LOG_DEBUG, "Params: "+msg.param_count);
	msg.params={};
	for (i=0; i<msg.param_count; i++) {
		var id = sock.recvBin(2);
		var param;

log(LOG_DEBUG, "Getting parameter ID: "+id);
		if (msg.params[id] != undefined) {
			if (!msg.params[id].isArray())
				msg.params[id] = [msg.params[id]];
			msg.params[id].push({});
			param = msg.params[id][msg.params[id].length-1];
		}
		else {
			msg.params[id]={};
			param = msg.params[id];
		}
		param.size = sock.recvBin(2);
		if (get_param_type(id)==INTEGER)
			param.data = sock.recvBin(param.size);
		else
			param.data = sock.recv(param.size);
	}
	return msg;
}

function rage_quit(reason)
{
	log(LOG_DEBUG, "Rage quit: "+reason);
	if (usr != undefined)
		remove_online_user(usr.alias);
	client.socket.close();
	exit();
}

function send_message(type, args, is_reply, id, error)
{
	var outbuf='';
	var i;
	var tmp;

	if (is_reply === undefined)
		is_reply = false;

	if (id === undefined) {
		if (is_reply)
			rage_quit("Reply with no ID specified!");
		id = next_id;
	}

	if (error == undefined)
		error = 0;

log(LOG_DEBUG, "Message type: "+ type);
	for (i=0; i<args.length; i++) {
		outbuf += encode_integer(args[i].id);
if (args[i].id == 100 && error)
log(LOG_DEBUG, "Sending error: "+args[i].value);
else
log(LOG_DEBUG, "Parameter: "+args[i].id);
		switch (get_param_type(args[i].id)) {
			case BINARY:
			case STRING:
				tmp = args[i].value;
				break;
			case INTEGER:
				tmp = encode_integer(args[i].value);
				break;
		}
		outbuf += encode_integer(tmp.length);
		outbuf += tmp;
	}
	outbuf = encode_short(args.length) + outbuf;
	tmp = outbuf.length;
	outbuf = encode_long(tmp) + outbuf;	// Data size
	outbuf = encode_long(tmp) + outbuf;	// Total size
	outbuf = encode_long(error) + outbuf;	// Error
	outbuf = encode_long(id) + outbuf;	// Message ID
	outbuf = encode_short(type) + outbuf;	// Type
	outbuf = ascii(is_reply?1:0) + outbuf;	// Is Reply
	outbuf = ascii(0) + outbuf;	// Flags
	client.socket.send(outbuf);
	if (!is_reply) {
		next_id++;
		if (next_id == 0 || next_id >= 1<<32)
			next_id = 1;
	}
}

function send_response(hdr, args, error)
{
	return send_message(0, args, true, hdr.id, error);
}

function mb_from_path(path)
{
	var i,j,k;
	if (path != undefined) {
		var depth = decode_integer(path.data.substr(0,2));

		if (depth == 2) {
			var grplen = decode_integer(path.data.substr(4,1));
			var grpdesc = path.data.substr(5, grplen);
			var sublen = decode_integer(path.data.substr(7+grplen,1));
			var subdesc = path.data.substr(8+grplen, sublen);
			var mb;
			for (i in msg_area.grp_list) {
				if (!usr.compare_ars(msg_area.grp_list[i].ars))
					continue;
				if (msg_area.grp_list[i].description == grpdesc) {
					for (j in msg_area.grp_list[i].sub_list) {
						if (!usr.compare_ars(msg_area.grp_list[i].sub_list[j].ars))
							continue;
						if (!usr.compare_ars(msg_area.grp_list[i].sub_list[j].read_ars))
							continue;
						if (msg_area.grp_list[i].sub_list[j].description == subdesc) {
							mb = new MsgBase(msg_area.grp_list[i].sub_list[j].code);
							if (!mb.open())
								rage_quit("Unable to open message base!");
							return mb;
						}
					}
				}
			}
		}
	}
	return undefined;
}

function get_sub(path)
{
	var i, j;
	var liblen = decode_integer(path.data.substr(4,1));
	var libdesc = path.data.substr(5, liblen);
	var dirlen = decode_integer(path.data.substr(7+liblen, 1));
	var dirdesc = path.data.substr(8+liblen, dirlen);

	for (i in file_area.lib_list) {
		if (!usr.compare_ars(file_area.lib_list[i].ars))
			continue;
		if (file_area.lib_list[i].description == libdesc) {
			for (j in file_area.lib_list[i].dir_list) {
				if (!usr.compare_ars(file_area.lib_list[i].dir_list[j].ars))
					continue;
				if (file_area.lib_list[i].dir_list[j].description == dirdesc)
					return file_area.lib_list[i].dir_list[j];
			}
		}
	}
	return undefined;
}

function get_path(path)
{
	var sub = get_sub(path);
	if (sub != undefined)
		return backslash(sub.path);
	return undefined;
}

function setup_transfer(path)
{
	// Generate random number, ensure it's unique, and append.
	var f = new File(system.temp_dir+'hotline.transfers');
	var ret=undefined;
	var i;

	if (f.open("a+b")) {
		f.rewind();
		var lines=f.readAll(2048);

		while(ret == undefined) {
			ret = random(2147483647);
			for(i in lines) {
				m = lines[i].match(/^([0-9]+) /);
				if (m != null) {
					if (parseInt(m[1], 10) == ret) {
						ret = undefined;
						break;
					}
				}
			}
		}
		f.writeln(ret + ' ' + client.ip_address + ' ' + time() + ' ' + path);
		f.close();
	}
	return ret;
}

function parse_user_line(line)
{
	var ret = {};
	var m = line.match(/^(.*) ([0-9]+) ([0-9]+)$/);
	if (m != null) {
		ret.alias = m[1];
		ret.icon = parseInt(m[2], 10);
		ret.flags = parseInt(m[3], 10);
		ret.id = system.matchuser(ret.alias);
	}
	return ret;
}

function update_online_user(username, icon, flags)
{
	var i;
	var found = false;
	var f = new File(system.temp_dir+'hotline.users');
	if (icon == undefined)
		icon = 412;
	if (flags == undefined)
		flags = 0;

	if (f.open(f.exists ? 'r+b':'ew+b')) {
		var lines = f.readAll(2048);

		for (i in lines) {
			var u=parse_user_line(lines[i]);
			if(u.id & 0x8000)
				continue;
			if (u.alias == username) {
				found = true;
				lines[i] = username+' '+icon+' '+flags;
			}
		}
		if (!found)
			lines.push(username+' '+icon+' '+flags);
		f.rewind();
		f.truncate();
		f.writeAll(lines);
		f.close();
		return;
	}
	rage_quit("Unable to open user file!");
}

function read_online_users()
{
	var ret = [];
	var i;
	var f = new File(system.temp_dir+'hotline.users');

	if (f.open('rb')) {
		var lines = f.readAll(2048);
		f.close();

		for (i in lines) {
			ret.push(parse_user_line(lines[i]));
		}
	}

	// Now add users who are on the BBS
	for (i in system.node_list) {
		if ((system.node_list[i].status == NODE_INUSE) || (usr.is_sysop && system.node_list[i].status == NODE_QUIET)) {
			var uo = new User(system.node_list[i].useron);
			if (uo != null) {
				var usro = {node:i, alias:uo.alias, icon:412, flags:0x01};
				if (uo.is_sysop)
					usro.flags |= 0x02;
				usro.id = uo.number | 0x8000; 
				ret.push(usro);
			}
		}
	}
	return ret;
}

function remove_online_user(username)
{
	var i;
	var f = new File(system.temp_dir+'hotline.users');
	found = false;

	if (f.open('r+b')) {
		var lines = f.readAll(2048);
		for (i = 0; i<lines.length; i++) {
			var u = parse_user_line(lines[i]);
			if(u.id & 0x8000)
				continue;
			if (u.alias == username) {
				found = true;
				lines.splice(i, 1);
				i--;
			}
		}
		if (found) {
			f.rewind();
			f.truncate();
			f.writeAll(lines);
		}
		f.close();
	}
	file_remove(system.temp_dir+'hotline.chat.'+usr.number);
}

// TODO: modopts.ini thingie.
var include_real_name = false;
var include_age_gender = false;
var agreement_file = undefined;
function get_user_info_str(u)
{
	var ret = '';
	ret += "User: "+u.alias+' #'+u.number+'\r';
	if(include_real_name)
		ret += "In real life: "+u.name+'\r';

	ret += "From: "+u.location+'\r';
	ret += "Handle: "+u.handle+'\r';
	if(include_age_gender) {
		ret += 'Birth: '+u.birthdate+' (Age: '+u.age+' years)\r';
		ret += 'Gender: '+u.gender+'\r';
	}
	ret += "Shell: "+u.command_shell+'\r';
	ret += "Editor: "+u.editor+"\r";
	ret += format("Last login %s %s\rvia %s from %s [%s]\r"
			  ,system.timestr(u.stats.laston_date)
			  ,system.zonestr()
			  ,u.connection
			  ,u.host_name
			  ,u.ip_address);
	var plan;
	plan=format("%suser/%04d.plan",system.data_dir,u.number);
	if(file_exists(plan)) {
		var pf = new File(plan);
		if (pf.open("rb", true)) {
			ret += 'Plan:\r'+pf.readAll(2048).join('\r');
			pf.close();
		}
	}
	else
		ret += "No plan.\r";
	return ret.substr(0, 65535);
}

function send_privs()
{
	var tmp;
	tmp = add_privs([UPLOAD_FILE, DOWNLOAD_FILE, READ_CHAT, SEND_CHAT, OPEN_CHAT, CLOSE_CHAT, SHOW_IN_LIST, OPEN_USER, CHANGE_PASSWORD, SEND_PRIVATE_MESSAGE, NEWS_READ_ARTICLE, NEWS_POST_ARTICLE, GET_CLIENT_INFO, NO_AGREEMENT, SET_FILE_COMMENT, BROADCAST, DOWNLOAD_FOLDER, SEND_MESSAGE, CHANGE_ICON, REFUSE_CHAT, VISIBLE]);
	if (usr.is_sysop)
		tmp = add_privs([DELETE_FILE, RENAME_FILE, MOVE_FILE, RENAME_FOLDER, DELETE_USER, MODIFY_USER, DISCONNECT_USER, CANNOT_BE_DISCONNECTED, SET_FOLDER_COMMENT, NEWS_DELETE_ARTICLE, CAN_VIEW_INVISIBLE], tmp);
	send_message(354, [{id:110, value:tmp}]);
}

function get_chat_users(id)
{
	var ret = [];
	var i;

	if (id == 0) {
		var new_usrs = read_online_users();
		for (i in new_usrs) {
			if (new_usrs[i].id & 0x8000)
				continue;
			ret.push(new_usrs[i].id);
		}
	}
	return ret;
}

function send_chat_msg(id, opt, msg)
{
	var i;
	var ids = get_chat_users(id);
	var f;

	for (i in ids) {
		f = new File(system.temp_dir+'hotline.chat.'+ids[i]);
		if (f.open('ab')) {
			f.writeln(id+' '+usr.number+' '+msg);
			f.close();
		}
	}
}

function handle_message(msg)
{
	var i, j, k;

	switch(msg.hdr.type) {
		case 101:	// Get Message...
			var f=new File(system.text_dir+"welcome.msg");
			var message = "Welcome to "+system.name;
			if(f.open("rb",true)) {
				message = f.read();
				f.close();
				message=strip_exascii(message).replace(/@([^@]*)@/g,
					function(matched, code) {
						var fmt="%s";
						ma=new Array();
						if((ma=code.match(/^(.*)-L.*$/))!=undefined) {
							fmt="%-"+(code.length)+"s";
							code=ma[1];
						}
						if((ma=code.match(/^(.*)-R.*$/))!=undefined) {
							fmt="%"+(code.length)+"s";
							code=ma[1];
						}
						switch(code.toUpperCase()) {
							case 'BBS':
								return(format(fmt,system.name.toString()));
							case 'LOCATION':
								return(format(fmt,system.location.toString()));
							case 'SYSOP':
								return(format(fmt,system.operator.toString()));
							case 'HOSTNAME':
								return(format(fmt,system.host_name.toString()));
							case 'OS_VER':
								return(format(fmt,system.os_version.toString()));
							case 'UPTIME':
								var days=0;
								var hours=0;
								var min=0;
								var seconds=0;
								var ut=time()-system.uptime;
								days=(ut/86400);
								ut%=86400;
								hours=(ut/3600);
								ut%=3600;
								mins=(ut/60);
								secsonds=parseInt(ut%60);
								if(parseInt(days)!=0)
									ut=format("%d days %d:%02d",days,hours,mins);
								else
									ut=format("%d:%02d",hours,mins);
								return(format(fmt,ut.toString()));
							case 'TUSER':
								return(format(fmt,system.stats.total_users.toString()));
							case 'STATS.NUSERS':
								return(format(fmt,system.stats.new_users_today.toString()));
							case 'STATS.LOGONS':
								return(format(fmt,system.stats.total_logons.toString()));
							case 'STATS.LTODAY':
								return(format(fmt,system.stats.logons_today.toString()));
							case 'STATS.TIMEON':
								return(format(fmt,system.stats.total_timeon.toString()));
							case 'STATS.TTODAY':
								return(format(fmt,system.stats.timeon_today.toString()));
							case 'TMSG':
								return(format(fmt,system.stats.total_messages.toString()));
							case 'STATS.PTODAY':
								return(format(fmt,system.stats.messages_posted_today.toString()));
							case 'MAILW:0':
								return(format(fmt,system.stats.total_email.toString()));
							case 'STATS.ETODAY':
								return(format(fmt,system.stats.email_sent_today.toString()));
							case 'MAILW:1':
								return(format(fmt,system.stats.total_feedback.toString()));
							case 'STATS.FTODAY':
								return(format(fmt,system.stats.feedback_sent_today.toString()));
							case 'TFILE':
								return(format(fmt,system.stats.total_files.toString()));
							case 'STATS.ULS':
								return(format(fmt,system.stats.files_uploaded_today.toString()));
							case 'STATS.DLS':
								return(format(fmt,system.stats.files_downloaded_today.toString()));
							case 'STATS.DLB':
								return(format(fmt,system.stats.bytes_downloaded_today.toString()));
							default:
								return('@'+code+'@');
						}
					});
			}
			send_response(msg.hdr, [{id:101, value:message}]);
			break;
		case 105:	// Send Chat
			var chatid = 0;
			var chatstr = '';
			var chatopt = 0;

			if (msg.params[114] != undefined)
				chatid = msg.params[114].data;
			if (msg.params[101] != undefined)
				chatstr = msg.params[101].data;
			if (msg.params[109] != undefined)
				chatopt = msg.params[109].data;
			send_chat_msg(chatid, chatopt, chatstr);
			break;
		case 107:	// Login
			if (msg.params[105] == undefined || msg.params[106] == undefined) {
				send_response(msg.hdr, [{id:100, value:"Incorrect parameters to login"}], 1);
				break;
			}
			if (msg.params[160] != undefined)
				client_version = msg.params[160].data;
			if (msg.params[104] != undefined)
				icon = msg.params[104].data;
			var uid = system.matchuser(decode_string(msg.params[105].data));
			if (uid == 0) {
				send_response(msg.hdr, [{id:100, value:"No such user"}], 1);
				break;
			}
			usr = new User(uid);
			if (usr == null) {
				send_response(msg.hdr, [{id:100, value:"No such user"}], 1);
				break;
			}
			if (!(usr.compare_ars('REST G'))) {
				if (usr.security.password.toLowerCase() != decode_string(msg.params[106].data).toLowerCase()) {
					send_response(msg.hdr, [{id:100, value:"Incorrect password"}], 1);
					break;
				}
			}
			usrs = read_online_users();
			j = false;
			for (i in usrs) {
				if (usrs[i].id == usr.id) {
					j = true;
					send_response(msg.hdr, [{id:100, value:"You are already logged in!"}], 1);
					usr = undefined;
					break;
				}
			}
			if (j)
				break;
			update_online_user(usr.alias, icon, flags);
			send_response(msg.hdr, [{id:160, value:151},{id:161, value:0},{id:162, value:system.name}]);
			// TODO: send banner...
			if (agreement_file == null) {
				send_message(109, [{id:154, value:1}]);
				update_online_user(usr.alias, icon, flags);
				send_privs();
				logged_in = true;
			}
			else {
				var agreementf = new File(agreement_file);
				if (f.open("rb", true)) {
					var tmp = f.read();
					f.close();
					send_message(109, [{id:101, value:tmp}]);
				}
				else {
					rage_quit("Unable to open agreement file");
				}
			}
			break;
		case 108:
			var userid = msg.params[103].data;
			if (userid == undefined) {
				send_response(msg.hdr, [{id:100, value:'No User ID Specified'}], 1);
				break;
			}
			if (msg.params[101] != undefined) {
				if (userid & 0x8000) {
					var new_usrs = read_online_users();
					for (i in new_usrs) {
						if (new_usrs[i].id == userid)
							system.put_node_message(new_usrs[i].node, "\1n\1b\1r\7Hotline \1n\1gmessage from \1n\1h"+usr.alias+":\r\n\1h"+msg.params[101].data);
					}
				}
				else
					system.put_telegram(userid, "\1n\1b\1r\7Hotline \1n\1gmessage from \1n\1h"+usr.alias+":\r\n\1h"+msg.params[101].data);
				send_response(msg.hdr, []);
			}
		case 121:	// Accept agreement...
			send_response(msg.hdr, []);
			if (msg.params[104] != undefined) {
				icon = parseInt(msg.params[104].data, 10);
			}
			if (msg.params[113] != undefined) {
				flags = parseInt(msg.params[113].data, 10);
				flags &= 0x0c;
				if(usr.is_sysop)
					flags |= 0x02;
			}
			update_online_user(usr.alias, icon, flags);
			send_privs();
			logged_in = true;
			break;
		case 200:	// File list
			var tmp=[];
			if (msg.params[202] == undefined || decode_integer(msg.params[202].data.substr(0,2))==0) {
				// Root
				for (i in file_area.lib_list) {
					if (!usr.compare_ars(file_area.lib_list[i].ars))
						continue;
					tmp.push({id:200, value:'fldr\x00\x00\x00\x01'+encode_long(0)+encode_long(file_area.lib_list[i].dir_list.length)+encode_short(0)+encode_short(file_area.lib_list[i].description.length)+file_area.lib_list[i].description});
				}
			}
			else {
				var depth = decode_integer(msg.params[202].data.substr(0,2));
				if (depth == 1) {
					// DIRs...
					var liblen = decode_integer(msg.params[202].data.substr(4,1));
					var libdesc = msg.params[202].data.substr(5, liblen);

					k = false;
					for (i in file_area.lib_list) {
						if (!usr.compare_ars(file_area.lib_list[i].ars))
							continue;
						if (file_area.lib_list[i].description == libdesc) {
							k = true;
							for (j in file_area.lib_list[i].dir_list) {
								if (!usr.compare_ars(file_area.lib_list[i].dir_list[j].ars))
									continue;
								tmp.push({id:200, value:'fldr\x00\x00\x00\x01'+encode_long(0)+encode_long(0)+encode_short(0)+encode_short(file_area.lib_list[i].dir_list[j].description.length)+file_area.lib_list[i].dir_list[j].description});
							}
						}
					}
					if (!k) {
						send_response(msg.hdr, [{id:100, value:'No such directory'}], 1);
						break;
					}
				}
				else {
					if (depth != 2) {
						send_response(msg.hdr, [{id:100, value:'No such directory'}], 1);
						break;
					}
					var path = get_path(msg.params[202]);
					if (path == undefined) {
						send_response(msg.hdr, [{id:100, value:'No such directory'}], 1);
						break;
					}
					var files = directory(path+'*');
					for (i in files) {
						var fname = file_getname(files[i]);
						tmp.push({id:200, value:'file\x00\x00\x00\x01'+encode_long(file_size(files[i]))+encode_long(0)+encode_short(0)+encode_short(fname.length)+fname});
					}
				}
			}
			send_response(msg.hdr, tmp);
			break;
		case 202:	// File download... pass off to transfer thingie.
			var sub = get_sub(msg.params[202]);
			if (sub == undefined) {
				send_response(msg.hdr, [{id:100, value:'No such directory'}], 1);
				break;
			}
			if (!usr.compare_ars(sub.download_ars)) {
				send_response(msg.hdr, [{id:100, value:'Permission denied'}], 1);
				break;
			}
			var path = sub.path;
			path += file_getname(msg.params[201].data);
			if (!file_exists(path)) {
				send_response(msg.hdr, [{id:100, value:'No such file'}], 1);
				break;
			}
			if (msg.params[203] != undefined) {
				send_response(msg.hdr, [{id:100, value:'Resume not supported'}], 1);
				break;
			}
			var tid = setup_transfer(path);
			var tcount = 0;
			send_response(msg.hdr, [{id:108, value:file_size(path)+122}, {id:207, value:file_size(path)}, {id:107, value:tid}, /*{id:116, value:tcount}*/]);
			break;
		case 300:
			usrs = read_online_users();
			var tmp = [];
			for (i in usrs) {
				if (usrs[i].id)
					tmp.push({id:300, value:encode_short(usrs[i].id)+encode_short(usrs[i].icon)+encode_short(usrs[i].flags)+encode_short(usrs[i].alias.length)+usrs[i].alias});
			}
			send_response(msg.hdr, tmp);
			break;
		case 303:
			if (msg.params[103] != undefined) {
				var tmp = new User(msg.params[103].data & 0x7fff);
				if (tmp != null)
					send_response(msg.hdr, [{id:102, value:tmp.alias}, {id:101, value:get_user_info_str(tmp)}]);
				else
					send_response(msg.hdr, [{id:100, value:"Can't find user #"+msg.params[103].data}], 1);
			}
			else
				send_response(msg.hdr, [{id:100, value:"User number not specified"}], 1);
			break;
		case 304:	// TODO: "Automatic response"
			if (msg.params[104] != undefined) {
				icon = parseInt(msg.params[104].data, 10);
			}
			if (msg.params[113] != undefined) {
				flags = parseInt(msg.params[113].data, 10);
			}
			update_online_user(usr.alias, icon, flags);
			break;
		case 370:	// Get News Category Name List
			var tmp = [];
			if (msg.params[325] != undefined && decode_integer(msg.params[325].data.substr(0,2)) > 0) {
				var depth = decode_integer(msg.params[325].data.substr(0,2));

				if (depth == 1) {
					var grplen = decode_integer(msg.params[325].data.substr(4,1));
					var grpdesc = msg.params[325].data.substr(5, grplen);

					for (i in msg_area.grp_list) {
						if (!usr.compare_ars(msg_area.grp_list[i].ars))
							continue;
						if (msg_area.grp_list[i].description == grpdesc) {
							for (j in msg_area.grp_list[i].sub_list) {
								if (!usr.compare_ars(msg_area.grp_list[i].sub_list[j].ars))
									continue;
								var mb = new MsgBase(msg_area.grp_list[i].sub_list[j].code);
								if (!mb.open()) {
									log(LOG_ERR, "Unable to open message base "+msg_area.grp_list[i].sub_list[j].code);
									continue;
								}
								tmp.push({id:323, value:"\x00\x03"+encode_short(mb.total_msgs)+'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'+encode_long(0)+encode_long(0)+ascii(mb.cfg.description.length)+mb.cfg.description});
								mb.close();
							}
						}
					}
				}
				else {
					send_response(msg.hdr, [{id:100, value:"Invalid path"}], 1);
					break;
				}
			}
			else {
				for (i in msg_area.grp_list) {
					if (!usr.compare_ars(msg_area.grp_list[i].ars))
						continue;
					tmp.push({id:323, value:"\x00\x02"+encode_short(msg_area.grp_list[i].sub_list.length)+ascii(msg_area.grp_list[i].description.length)+msg_area.grp_list[i].description});
				}
			}
			send_response(msg.hdr, tmp);
			break;
		case 371:	// Get article list
			if (msg.params[325] != undefined) {
				var items={count:0};
				var mb = mb_from_path(msg.params[325]);
				if (mb == undefined) {
					send_response(msg.hdr, [{id:100, value:"Invalid path"}], 1);
					break;
				}
				var hdrs = mb.get_all_msg_headers();
				var msgs = '';
				for (i in hdrs) {
					if (hdrs[i].attr & MSG_DELETE)
						continue;
					if ((hdrs[i].attr & (MSG_MODERATED|MSG_VALIDATED)) == MSG_MODERATED)
						continue;
					if ((hdrs[i].attr & MSG_PRIVATE) && (hdrs[i].to_ext != usr.number))
						continue;
					items.count++;
					// TODO: Attachments etc.
					var body = mb.get_msg_body(parseInt(i, 10), true);
					if (body == null)
						continue;
					if (body.length > 65535 || body.length < 1)	// Message too long
						continue;
					if (hdrs[i].subject.length > 255)	// Subject too long
						continue;
					if (hdrs[i].subject.length < 1)	// Subject too short
						continue;
					if (hdrs[i].from.length < 1 || hdrs[i].from.length > 255)	// From too long
						continue;
					// TODO: Encode the flags...
					msgs += encode_long(hdrs[i].number)+time_to_timestamp(hdrs[i].when_written_time)+encode_long(hdrs[i].thread_back)+encode_long(0)+encode_short(1)+ascii(hdrs[i].subject.length)+hdrs[i].subject+ascii(hdrs[i].from.length)+hdrs[i].from+ascii("text/plain".length)+"text/plain"+encode_short(body.length);
				}
				hdrs={};
				mb.close();
				var overhead = 26+4+4+1+mb.cfg.name.length+1+mb.cfg.description.length;
				while (overhead + msgs.length > 65535) {
					var sublen=ascii(msgs.substr(22,1));
					var fromlen=ascii(msgs.substr(23+sublen,1));
					msgs=msgs.substr(37+sublen+fromlen);
					items.count--;
				}
				send_response(msg.hdr, [{id:321, value:encode_long(0)+encode_long(items.count)+ascii(mb.cfg.name.length)+mb.cfg.name+ascii(mb.cfg.description.length)+mb.cfg.description+msgs}]);
			}
			else {
				send_response(msg.hdr, [{id:100, value:"No path specified!"}], 1)
			}
			break;
		case 400:	// Get message
			var mb = mb_from_path(msg.params[325]);
			if (mb == undefined) {
				send_response(msg.hdr, [{id:100, value:"Invalid path"}], 1);
				break;
			}
			var hdr = mb.get_msg_header(msg.params[326].data);
			if (hdr == undefined) {
				send_response(msg.hdr, [{id:100, value:"Message ID"}], 1);
				break;
			}
			var body = mb.get_msg_body(hdr.number, true);
			if (body == undefined) {
				send_response(msg.hdr, [{id:100, value:"Message ID"}], 1);
				break;
			}
			// TODO (?) update last-read pointer...
			send_response(msg.hdr, [{id:328, value:hdr.subject}, {id:329, value:hdr.from}, {id:330, value:time_to_timestamp(hdr.when_written_time)}, {id:331, value:msg.params[326].data-1}, {id:332, value:msg.params[326]+1}, {id:335, value:hdr.thread_back}, {id:336, value:hdr.thread_first}, {id:327, value:"text/plain"}, {id:333, value:body}]);
			break;
		default:
			send_response(msg.hdr, [{id:100, value:"Unsupported command"}], 1);
			break;
	}
}

function decode_string(str)
{
	var i;
	var ret='';

	for (i=0; i<str.length; i++) {
		ret += ascii(ascii(str.substr(i, 1)) ^ 255);
	}
	return ret;
}

function time_to_timestamp(time)
{
	var ret ='';
	var d = new Date(time*1000);
	var y = new Date(time*1000);

	y.setMonth(0);
	y.setDate(1);
	y.setHours(0);
	y.setMinutes(0);
	y.setSeconds(0);
	y.setMilliseconds(0);
	ret = encode_short(d.getYear()+1900);
	ret += encode_short(0);
	ret += encode_long((d.valueOf()-y.valueOf())/1000);
	return ret;
}

log(LOG_DEBUG, "Connected!");
var ver = client.socket.recvBin();
if (ver != 0x54525450)
	rage_quit(format("Invalid version %08x", ver));
var subprot = client.socket.recvBin();
var version = client.socket.recvBin(2);
var subver = client.socket.recvBin(2);
log(LOG_DEBUG, "Ver="+ver+"-"+subprot+", "+version+"-"+subver);
client.socket.send("TRTP\0\0\0\0");
while(client.socket.is_connected) {
	var msg = read_msg(client.socket);
	if ((!logged_in) && msg.hdr.type != 107 && msg.hdr.type != 121) {
		send_response(msg.hdr, [{id:100, value:"Not logged in!"}]);
		continue;
	}
	handle_message(msg);
}
if (usr != undefined)
	remove_online_user(usr.alias);
log(LOG_DEBUG, "Done!");
