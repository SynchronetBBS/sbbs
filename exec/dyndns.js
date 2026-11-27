// dyndns.js

// Client for Synchronet dynamic DNS service (yourbbs.synchro.net)

// usage: ?dyndns [password] [ip_address] [[-option] | [-option value] [...]]
//
// When a password is set in the [dyndns] section of ctrl/modopts.ini, it
// overrides a password given as the first argument; only an argument that
// is a valid IPv4 address is then used as the IP address.

var REVISION = "2.1";
var rx_log_level = LOG_INFO;
var tx_log_level = LOG_DEBUG;

printf("Synchronet Dynamic DNS Client %s\r\n", REVISION);

host_list=["dyndns.synchro.net", "rob.synchro.net", "bbs.synchro.net", "cvs.synchro.net"];

var quiet = false;

function writeln(str)
{
	sock.send(str + "\r\n");
	if(!quiet)
		log(tx_log_level, "TX: " + str);
}

var options=load("modopts.js", "dyndns");
if(!options)
	options = {};

var mx_record = options.mx;
var ip_address = options.ip;
var ip6_address = options.ip6;
var host_name = system.qwk_id;
var password = options.password;
var positional = 0;

function valid_ipv4_address(str)
{
	var octet = str.split('.');
	if (octet.length != 4)
		return false;
	for (var o = 0; o < octet.length; o++) {
		if (!/^\d{1,3}$/.test(octet[o]) || parseInt(octet[o], 10) > 255)
			return false;
	}
	return true;
}

for(i=0;i<argc;i++) {
	switch (argv[i].toLowerCase()) {
		case "-q":
			quiet = true;
			break;
		case "-mx":
			mx_record = argv[++i];
			break;
		case "-hn":
			host_name = argv[++i];
			break;
		case "-ip6":
		case "-ipv6":
			ip6_address = argv[++i];
			break;
		default:
			++positional;
			if (positional == 1 && !options.password)
				password = argv[i];
			else if (valid_ipv4_address(argv[i]))
				ip_address = argv[i];
			else if (positional == 1)
				;	// Command-line password, overridden by modopts.ini
			else	// Don't log the argument itself: it may be a password
				alert("Ignoring unrecognized argument #" + (i + 1));
			break;
	}
}



for(h in host_list) {
	sock = new Socket();
	if( (this.server != undefined) &&
	    !sock.bind(0,server.interface_ip_address)) {
		printf("Error %lu binding socket to %s\r\n"
			,sock.last_error,server.interface_ip_address);
		continue;
	}
	if(!sock.connect(host_list[h],8467)) {
		sock.close();
		printf("Error %lu connecting to %s\r\n",sock.last_error,host_list[h]);
		continue;
	}
	var count=0;
	while(sock.is_connected && count++<10) {
		str=sock.readline();
		if(str == null)
			break;
		if(!quiet)
			log(rx_log_level, "RX: " + str);
		switch(str) {
			case "id?":
				writeln(host_name);
				break;
			case "pw?":
				writeln(password);
				break;
			case "ip?":
				if(ip_address)
					writeln(ip_address);
				else
					writeln("");
				break;
			case "ip6?":
				if(ip6_address)
					writeln(ip6_address);
				else
					writeln("");
				break;
			case "mx?":
				if(mx_record)
					writeln(mx_record);
				else
					writeln("");
				break;
			case "txt?":
				if(options.txt)
					writeln(options.txt);
				else
					writeln(system.name);
				break;
			case "loc?":
				if(options.loc)
					writeln(options.loc);
				else
					writeln("");
				break;
			case "wc?":
				if(options.wildcard)
					writeln(options.wildcard);
				else
					writeln("");
				break;
			case "ok":
				exit(0);
				break;
			default:
				alert("Unexpected message from server: " + str);
			case "ttl?":
				writeln("");
				break;
		}
	}
	break;
}
alert("Unexpected termination by server");
exit(1);
