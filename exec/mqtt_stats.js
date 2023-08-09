// Publish BBS statistics in either JSON or number-per-topic format

"use strict";

var mqtt = new MQTT;
var topic = "sbbs/" + system.qwk_id + "/stats";
var retain = true;

if(!mqtt.connect()) {
	alert(format("Error (%s) connecting to %s:%u"
		,mqtt.error_str, mqtt.broker_addr, mqtt.broker_port));
	exit(1);
}

if(argv.indexOf("-json") >= 0) {
	if(!mqtt.publish(retain, topic, JSON.stringify(system.stats))) {
		alert(format("Error (%s) publishing to %s", mqtt.error_str, topic));
		exit(1);
	}
} else {
	for(var p in system.stats) {
		if(!mqtt.publish(retain, topic + "/" + p, JSON.stringify(system.stats[p]))) {
			alert(format("Error (%s) publishing to %s", mqtt.error_str, topic));
			exit(1);
		}
	}
}
