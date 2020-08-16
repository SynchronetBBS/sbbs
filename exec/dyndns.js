// dyndns.js

// Client for Synchronet dynamic DNS service (yourbbs.synchro.net)

// $Id: dyndns.js,v 1.21 2019/08/28 21:00:41 rswindell Exp $

// usage: ?dyndns <password> [ip_address] [-mx address]

const REVISION = "$Revision: 1.21 $".split(' ')[1];
const rx_log_level = LOG_INFO;
const tx_log_level = LOG_DEBUG;

printf("Synchronet Dynamic DNS Client %s\r\n", REVISION);

host_list=["dyndns.synchro.net", "rob.synchro.net", "bbs.synchro.net", "cvs.synchro.net"];

var quiet = false;

function writeln(str)
{
	sock.send(str + "\r\n");
	if(!quiet)
		log(tx_log_level, "TX: " + str);
}

var options=load({}, "modopts.js", "dyndns");
if(!options)
	options = {};

var mx_record = options.mx;
var ip_address = options.ip;
var ip6_address = options.ip6;
var host_name = system.qwk_id;

for(i=1;i<argc;i++) {
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
			ip_address = argv[i];
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
				writeln(argv[0]);
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
