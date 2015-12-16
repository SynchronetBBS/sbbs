load('sbbsdefs.js');
load('websocket-proxy.js');

function err(msg) {
	log(LOG_DEBUG, msg);
	client.socket.close();
	exit();
}

function getSession(un) {
	var fn = format('%suser/%04d.web', system.data_dir, un);
	if (!file_exists(fn)) return false;
	var f = new File(fn);
	if (!f.open('r')) return false;
	var session = f.iniGetObject();
	f.close();
	return session;
}

// Obfuscated lazy port of an unfinished rlogin client I made quite some time
// ago. It does what it needs to do.  Unless it doesn't - in which case,
// replace it with something better.
var RLogin=function(e){var n=this;const t=24,i=13,s=17,o=19,c=10;var r=[],p=[],u={connected:!1,cooked:!0,suspendInput:!1,suspendOutput:!1,watchForClientEscape:!0,clientHasEscaped:!1},d={rows:24,columns:80,pixelsX:640,pixelsY:480,clientEscape:"~"},f={DOT:n.disconnect,EOT:n.disconnect,SUB:function(){u.suspendInput=u.suspendInput?!1:!0,u.suspendOutput=u.suspendInput},EOM:function(){u.suspendInput=u.suspendInput?!1:!0,u.suspendOutput=!1}};this.__defineGetter__("connected",function(){return u.connected}),this.__defineSetter__("connected",function(e){"boolean"!=typeof e||e||n.disconnect()}),this.__defineGetter__("rows",function(){return d.rows}),this.__defineSetter__("rows",function(e){if(!("number"==typeof e&&e>0))throw"RLogin: Invalid 'rows' setting "+e;d.rows=e}),this.__defineGetter__("columns",function(){return d.columns}),this.__defineSetter__("columns",function(e){if(!("number"==typeof e&&e>0))throw"RLogin: Invalid 'columns' setting "+e;d.columns=e}),this.__defineGetter__("pixelsX",function(){return d.pixelsX}),this.__defineSetter__("pixelsX",function(e){if(!("number"==typeof e&&e>0))throw"RLogin: Invalid 'pixelsX' setting "+e;d.pixelsX=e}),this.__defineGetter__("pixelsY",function(){return d.pixelsY}),this.__defineSetter__("pixelsY",function(e){if(!("number"==typeof e&&e>0))throw"RLogin: Invalid 'pixelsY' setting "+e;d.pixelsY=e}),this.__defineGetter__("clientEscape",function(){return d.clientEscape}),this.__defineSetter__("clientEscape",function(e){if("string"!=typeof e||1!=e.length)throw"RLogin: Invalid 'clientEscape' setting "+e;d.clientEscape=e});var a=new Socket,l=function(){if(!(a.nread<1)){for(var e=[];a.nread>0;)e.push(a.recvBin(1));if(!u.connected)if(0==e[0]){if(u.connected=!0,!(e.length>1))return;e=e.slice(1)}else n.disconnect();u.suspendOutput||(r=r.concat(e))}};this.send=function(e){if(!u.connected)throw"RLogin.send: not connected.";if(u.suspendInput)throw"RLogin.send: input has been suspended.";"string"==typeof e&&(e=e.split("").map(function(e){return ascii(e)}));for(var n=[],r=0;r<e.length;r++)u.watchForClientEscape&&e[r]==d.clientEscape.charCodeAt(0)?(u.watchForClientEscape=!1,u.clientHasEscaped=!0):u.clientHasEscaped?(u.clientHasEscaped=!1,"undefined"!=typeof f[e[r]]&&f[e[r]]()):!u.cooked||e[r]!=s&&e[r]!=o?((r>0&&e[r-1]==i&&e[r]==c||e[r]==t)&&(u.watchForClientEscape=!0),n.push(e[r])):u.suspendOutput==(e[r]==o);p=p.concat(n)},this.receive=function(){return r.splice(0,r.length)},this.addClientEscape=function(e,n){if("string"!=typeof e&&"number"!=typeof e||"string"==typeof e&&e.length>1||"function"!=typeof n)throw"RLogin.addClientEscape: invalid arguments.";f[e.charCodeAt(0)]=n},this.connect=function(){if("number"!=typeof e.port||"string"!=typeof e.host)throw"RLogin: invalid host or port argument.";if("string"!=typeof e.clientUsername)throw"RLogin: invalid clientUsername argument.";if("string"!=typeof e.serverUsername)throw"RLogin: invalid serverUsername argument.";if("string"!=typeof e.terminalType)throw"RLogin: invalid terminalType argument.";if("number"!=typeof e.terminalSpeed)throw"RLogin: invalid terminalSpeed argument.";if(!a.connect(e.host,e.port))throw"RLogin: Unable to connect to server.";for(a.sendBin(0,1),a.send(e.clientUsername),a.sendBin(0,1),a.send(e.serverUsername),a.sendBin(0,1),a.send(e.terminalType+"/"+e.terminalSpeed),a.sendBin(0,1);a.is_connected&&a.nread<1;)mswait(5);l()},this.cycle=function(){if(l(),!(u.suspendInput||p.length<1))for(;p.length>0;)a.sendBin(p.shift(),1)},this.disconnect=function(){a.close(),u.connected=!1}};

try {

	wss = new WebSocketProxy(client);

	if (typeof wss.headers['Cookie'] === 'undefined') {
		err('No cookie from WebSocket client.');
	}

	// Should probably search for the right cookie instead of assuming
	var cookie = wss.headers['Cookie'].split('=');
	if (cookie[0] !== 'synchronet' || cookie.length < 2) {
		err('Invalid cookie from WebSocket client.');
	}

	cookie = cookie[1].split(',');
	cookie[0] = parseInt(cookie[0]);
	if (cookie.length < 2 ||
		isNaN(cookie[0]) ||
		cookie[0] < 1 ||
		cookie[0] > system.lastuser
	) {
		err('Invalid cookie from WebSocket client.');
	}

	var usr = new User(cookie[0]);
	var session = getSession(usr.number);
	if (!session) {
		err('Unable to read web session file for user #' + usr.number);
	}
	if (cookie[1] != session.key) {
		err('Session key mismatch for user #' + usr.number);
	}
	if (typeof session.xtrn !== 'string' ||
		typeof xtrn_area.prog[session.xtrn] === 'undefined'
	) {
		err('Invalid external program code.');
	}

	var f = new File(file_cfgname(system.ctrl_dir, 'sbbs.ini'));
	if (!f.open('r')) err('Unable to open sbbs.ini.');
	var ini = f.iniGetObject('BBS');
	f.close();

	rlogin = new RLogin(
		{	host : system.inet_addr,
			port : ini.RLoginPort,
			clientUsername : usr.security.password,
			serverUsername : usr.alias,
			terminalType : 'xtrn=' + session.xtrn,
			terminalSpeed : 115200
		}
	);
	rlogin.connect();
	log(LOG_DEBUG, usr.alias + ' logged on via RLogin for ' + session.xtrn);

	while (client.socket.is_connected && rlogin.connected) {

		wss.cycle();
		rlogin.cycle();

		var send = rlogin.receive();
		if (send.length > 0) wss.send(send);

		while (wss.data_waiting) {
			var data = wss.receiveArray();
			rlogin.send(data);
		}

	}

} catch (err) {

	log(err);

} finally {
	rlogin.disconnect();
	client.socket.close();
}