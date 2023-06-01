// A simple MQTT subscriber, for use with jsexec [-f]
// Command-line options more or less mimic 'mosquitto_sub'

"use strict";

const default_topic = "sbbs/#"
var mqtt = new MQTT;
var json = false;
var verbose = false;
var topic = [];
var count = 0;
var newline = true;
var options = {};

for(var i = 0; i < argc; ++i) {
	var arg = argv[i];
	while(arg[0] === '-')
		arg = arg.substring(1);
	switch(arg) {
		case 'C':
			count = parseInt(argv[++i], 10);
			break;
		case 'N':
			newline = false;
			break;
		case 'h':
			mqtt.broker_addr = argv[++i];
			break;
		case 'p':
			mqtt.broker_port = parseInt(argv[++i], 10);
			break;
		case 'q':
			mqtt.subscribe_qos = parseInt(argv[++i], 10);
			break;
		case 't':
			topic.push(argv[++i]);
			break;
		case 'v':
			verbose = true;
			break;
		case '?':
		case 'help':
			print("options:");
			print("  -h  MQTT broker hostname or IP address, default: " + mqtt.broker_addr);
			print("  -p  MQTT TCP port number to use, default: " + mqtt.broker_port);
			print("  -q  QoS value to use, default: " + mqtt.subscribe_qos);
			print("  -N  do not append newline to printed messages");
			print("  -t  Topic to subscribe to (can be repeated), default: " + default_topic);
			print("  -v  Use verbose output (include topic)");
			print("  -json Output in JSON format (add -pretty for prettier JSON)");
			exit(0);
		default:
			options[argv[i].slice(1)] = true;
			break;
	}
}

if(!mqtt.connect()) {
	alert(format("Error (%s) connecting to %s:%u", mqtt.error_str, mqtt.broker_addr, mqtt.broker_port));
	exit(1);
}
if(!topic.length)
	topic = [default_topic];
for(var i in topic) {
	if(!mqtt.subscribe(topic[i])) {
		alert(format("Error (%s) subscribing to %s", mqtt.error_str, topic[i]));
		exit(1);
	}
}
var msgs_received = 0;
var outfunc = newline ? print : write;
while(!js.terminated) {
	var msg = mqtt.read(1000, /* object retval: */true);
	if(msg === false) {
		continue;
	}
	if(options.json)
		outfunc(JSON.stringify(msg, null, options.pretty ? 4 : 0));
	else {
		if(verbose)
			printf("%s ", msg.topic);
		outfunc(msg.data);
	}
	++msgs_received;
	if(count && msgs_received >= count)
		break;
}
