// sbbsimsg_lib.js

// Synchronet inter-bbs instant message library

// $Id: sbbsimsg_lib.js,v 1.6 2019/06/15 03:07:56 rswindell Exp $

require("sockdefs.js", 'SOCK_DGRAM');

const SentAddressHistoryLength = 10;
const props_sent = "imsg sent";
const props_recv = "imsg received";

// Read the list of systems into list array
var filename = system.ctrl_dir + "sbbsimsg.lst";
var sys_list = {};
var sock = new Socket(SOCK_DGRAM);

function read_sys_list(include_self)
{
	var f = new File(filename);
	if(!f.open("r")) {
		return null;
	}

	var list = f.readAll();
	f.close();
	for(var i in list) {
		if(list[i]==null)
			break;
		var line = list[i].trimLeft();
		if(line.charAt(0)==';')		// comment? 
			continue;

		var word = line.split('\t');
		var host = word[0].trimRight();
		var ip_addr = word[1];
		var name = word[2];

		if(!include_self) {
			if(host == system.host_name
				|| host == system.inetaddr)		// local system?
				continue;						// ignore
	//		if(js.global.client && ip_addr == client.socket.local_ip_address)
	//			continue;
			if(js.global.server && ip_addr == server.interface_ip_address)
				continue;
		}

		this.sys_list[ip_addr] = { host: host, name: name, users: [] };
	}
	
	return this.sys_list;
}

function request_active_users()
{
	var requests_sent = 0;
	for(var i in sys_list) {
		if(!sock.sendto("?active-users.json\r\n", i, IPPORT_SYSTAT))	// Get list of active users (in JSON fmt, if available)
			continue;
		requests_sent++;
	}
	return requests_sent;
}

function find_name(arr, name)
{
	for(var i in arr)
		if(arr[i].name == name)
			return true;
	return false;
}

// Converts HH:MM:SS to seconds
function parseTimeOn(str)
{
	var arr = str.match(/(\d+)\:(\d+)\:(\d+)$/);
	if(!arr)
		return 0;
	return parseInt(arr[1]*60*60, 10) + parseInt(arr[2]*60, 10) + parseInt(arr[3], 10);
}

function parse_active_users(message, logon_callback, logoff_callback)
{
	if(!message)
		return false;
	var sys = sys_list[message.ip_address];
	if(!sys)
		return "Unknown system: " + message.ip_address;
	
	sys.last_response = time();
	var old_users = sys.users.slice();
	
	if(message.data[0] == '[') {
		try {
			sys.users = JSON.parse(message.data);
		} catch(e) {
			return e;
		}
	}
	else {
		var response = message.data.split("\r\n");
		
		// Skip header
		while(response.length && response[0].charAt(0)!='-')
			response.shift();
		if(response.length && response[0].charAt(0)=='-')
			response.shift();	// Delete the separator line
		while(response.length && !response[0].length)
			response.shift();	// Delete any blank lines
		while(response.length && !response[response.length-1].length)
			response.pop();		// Delete trailing blank lines
		
		sys.users = [];

		for(var i in response) {
			if(response[i]=="")
				continue;

			sys.users.push( { 
				name: format("%.25s",response[i]).trimRight(),
				action: response[i].substr(26, 31).trimLeft().trimRight(),
				timeon: parseTimeOn(response[i].substr(57, 9).trimLeft()),
				age: response[i].substr(67, 3).trimLeft(),
				sex: response[i].substr(71, 3).trimLeft(),
				node: response[i].substr(75, 4).trimLeft()
				} );
		}
	}
	if(logon_callback) {
		var count = 0;
		for(var i in sys.users) {
			if(!find_name(old_users, sys.users[i].name)) {
				logon_callback(sys.users[i], count ? null : sys);
				count++;
			}
		}
	}
	if(logoff_callback) {
		for(var i in old_users) {
			if(!find_name(sys.users, old_users[i].name))
				logoff_callback(old_users[i], sys);
		}
	}
			
	return true;
}

function receive_active_users()
{
	return sock.recvfrom(0x10000);
}

// Cancel listening if callback returns 'true'
function poll_systems(sent, interval, timeout, callback)
{
	var replies = 0;
	var begin = new Date();
	for(var loop = 0; replies < sent && new Date().valueOf()-begin.valueOf() < timeout; loop++)
	{
		if(callback && callback(loop))
			break;
		if(!sock.poll(interval))
			continue;

		var message = receive_active_users();
		if(message == null)
			continue;
		
		replies++;

		var result = parse_active_users(message);
		if(result !== true)
			return format("%s: %s", result, JSON.stringify(message));
	}
	return replies;
}

function dest_host(dest)
{
	var hp = dest.indexOf('@');
	if(hp < 0)
		return false;
	return hp;
}

// Returns true on success, string (error) on failure
function send_msg(dest, msg, from)
{
	var hp = dest_host(dest);
	if(!hp)
		return "Invalid destination";
	var host = dest.slice(hp+1);
	var destuser = dest.substr(0, hp);
	var sock = new Socket();
	if(!sock.connect(host, IPPORT_MSP))
		return "MSP Connection to " + host + " failed with error " + sock.last_error;
	var result = sock.send("B" + destuser + "\0" + /* Dest node +*/"\0" + msg + "\0" + from + "\0\0\0" + system.name + "\0");
	sock.close();
	if(result < 1)
		return "MSP Send to " + host + " failed with error " + sock.last_error;
	
	var userprops = load({}, "userprops.js")
	var addr_list = userprops.get(props_sent, "address", []);
	var addr_idx = addr_list.indexOf(dest);
	if(addr_idx >= 0)	 
		addr_list.splice(addr_idx, 1);
	addr_list.unshift(dest);
	if(addr_list.length > SentAddressHistoryLength)
		addr_list.length = SentAddressHistoryLength;
	userprops.set(props_sent, "address", addr_list);
	userprops.set(props_sent, "localtime", new Date().toString());
	return true;
}

// TEST CODE ONLY:
if(this.argc == 1 && argv[0] == "test") {
	
	this.read_sys_list();
	print(JSON.stringify(sys_list, null ,4));
	var sent = request_active_users();
	printf("Requests sent = %d\n", sent);

	var UDP_RESPONSE_TIMEOUT = 10000;

	function logon_callback(sys, user)
	{
		print("User " + user.name + " logged on to " + sys.name);
	}

	function poll_callback(loop)
	{
		printf("%c\r", "/-\\|"[loop%4]);
	}

	var replies = poll_systems(sent, 0.25, UDP_RESPONSE_TIMEOUT, poll_callback);

	printf("Replies received = %d\n", replies);

	print(JSON.stringify(sys_list, null, 4));
}

this;	// Must be last line