// A simple MQTT publisher, for use with jsexec
// Command-line options more or less mimic 'mosquitto_pub'

"use strict";

var mqtt = new MQTT;
var topic = '';
var msg;
var retain = false;
const bbs_prefix = "sbbs/" + system.qwk_id + "/";
const host_prefix = bbs_prefix + "host/" + system.local_host_name + "/";

function usage()
{
	print("options:");
	print("  -h  MQTT broker hostname or IP address, default: " + mqtt.broker_addr);
	print("  -p  MQTT TCP port number to use, default: " + mqtt.broker_port);
	print("  -q  QoS value to use, default: " + mqtt.publish_qos);
	print("  -r  Retain message");
	print("  -t  Topic to publish message to (required)");
	print("  -B  Prefix the topic with " + bbs_prefix);
	print("  -H  Prefix the topic with " + host_prefix);
	print("  -m  Message text to publish");
	print("  -n  send a null (zero length) message");
	exit(0);
}

for(var i = 0; i < argc; ++i) {
	var arg = argv[i];
	while(arg[0] === '-')
		arg = arg.substring(1);
	switch(arg) {
		case 'B':
			topic = bbs_prefix;
			break;
		case 'H':
			topic = host_prefix;
			break;
		case 'h':
			mqtt.broker_addr = argv[++i];
			break;
		case 'm':
			msg = argv[++i];
			break;
		case 'n':
			msg = '';
			break;
		case 'p':
			mqtt.broker_port = parseInt(argv[++i], 10);
			break;
		case 'q':
			mqtt.publish_qos = parseInt(argv[++i], 10);
			break;
		case 'r':
			retain = true;
			break;
		case 't':
			topic += argv[++i];
			break;
		case 'v':
			verbose = true;
			break;
		case '?':
		case 'help':
			usage();
		default:
			options[argv[i].slice(1)] = true;
			break;
	}
}
if(msg === undefined || !topic)
	usage();
if(!mqtt.connect()) {
	alert(format("Error (%s) connecting to %s:%u", mqtt.error_str, mqtt.broker_addr, mqtt.broker_port));
	exit(1);
}

if(!mqtt.publish(retain, topic, msg)) {
	alert(format("Error (%s) publishing to %s", mqtt.error_str, topic));
	exit(1);
}
