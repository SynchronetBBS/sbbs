// Spy on a terminal server node using MQTT

// usage: mqtt_spy <node_num> [char_set]

require("key_defs.js", "CTRL_C");
require("nodedefs.js", "NodeStatus");

"use strict";

function get_ansi_seq()
{
	var seq = '';
	var key;

	while(key = ascii(console.getbyte(100))) {
		seq += key;
		if(key < '@' || key > '~')
			continue;
		switch(key) {
			case 'A':	// Up
			case 'B':	// Down
			case 'C':	// Right
			case 'D':	// Left
			case 'F':	// Preceding line
			case 'H':	// Home
			case 'K':	// End
			case 'V':	// PageUp
			case 'U':	// PageDn
			case '@':	// Insert
			case '~':	// Various VT-220
				// Pass-through these sequences to spied-upon node (eat all others)
				return KEY_ESC + '[' + seq;
		}
		return null;
	}
	return null;
}

var my_charset = js.global.console ? console.charset : argv[1] || 'CP437';
var node_charset;
function output(str)
{
	if(my_charset != node_charset) {
		if(node_charset == 'UTF-8')
			str = utf8_decode(str);
		else if(my_charset == 'UTF-8')
			str = utf8_encode(str);
	}
	if(js.global.console)
		write_raw(str);
	else
		write(str);
}

if(argc < 1) {
	alert("Node number not specified");
	exit(0);
}

var node_num = parseInt(argv[0], 10);
if(node_num < 1 || node_num > system.nodes || isNaN(node_num)
	|| (js.global.bbs != undefined && node_num == bbs.node_num)) {
	alert("Invalid Node Number: " + node_num);
	exit(0);
}
var in_topic = format("sbbs/%s/node/%u/input", system.qwk_id, node_num);
var out_topic = format("sbbs/%s/node/%u/output", system.qwk_id, node_num);
var status_topic = format("sbbs/%s/node/%u/status", system.qwk_id, node_num);
var terminal_topic = format("sbbs/%s/node/%u/terminal", system.qwk_id, node_num);
var mqtt = new MQTT();
if(!mqtt.connect()) {
	alert("Connect error: " + mqtt.error_str);
	exit(1);
}
if(!mqtt.subscribe(status_topic)) {
	alert(format("Subscribe to '%s' error: %s", status_topic, mqtt.error_str));
	exit(1);
}
var node_status = parseInt(mqtt.read(1000));
if(!mqtt.subscribe(out_topic)) {
	alert(format("Subscribe to '%s' error: %s", out_topic, mqtt.error_str));
	exit(1);
}
if(!mqtt.subscribe(terminal_topic)) {
	alert(format("Subscribe to '%s' error: %s", terminal_topic, mqtt.error_str));
	exit(1);
}
print("*** Synchronet MQTT Spy on Node " + node_num + ": Ctrl-C to Abort ***\r\n");

try {
	while(!js.terminated) {
		var msg = mqtt.read(/* timeout: */10, /* verbose: */true);
		if(msg) {
			if(msg.topic == out_topic)
				output(msg.data);
			else if(msg.topic == status_topic) {
				var new_status = parseInt(msg.data, 10);
				if(new_status != node_status) {
					node_status = new_status;
					var node_aux = msg.data.split('\t')[5];
					print("\r\nNew node status: "
						+ format(NodeStatus[node_status], node_aux));
				}
			}
			else if(msg.topic == terminal_topic) {
				node_charset = msg.data.split('\t')[4];
			}
		}
		if(js.global.console) {
			if(console.aborted || !bbs.online)
				break;
			console.line_counter = 0;
			while(bbs.online && !console.aborted) {
				var key = ascii(console.getbyte(10));
				if(!key)
					break;
				if(key == CTRL_C)
					console.aborted = true;
				if(key == KEY_ESC) {
					key = ascii(console.getbyte(500));
					if(!key)
						key = KEY_ESC;
					else if(key != '[')
						key = KEY_ESC + key;
					else
						key = get_ansi_seq();
				}
				if(key)
					mqtt.publish(in_topic, key);
			}
		}
	}
} catch(e) {
	print(e);
}
print();
print("Done spying");
