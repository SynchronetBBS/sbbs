if (js.global.jsdoor_revision !== undefined)
	exit(0);

load('sbbsdefs.js');

function nettype(t)
{
	switch(Number(t)) {
		case NET_NONE: return "none";
		case NET_UNKNOWN: return "unknown";
		case NET_INTERNET: return "Internet";
		case NET_QWK: return "QWKnet";
		case NET_FIDO: return "FidoNet";
	}
	return format("???(%s)", t);
}

var test = [];
test[NET_NONE] = [
	null,
	undefined,
	"",
	" ",
	"-",
	" user@addr",
	"user @host.name",
	"user name@host.name",
];

test[NET_UNKNOWN] = [
	"@user",
	"user@addr ",
];

test[NET_INTERNET] = [
	"user@192.168.1.2",
	"user.name@1.2.3.4",
	"user@host.name",
	"user.name@host.name.tld",
];

test[NET_FIDO] = [
	"user@1",
	"user@1/2",
	"user@1:2/3",
	"user@1:2/3.4",
	"user name @ 1:2/3",
];

test[NET_QWK] = [
	"1@VERT",
	"digital man@VERT",
	"digital man @ VERT",
];


for(var t in test) {
	for(var i in test[t]) {
		var type = netaddr_type(test[t][i]);
		if(type != t)
			throw new Error(format("'%s'", test[t][i]) + ' = ' + nettype(type) + ' != ' + nettype(t));
	}
}
